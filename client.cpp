#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <thread>
#include <vector>
#include <cmath>
#include <atomic>

using namespace std;

#define MAX_MSG 4096
#define PORT 6000
#define MAX_NAME 50

// bool flagSaida = false;
atomic<bool> flagSaida(false);
int socketCliente;
thread tEnviar, tReceber;

void tratarControlC(int signal);
void apagarTexto(int n);
void enviarMensagem(int socketCliente);
void receberMensagem(int socketCliente);
bool checkChannelName(string channel);

bool checkQuit(string msg) {
	if (msg == "/quit" || cin.eof()) {
		cout << "Cliente encerrado!" << endl;
		flagSaida = true;
		close(socketCliente);
		return true;
	}

	return false;
}

int main() {
	// Criação do socket
    if ((socketCliente = socket(AF_INET,SOCK_STREAM, 0)) == -1) {
		cout << "[Error] Erro na criação do socket, tente novamente!" << endl;
		return EXIT_FAILURE;
	}
	
    struct sockaddr_in client;
	client.sin_family = AF_INET;
	client.sin_port = htons(PORT);
	client.sin_addr.s_addr = INADDR_ANY;
	bzero(&client.sin_zero, 0);

	signal(SIGINT, tratarControlC); // Captura do ctrl + c

	// Conecta ao servidor
	if ((connect(socketCliente, (struct sockaddr *)&client, sizeof(struct sockaddr_in))) == -1) {
		cout << "[Error] Bind falhou, tente novamente!" << endl;
		return EXIT_FAILURE;
	}

	string name, channel;

	while (!flagSaida) {
		string conexao;
		cout << "Digite /connect para se conectar ao servidor ou /quit para sair" << endl;
		getline(cin, conexao);

		if (checkQuit(conexao)) return EXIT_SUCCESS;

		if (conexao == "/connect") {
			bool settedName = false;
			bool settedChannel = false;

			while (!settedName) {
				cout << "Utilize /nickname NOME para definir um nome para você no chat" << endl;
				string username;
				getline(cin, username);

				if (checkQuit(username)) return EXIT_SUCCESS;

				if (username.substr(0, 10) == "/nickname ") {
					name = username.substr(10, username.size() - 10);

					if (name.size() > MAX_NAME) {
						cout << "O nome deve ter no máximo 50 caracteres!" << endl;
						continue;
					}
					settedName = true;
				} else {
					cout << "Comando inválido, tente novamente!" << endl;
				}
			}

			while (!settedChannel) {
				cout << "Utilize /join CANAL para entrar em um canal" << endl;
				string command;
				getline(cin, command);

				if (checkQuit(command)) return EXIT_SUCCESS;

				if (command.substr(0, 6) == "/join ") {
					channel = command.substr(6, command.size() - 6);

					if (!checkChannelName(channel)) {
						cout << "Nome de canal não segue a RFC-1459!" << endl;
						continue;
					}

					settedChannel = true;
				} else {
					cout << "Comando inválido, tente novamente!" << endl;
				}
			}

			cout << "==== Bem vindo ao canal " << channel << " ====" << endl;

			size_t nameSize = name.size();
			send(socketCliente, &nameSize, sizeof(nameSize), 0); // Envia o tamanho do nome
			send(socketCliente, name.c_str(), nameSize, 0); // Envia o nome

			// Enviar canal
			size_t channelSize = channel.size();
			send(socketCliente, &channelSize, sizeof(channelSize), 0); // Envia o tamanho da string
			send(socketCliente, channel.c_str(), channelSize, 0); // Envia a string

			// Criação de duas threads, para realizar o recebimento e envio de msgs
			// Coloca as threads criadas em variáveis globais
			thread t1(enviarMensagem, socketCliente);
			thread t2(receberMensagem, socketCliente);

			tEnviar = move(t1);
			tReceber = move(t2);

			if (tEnviar.joinable()) tEnviar.join();
			if (tReceber.joinable()) tReceber.join();
		} else {
			cout << "Comando inválido, tente novamente!" << endl;
		}
	}

    return EXIT_SUCCESS;
}

// Realiza a quebra da mensagem em blocos de 4096 bytes
vector<char *> comporBlocos(string msg) {
	vector<char *> blocos;

	int msgSize = msg.size();
	int numBlocos = ceil(msgSize / (double) MAX_MSG);

	for (int i = 0; i < numBlocos; i++) {
		char *bloco = new char[MAX_MSG + 1]; // +1 para o caractere nulo

		bloco[0] = msg[i*MAX_MSG];

		int j = 1;
		while (msg[i * MAX_MSG + j] != '\n' && msg[i * MAX_MSG + j] != '\0' && j % MAX_MSG != 0) {
			bloco[j] = msg[i*MAX_MSG + j];
			j++;
		}

		bloco[j] = '\0';

		blocos.push_back(bloco);
	}

	return blocos;
}

// Envia as mensagens do cliente para o servidor por meio do socket
void enviarMensagem(int socketCliente) {
    while(!flagSaida) {
        cout << "Voce: ";
		string msg;
		getline(cin, msg, '\n');

		// Em caso de quit, ctrl + d ou canal encerrado pelo server, encerra o chat
		if (msg == "/quit" || cin.eof() || flagSaida) {
			send(socketCliente, "/quit", 5, 0);
			flagSaida = true;
			tEnviar.detach();
			tReceber.detach();
			close(socketCliente);
			return;
		}

		// Envia a mensagem em blocos de 4kb para o servidor
		vector<char *> blocos = comporBlocos(msg);

		for (char *bloco : blocos) {
			send(socketCliente, bloco, strlen(bloco), 0);
		}
    }
}

// Recebe as mensagens do servidor
void receberMensagem(int socketCliente) {
    while(!flagSaida) {
		char nome[MAX_MSG], str[MAX_MSG];
        
        // Recebe o nome de quem enviou a msg, verificando o tamanho
		int bytes_received = recv(socketCliente, nome, sizeof(nome), 0);

		// Caso nao receba nada, retorna ao inicio do loop
		if (bytes_received <= 0) continue; 

        // Recebe a msg e trata ela
		recv(socketCliente, str, sizeof(str), 0);
		apagarTexto(6); // Apaga "Voce: "

		// Canal foi encerrado pelo server
		if (strcmp(str, "#closeconnection") == 0) {
			flagSaida = true;
			return;
		}
        
        // Imprime apenas o texto se for #NULL, ou nome:texto se for diferente
		if (strcmp(nome, "#NULL") != 0)
			cout << nome << ": " << str <<endl;
		else
			cout << str << endl;

		cout << "Voce: ";
		fflush(stdout);
	}
}

// Trata o ctrl + c para sair do chat
void tratarControlC(int signal) {
	cout << "\nPara sair digite /quit\n";
}

// Apaga n bytes da tela
void apagarTexto(int n) {
    char del = 8;
	for (int i = 0; i < n; i++)
		cout << del;
}

bool checkChannelName(string channel) {
	if (channel.size() > 200 || channel.size() < 2)
		return false;

	if (channel[0] != '&' && channel[0] != '#')
		return false;

	for (int i = 0; i < channel.size(); i++)
		if (channel[i] == ' ' || channel[i] == '\a' || channel[i] == ',')
			return false;

    return true;
}