#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <vector>

using namespace std;

#define MAX_LEN 4096
#define PORT 6000

struct client {
	int id;
	string name;
	int socket;
	thread th;
};

int numClients = 0;
vector<client> clients;
mutex clients_mtx, cout_mtx;

void setClientName(int clientId, char name[]) {
    for (auto &client : clients) {
        if (client.id == clientId) {
            client.name = string(name);
        }
    }
}

void messageToUser(string msg, int clientId) {
    char tmp[MAX_LEN];
    strcpy(tmp, msg.c_str());

    for (auto &client : clients) {
        if (client.id == clientId) {
            send(client.socket, tmp, sizeof(tmp), 0);
        }
    }
}

void broadcast(string msg, int sender_id) {
    char tmp[MAX_LEN];
    strcpy(tmp, msg.c_str());

    for (auto &client : clients) {
        if (client.id != sender_id) {
            send(client.socket, tmp, sizeof(tmp), 0);
        }
    }
}

// void broadcast(int num, int sender_id) {
//     for (auto &client : clients) {
//         if (client.id != sender_id) {
//             send(client.socket, &num, sizeof(num), 0);
//         }
//     }
// }

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
    char name[MAX_LEN], str[MAX_LEN + 1];
    recv(client_socket, name, sizeof(name), 0);
    setClientName(id, name);

    string welcome_message = string(name) + string(" entrou no chat!");
    broadcast("#NULL", id);
    // broadcast(id, id);
    broadcast(welcome_message, id);
    shared_print(welcome_message);

    while (true) {
        memset(str, '\0', sizeof(str));
        int bytes_recv = recv(client_socket, str, sizeof(str), 0);
        if (bytes_recv <= 0) return;

        if (strcmp(str, "/quit") == 0) {
            string msg = string(name) + string(" saiu do chat!");
            broadcast("#NULL", id);
            // broadcast(id, id);
            broadcast(msg, id);
            shared_print(msg);
            end_connection(id);
            return;
        } else if (strcmp(str, "/ping") == 0) {
            cout << string(name) << " pingou o servidor!" << endl;
            messageToUser(string("server"), id);
            messageToUser(string("pong"), id);
        } else {
            broadcast(string(name), id);
            // broadcast(id, id);
            broadcast(string(str), id);
            shared_print(name + string(": ") + str);
        }
    }
}

int main() {
    int serverSocket;
    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        cout << "[Error] Socket" << endl;
        return EXIT_FAILURE;
    }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(6000);
    server.sin_addr.s_addr = INADDR_ANY;
    bzero(&server.sin_zero, 0);

    if ((bind(serverSocket, (struct sockaddr *)&server, sizeof(struct sockaddr_in))) == -1) {
        cout << "[Error] Bind" << endl;
        return EXIT_FAILURE;
    }

    if ((listen(serverSocket, 8)) == -1) {
        cout << "[Error] Listen" << endl;
        return EXIT_FAILURE;
    }
    
    int clientSocket;
    struct sockaddr_in client;
    unsigned int len = sizeof(sockaddr_in);

    cout << "==== Bem vindo ao chatroom ====" << endl;

    while (true) {
        if ((clientSocket = accept(serverSocket, (struct sockaddr *)&client, &len)) == -1) {
            cout << "[Error] Accept" << endl;
            return EXIT_FAILURE;
        }

        numClients++;
        thread t(handle_client, clientSocket, numClients);
        lock_guard<mutex> guard(clients_mtx);
        clients.push_back({numClients, string("Anonymous"), clientSocket, (move(t))});
    }

    for (auto &client : clients) {
        client.th.join();
    }

    close(serverSocket);

    return EXIT_SUCCESS;
}