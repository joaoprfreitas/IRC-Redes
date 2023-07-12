#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <vector>

using namespace std;

#define MAX_LEN 200

struct terminal {
	int id;
	string name;
	int socket;
	thread th;
};

int seed = 0;
vector<terminal> clients;
mutex clients_mtx, cout_mtx;

int main() {
    int server_socket;
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket error: ");
        exit(-1);
    }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(9000);
    server.sin_addr.s_addr = INADDR_ANY;
    bzero(&server.sin_zero, 0);

    if ((bind(server_socket, (struct sockaddr *)&server, sizeof(struct sockaddr_in))) == -1) {
        perror("bind error: ");
        exit(-1);
    }

    if ((listen(server_socket, 8)) == -1) {
        perror("listen error: ");
        exit(-1);
    }
    
    int client_socket;
    struct sockaddr_in client;
    unsigned int len = sizeof(sockaddr_in);

    cout << "==== Bem vindo ao chatroom ====" << endl;

    while (1) {
        if ((client_socket = accept(server_socket, (struct sockaddr *)&client, &len)) == -1) {
            perror("accept error: ");
            exit(-1);
        }

        seed++;
        thread t(handle_client, client_socket, seed);
        lock_guard<mutex> guard(clients_mtx);
        clients.push_back({seed, string("Anonymous"), client_socket, (move(t))});
    }

    close(server_socket);

    return EXIT_SUCCESS;
}

void set_name(int id, char name[]) {
    for (auto &client : clients) {
        if (client.id == id) {
            client.name = string(name);
        }
    }
}

int broadcast(string msg, int sender_id) {
    char tmp[MAX_LEN];
    strcpy(tmp, msg.c_str());

    for (auto &client : clients) {
        if (client.id != sender_id) {
            send(client.socket, tmp, sizeof(tmp), 0);
        }
    }
}

int broadcast(int num, int sender_id) {
    for (auto &client : clients) {
        if (client.id != sender_id) {
            send(client.socket, &num, sizeof(num), 0);
        }
    }
}

void shared_print(string str, bool endline = true) {
    lock_guard<mutex> guard(cout_mtx);
    cout << str;
    if (endline) cout << endl;
}

void end_connection(int id) {
    for (auto it = clients.begin(); it != clients.end(); it++) {
        if (it->id == id) {
            lock_guard<mutex> guard(clients_mtx);
            it->th.detach();
            clients.erase(it);
            close(it->socket);
            break;
        }
    }
}

void handle_client(int client_socket, int id) {
    char name[MAX_LEN], str[MAX_LEN];
    recv(client_socket, name, sizeof(name), 0);
    set_name(id, name);

    string welcome_message = string(name) + string(" entrou no chat!");
    broadcast("#NULL", id);
    broadcast(id, id);
    broadcast(welcome_message, id);
    shared_print(welcome_message);

    while (1) {
        int bytes_recv = recv(client_socket, str, sizeof(str), 0);
        if (bytes_recv <= 0) return;

        if (strcmp(str, "#exit") == 0) {
            string msg = string(name) + string(" saiu do chat!");
            broadcast("#NULL", id);
            broadcast(id, id);
            broadcast(msg, id);
            shared_print(msg);
            end_connection(id);
            return;
        }

        broadcast(string(name), id);
        broadcast(id, id);
        broadcast(string(str), id);
        shared_print(id + name + string(":") + str);
    }
}