#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <thread>
#include <vector>
#include <cmath>

using namespace std;

#define MAX_MSG 4096
#define PORT 6000

bool flagSaida = false, inChannel = false;
int socketCliente;
thread tEnviar, tReceber;

void tratarControlC(int signal);
void apagarTexto(int n);
void enviarMensagem(int socketCliente);
void receberMensagem(int socketCliente);
bool checkChannelName(char channel[]);

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

	string conexao;
	cout << "Digite /connect para se conectar ao servidor ou /quit para sair" << endl;
	getline(cin, conexao);

	if (conexao == "/quit" || cin.eof()) {
		cout << "Cliente encerrado!" << endl;
		flagSaida = true;
		close(socketCliente);
		return EXIT_SUCCESS;
	}

	if (conexao != "/connect") {
		cout << "Comando inválido, cliente encerrado!" << endl;
		flagSaida = true;
		close(socketCliente);
		return EXIT_SUCCESS;
	}

    // Conecta ao servidor
    if ((connect(socketCliente, (struct sockaddr *)&client, sizeof(struct sockaddr_in))) == -1) {
		cout << "[Error] Bind falhou, tente novamente!" << endl;
		return EXIT_FAILURE;
	}

    signal(SIGINT, tratarControlC); // Captura do ctrl + c

	char nome[MAX_MSG], channel[MAX_MSG];
	cout << "Digite seu nome: ";
	cin.getline(nome, MAX_MSG);
	send(socketCliente, nome, sizeof(nome), 0); //envia uma mensagem com o nome

    cout << "\n====== Conexão estabelecida ======   " << endl;

    while(!inChannel){
        cout << "Digite /join nomeCanal para entrar no canal especificado" << endl;

        getline(cin, conexao, ' ');
        cin.getline(channel, MAX_MSG);

        if (conexao != "/join") {
            cout << "Comando inválido, cliente encerrado!" << endl;
            flagSaida = true;
            close(socketCliente);
            return EXIT_SUCCESS;
        }

        if(checkChannelName(channel)){
            send(socketCliente, channel, sizeof(channel), 0); //envia uma mensagem com o nome do canal
            inChannel = true;
        }
        else
            cout << "\nNome de canal inválido \nNomes válidos de canais começam com '&' ou '#', tem tamanho máximo de 200 caracteres e não possui ' ', '^G' ou ','" << endl;
    }

	cout << "\n====== Bem vindo ao canal " << channel << " ======   " << endl;

	// Criação de duas threads, para realizar o recebimento e envio de msgs
	// Coloca as threads criadas em variáveis globais
	thread t1(enviarMensagem, socketCliente);
	thread t2(receberMensagem, socketCliente);

	tEnviar = move(t1);
	tReceber = move(t2);

	if (tEnviar.joinable()) tEnviar.join();
	if (tReceber.joinable()) tReceber.join();

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
    while(true) {
        cout << "Voce: ";
		string msg;
		getline(cin, msg, '\n');

		// Em caso de quit ou ctrl + d, encerra o chat
		if (msg == "/quit" || cin.eof()) {
			send(socketCliente, "/quit", 5, 0);
			flagSaida = true;
			// tReceber.detach();
			tEnviar.detach();
			close(socketCliente);
			return;
		}

        if(msg.substr(0, 5) == "/join"){
            int space = msg.find(" ");
            char channel[MAX_MSG];
            strcpy(channel, msg.substr(space + 1).c_str());

            if(checkChannelName(channel)){
                send(socketCliente, channel, sizeof(channel), 0); //envia uma mensagem com o nome do canal
                inChannel = true;
            }
            else
                cout << "\nNome de canal inválido \nNomes válidos de canais começam com '&' ou '#', tem tamanho máximo de 200 caracteres e não possui ' ', '^G' ou ','" << endl;
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
    while(true) {
        // caso nao devesse receber msg, cancela
		if (flagSaida) return;

		char nome[MAX_MSG], str[MAX_MSG];
        
        // Recebe o nome de quem enviou a msg, verificando o tamanho
		int bytes_received = recv(socketCliente, nome, sizeof(nome), 0);

		// Caso nao receba nada, retorna ao inicio do loop
		if (bytes_received <= 0) continue; 

        // Recebe a msg e trata ela
		recv(socketCliente, str, sizeof(str), 0);
		apagarTexto(6); // Apaga "Voce: "
        
        // imprime as msgs como suas ou do outro usuario 
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

bool checkChannelName(char channel[]) {
    int length = 0;
    while (channel[length] != '\0') {
        if (length >= 200) {
            return false;
        }
        length++;
    }

    if (length == 0 || (channel[0] != '&' && channel[0] != '#')) {
        return false;
    }

    for (int i = 0; i < length; ++i) {
        if (channel[i] == ' ' || channel[i] == '\a' || channel[i] == ',') {
            return false;
        }
    }

    return true;
}