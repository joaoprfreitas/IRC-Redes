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

bool flagSaida = false;
int socketCliente;
void tratarControlC(int signal);
void apagarTexto(int cnt);
void enviarMensagem(int socketCliente);
void receberMensagem(int socketCliente);
thread tEnviar, tReceber;

int main() {
    if ((socketCliente = socket(AF_INET,SOCK_STREAM, 0)) == -1) {
		cout << "[Error] Socket não pode ser criado, tente novamente!" << endl;
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

	if (conexao == "/quit") {
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

    // tenta conectar o socket cliente a um endereco se estiver em read mode
    if ((connect(socketCliente,(struct sockaddr *)&client,sizeof(struct sockaddr_in))) == -1) {
		cout << "[Error] Conexão com o servidor não estabelecida, tente novamente!" << endl;
		return EXIT_FAILURE;
	}

    signal(SIGINT, tratarControlC); //tratamento do control + c para parar o chat

	char nome[MAX_MSG];
	cout << "Digite seu nome: ";
	cin.getline(nome, MAX_MSG);
	send(socketCliente, nome, sizeof(nome), 0); //envia uma mensagem com o nome

	cout << "\n\t  ====== Bem vindo ao chat ======   " << endl;

	// criacao de duas threads, para realizar o recebimento e envio de msgs, coloca na variavel global e se junta a elas se possivel
	thread t1(enviarMensagem, socketCliente);
	thread t2(receberMensagem, socketCliente);
	tEnviar = move(t1);
	tReceber = move(t2);

	if (tEnviar.joinable()) tEnviar.join();
	if (tReceber.joinable()) tReceber.join();

    return EXIT_SUCCESS;
}

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

// leitura e envio de n mensagens ate receber o control+c
void enviarMensagem(int socketCliente) {
    while(true) {
        cout << "Voce: ";
		string msg;
		getline(cin, msg, '\n');

		vector<char *> blocos = comporBlocos(msg);

		for (char *bloco : blocos) {
			send(socketCliente, bloco, strlen(bloco), 0);
		}

        // recebe o control+c
		if (msg == "/quit") {
			flagSaida = true;
			tReceber.detach();	
			close(socketCliente);
			return;
		}
    }
}

// recebe a msg de outro usuario
void receberMensagem(int socketCliente) {
    while(true) {
        // caso nao devesse receber msg, cancela
		if(flagSaida) return;
		char nome[MAX_MSG], str[MAX_MSG];
        
        // recebe o nome e confere se foi enviado um nome com tamanho nao nulo
		int bytes_received = recv(socketCliente, nome, sizeof(nome), 0);
		if(bytes_received <= 0) continue; 

        // recebe a msg e trata ela
		recv(socketCliente, str, sizeof(str), 0);
		apagarTexto(6); //apaga "Voce: "
        
        // imprime as msgs como suas ou do outro usuario 
		if (strcmp(nome, "#NULL") != 0)
			cout << nome << ": " << str <<endl;
		else
			cout << str << endl;

		cout << "Voce: ";
		fflush(stdout);
	}
}

void tratarControlC(int signal) {
    char str[MAX_MSG] = "/quit";
	send(socketCliente, str, sizeof(str), 0);
	flagSaida = true;
	tEnviar.detach();
	tReceber.detach();
	close(socketCliente);
	exit(signal);
}

void apagarTexto(int cnt) {
    char tamanho = 8;
	for (int i = 0; i < cnt; i++) {
		cout << tamanho;
	}
}