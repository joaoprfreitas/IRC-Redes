#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <vector>
#include <algorithm>

using namespace std;

#define MAX_LEN 4096
#define PORT 6000

struct client {
	int id;
	string name;
	int socket;
	thread th;

    string channel;
    bool adm = false;
    bool mute = false;
};

int numClients = 0;
vector<client> clients;
mutex clients_mtx, cout_mtx;
vector<string> channels;

void encerraConexaoCliente(int id);
void messageToUser(string msg, int clientId);

// Atribui um nome a um cliente com base no seu id
void setClientName(int clientId, char name[]) {
    for (auto &client : clients) {
        if (client.id == clientId) {
            client.name = string(name);
            break;
        }
    }
}

bool channelExists(char channel[]) {
    for (auto &chan : channels)
        if (chan == string(channel))
            return true;

    return false;
}

void createChannel(int sender_id, char channel[]) {
    channels.push_back(channel);

    for (auto &client : clients) {
        if (client.id == sender_id) {
            client.channel = string(channel);
            client.adm = true;

            cout << client.name << " criou o canal " << client.channel << endl;

            break;
        }
    }
}

void encerraCanal(string channel) {
    auto it = find(channels.begin(), channels.end(), channel);

    for (auto &client : clients) {
        if (client.channel == channel && !client.adm) {
            messageToUser(string("server"), client.id);
            messageToUser(string("#closeconnection"), client.id);
        }
    }

    channels.erase(it);
}

// Define o canal ao qual o cliente está conectado
void setChannel(int clientId, char channel[]) {
    for (auto &client : clients) {
        if (client.id == clientId) {
            client.channel = string(channel);
            client.adm = false;
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
            if (it->adm == true) {
                string channel_closed_message = string("[AVISO] O canal ") + string(it->channel) + string(" foi encerrado, sua conexão será encerrada.\nPressione enter para sair.");
                broadcast(string("#NULL"), id);
                broadcast(channel_closed_message, id);
                printTerminal(string("[AVISO] O canal ") + string(it->channel) + string(" foi encerrado."));
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

bool isAdmin(int id, string channel) {
    for (auto &client : clients) {
        if (client.id == id && client.adm && client.channel == channel) {
            return true;
        }
    }

    return false;
}

int getClientId(string name) {
    for (auto &client : clients) {
        if (client.name == name) {
            return client.id;
        }
    }

    return -1;
}

void setMute(int id, string channel) {
    for (auto &client : clients) {
        if (client.id == id && client.channel == channel) {
            client.mute = true;
            break;
        }
    }
}

void setUnmute(int id, string channel) {
    for (auto &client : clients) {
        if (client.id == id && client.channel == channel) {
            client.mute = false;
            break;
        }
    }
}

bool isMuted(int id, string channel) {
    for (auto &client : clients) {
        if (client.id == id && client.channel == channel) {
            return client.mute;
        }
    }

    return false;
}


// Trata as mensagens recebidas por um cliente
void clientHandler(int client_socket, int id) {
    char name[MAX_LEN], str[MAX_LEN + 1], channel[MAX_LEN];

    // Recebe o tamanho do nome e o nome
    size_t nameSize;
    recv(client_socket, &nameSize, sizeof(nameSize), 0);
    vector<char> nameBuffer(nameSize + 1); // +1 para o caractere nulo
    recv(client_socket, nameBuffer.data(), nameSize, 0);
    nameBuffer[nameSize] = '\0'; // Adiciona o caractere nulo no final
    for (int i = 0; i < nameSize; i++) {
        name[i] = nameBuffer[i];
    }

    // Recebe o tamanho do nome do canal e o nome do canal
    size_t channelSize;
    recv(client_socket, &channelSize, sizeof(channelSize), 0);
    vector<char> channelBuffer(channelSize + 1); // +1 para o caractere nulo
    recv(client_socket, channelBuffer.data(), channelSize, 0);
    channelBuffer[channelSize] = '\0'; // Adiciona o caractere nulo no final
    for (int i = 0; i < channelSize; i++) {
        channel[i] = channelBuffer[i];
    }

    setClientName(id, name);
    
    if (channelExists(channel)) {
        setChannel(id, channel);
    } else{
        createChannel(id, channel);
    }

    // Informa a todos que um novo cliente entrou no chat
    string welcome_message = string(name) + string(" entrou no chat!");
    broadcast("#NULL", id);
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

        } else if (string cmd = string(str).substr(0, 6); cmd == "/kick ") { // Cliente envia kick
            if (!isAdmin(id, channel)) {
                string kickMessage = string("Você não tem permissão para kickar usuários!");
                messageToUser(string("#NULL"), id);
                messageToUser(kickMessage, id);
                continue;
            }
            string kickName = string(str + 6);

            if (kickName == name) {
                string kickMessage = string("[ERRO] Você não pode kickar a si mesmo!");
                messageToUser(string("#NULL"), id);
                messageToUser(kickMessage, id);
                continue;
            }

            int kickId = getClientId(kickName);
            if (kickId == -1) {
                string kickMessage = string("[ERRO] Usuário não encontrado!");
                messageToUser(string("#NULL"), id);
                messageToUser(kickMessage, id);
                continue;
            }

            string kickMessage = string("[AVISO] ") + string(name) + string(" kickou ") + kickName + string(" do canal ") + string(channel) + string("!");
            broadcast("#NULL", kickId);
            broadcast(kickMessage, kickId);
            printTerminal(kickMessage);

            messageToUser(string("#NULL"), kickId);
            messageToUser(string("[AVISO] Você foi removido do canal por um administrador, sua conexão foi encerrada.\nPressione ENTER para sair!"), kickId);

            messageToUser(string("#NULL"), kickId);
            messageToUser(string("#closeconnection"), kickId);

            encerraConexaoCliente(kickId);
        } else if (string cmd = string(str).substr(0, 6); cmd == "/mute ") { // Cliente envia mute
            if (!isAdmin(id, channel)) {
                string muteMessage = string("Você não tem permissão para mutar usuários!");
                messageToUser(string("#NULL"), id);
                messageToUser(muteMessage, id);
                continue;
            }
            string muteName = string(str + 6);

            if (muteName == name) {
                string muteMessage = string("[ERRO] Você não pode mutar a si mesmo!");
                messageToUser(string("#NULL"), id);
                messageToUser(muteMessage, id);
                continue;
            }

            int muteId = getClientId(muteName);
            if (muteId == -1) {
                string muteMessage = string("[ERRO] Usuário não encontrado!");
                messageToUser(string("#NULL"), id);
                messageToUser(muteMessage, id);
                continue;
            }

            if (isMuted(muteId, channel)) {
                string muteMessage = string("[ERRO] Usuário já está mutado!");
                messageToUser(string("#NULL"), id);
                messageToUser(muteMessage, id);
                continue;
            }

            string muteMessage = string("[AVISO] ") + string(name) + string(" mutou ") + muteName + string(" no canal ") + string(channel) + string("!");
            broadcast("#NULL", muteId);
            broadcast(muteMessage, muteId);
            printTerminal(muteMessage);

            messageToUser(string("#NULL"), muteId);
            messageToUser(string("[AVISO] Você foi mutado neste canal por um administrador!"), muteId);

            setMute(muteId, channel);
        } else if (string cmd = string(str).substr(0, 8); cmd == "/unmute ") { // Cliente envia unmute
            if (!isAdmin(id, channel)) {
                string unmuteMessage = string("Você não tem permissão para desmutar usuários!");
                messageToUser(string("#NULL"), id);
                messageToUser(unmuteMessage, id);
                continue;
            }
            string unmuteName = string(str + 8);

            if (unmuteName == name) {
                string unmuteMessage = string("[ERRO] Você não pode desmutar a si mesmo!");
                messageToUser(string("#NULL"), id);
                messageToUser(unmuteMessage, id);
                continue;
            }

            int muteId = getClientId(unmuteName);
            if (muteId == -1) {
                string unmuteMessage = string("[ERRO] Usuário não encontrado!");
                messageToUser(string("#NULL"), id);
                messageToUser(unmuteMessage, id);
                continue;
            }

            if (!isMuted(muteId, channel)) {
                string unmuteMessage = string("[ERRO] Usuário não está mutado!");
                messageToUser(string("#NULL"), id);
                messageToUser(unmuteMessage, id);
                continue;
            }

            string unmuteMessage = string("[AVISO] ") + string(name) + string(" desmutou ") + unmuteName + string(" no canal ") + string(channel) + string("!");
            broadcast("#NULL", muteId);
            broadcast(unmuteMessage, muteId);
            printTerminal(unmuteMessage);

            messageToUser(string("#NULL"), muteId);
            messageToUser(string("[AVISO] Você foi desmutado deste canal por um administrador!"), muteId);

            setUnmute(muteId, channel);
        } else { // Cliente envia uma mensagem
            // Verifica se o cliente está mutado
            if (isMuted(id, channel)) {
                string muteMessage = string("[ERRO] Você está mutado neste canal!");
                messageToUser(string("#NULL"), id);
                messageToUser(muteMessage, id);
                continue;
            }

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