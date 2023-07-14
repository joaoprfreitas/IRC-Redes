#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <vector>
#include <bits/stdc++.h>

using namespace std;

#define MAX_LEN 4096
#define PORT 6000

struct client {
	int id;
	string name;
	int socket;
	thread th;
    string channel;
    bool adm;
    bool mute;
};

int numClients = 0;
vector<client> clients;
mutex clients_mtx, cout_mtx;
vector<string> channels;

// Atribui um nome a um cliente com base no seu id
void setClientName(int clientId, char name[]) {
    for (auto &client : clients) {
        if (client.id == clientId) {
            client.name = string(name);
            break;
        }
    }
}

bool channelExists(char channel[]){
    
    return (std::find(channels.begin(),channels.end(), channel) != channels.end());
}

void createChannel(int sender_id, char channel[]){
    channels.push_back(channel);

    for (auto &client : clients) {
        if (client.id == sender_id) {
            client.adm = true;
            break;
        }
    }
}

void encerraCanal(string channel){
    auto it = std::find(channels.begin(),channels.end(), channel);

    channels.erase(it);
}

// Define o canal ao qual o cliente está conectado
void setChannel(int clientId, char channel[]) {
    for (auto &client : clients) {
        if (client.id == clientId) {
            client.channel = string(channel);
            break;
        }
    }
}

// Envia uma msg a um cliente específico
void messageToUser(string msg, int clientId) {
    char tmp[MAX_LEN];
    strcpy(tmp, msg.c_str());

    for (auto &client : clients) {
        if (client.id == clientId) {
            send(client.socket, tmp, sizeof(tmp), 0);
        }
    }
}

// Enviar uma msg a todos os clientes, exceto para quem a enviou
void broadcast(string msg, int sender_id) {
    char tmp[MAX_LEN];
    string channel;
    strcpy(tmp, msg.c_str());

    // Encontra o canal do sender
    for (auto &client : clients) {
        if (client.id == sender_id) {
            channel = client.channel;
            break;
        }
    }

    for (auto &client : clients) {
        if (client.id != sender_id && client.channel == channel) {
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

// Imprime uma msg no terminal do servidor, controlando race condition
void printTerminal(string str, bool endline = true) {
    lock_guard<mutex> guard(cout_mtx);
    cout << str;
    if (endline) cout << endl;
}

// Encerra a conexão de um cliente
void encerraConexaoCliente(int id) {
    for (auto it = clients.begin(); it != clients.end(); it++) {
        if (it->id == id) {
            if(it->adm = true){
                string channel_closed_message = string(it->channel) + string(" foi encerrado, selecione um novo canal utilizando o comando /join novamente");
                broadcast(channel_closed_message, id);
                encerraCanal(it->channel);
            }
            lock_guard<mutex> guard(clients_mtx);
            it->th.detach();
            clients.erase(it);
            close(it->socket);
            break;
        }
    }
}

// Trata as mensagens recebidas por um cliente
void clientHandler(int client_socket, int id) {
    char name[MAX_LEN], str[MAX_LEN + 1], channel[MAX_LEN];

    // Recebe o nome do cliente e altera no vetor de clientes
    recv(client_socket, name, sizeof(name), 0);
    setClientName(id, name);

    // Recebe o nome do canal
    recv(client_socket, channel, sizeof(channel), 0);
    if(channelExists)
        setChannel(id, channel);
    else{
        createChannel(id, channel);
    }

    // Informa a todos que um novo cliente entrou no chat
    string welcome_message = string(name) + string(" entrou no chat!");
    broadcast("#NULL", id);
    // broadcast(id, id);
    broadcast(welcome_message, id);
    printTerminal(welcome_message);

    while (true) {
        memset(str, '\0', sizeof(str));
        int bytes_recv = recv(client_socket, str, sizeof(str), 0);
        if (bytes_recv <= 0) return;
        
        // Se o cliente deseja sair do chat, encerra sua conexão
        if (strcmp(str, "/quit") == 0 || (int) str[0] == 0) {
            string msg = string(name) + string(" saiu do chat!");
            broadcast("#NULL", id);
            broadcast(msg, id);
            printTerminal(msg);
            encerraConexaoCliente(id);
            return;

        } else if (strcmp(str, "/ping") == 0) { // Cliente envia ping
            // Servidor responde pong
            cout << string(name) << " pingou o servidor!" << endl;
            messageToUser(string("server"), id);
            messageToUser(string("pong"), id);

        } else { // Cliente envia uma mensagem
            broadcast(string(name), id);
            broadcast(string(str), id);
            printTerminal(name + string(": ") + str);
        }
    }
}

int main() {
    int serverSocket;
    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        cout << "[Error] Erro na criação do socket, tente novamente!" << endl;
        return EXIT_FAILURE;
    }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(6000);
    server.sin_addr.s_addr = INADDR_ANY;
    bzero(&server.sin_zero, 0);

    if ((bind(serverSocket, (struct sockaddr *)&server, sizeof(struct sockaddr_in))) == -1) {
        cout << "[Error] Bind falhou, tente novamente!" << endl;
        return EXIT_FAILURE;
    }

    if ((listen(serverSocket, 8)) == -1) {
        cout << "[Error] Listener da porta falhou, tente novamente!" << endl;
        return EXIT_FAILURE;
    }
    
    int clientSocket;
    struct sockaddr_in client;
    unsigned int len = sizeof(sockaddr_in);

    cout << "==== Bem vindo ao chatroom ====" << endl;

    while (true) {
        if ((clientSocket = accept(serverSocket, (struct sockaddr *)&client, &len)) == -1) {
            cout << "[Error] Erro ao se conectar com cliente, tente novamente!" << endl;
            return EXIT_FAILURE;
        }

        numClients++; // id do proximo cliente
        thread t(clientHandler, clientSocket, numClients); // cria uma thread para tratar o cliente
        lock_guard<mutex> guard(clients_mtx); // cria um mutex para o cliente acessar o terminal
        clients.push_back({numClients, string("Anonymous"), clientSocket, (move(t)), string(""), false, false}); // adiciona o cliente ao vetor de clientes
    }
 
    // Espera todas as threads terminarem
    for (auto &client : clients) {
        client.th.join();
    }

    close(serverSocket);

    return EXIT_SUCCESS;
}