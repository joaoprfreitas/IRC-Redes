#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <signal.h>
#include <thread>

using namespace std;

#define MAX_MSG 200

bool flagSaida = false;
int socketCliente;
void tratarControlC(int signal);
int apagarTexto(int cnt);
void enviarMensagem(int socketCliente);
void receberMensagem(int socketCliente);
thread tEnviar, tReceber;

int main() {
    if ((socketCliente = socket(AF_INET,SOCK_STREAM,0)) == -1) {
		perror("socket: ");
		exit(-1);
	}
	
    struct sockaddr_in client;
	client.sin_family=AF_INET;
	client.sin_port=htons(9000); //Servidor na porta 9000 
	bzero(&client.sin_zero,0);

    // tenta conectar o socket cliente a um endereco se estiver em read mode
    if ((connect(socketCliente,(struct sockaddr *)&client,sizeof(struct sockaddr_in))) == -1){
		perror("connect: ");
		exit(-1);
	}

    signal(SIGINT, tratarControlC); //tratamento do control + c para parar o chat

    char nome[MAX_MSG];
	cout<<"Digite seu nome: ";
	cin.getline(nome, MAX_MSG);
    send(socketCliente, nome, sizeof(nome), 0); //envia uma mensagem com o nome

    cout<<"\n\t  ====== Bem vindo ao chat ======   "<<endl;

    // criacao de duas threads, para realizar o recebimento e envio de msgs, coloca na variavel global e se junta a elas se possivel
    thread t1(enviarMensagem, socketCliente);
	thread t2(receberMensagem, socketCliente);
    tEnviar=move(t1);
	tReceber=move(t2);
    if(tEnviar.joinable()) tEnviar.join();
	if(tReceber.joinable()) tReceber.join();

    return 0;
}

// leitura e envio de n mensagens ate receber o control+c
void enviarMensagem(int socketCliente) {
    while(1){
        cout << "Voce: ";
		char str[MAX_MSG];
		cin.getline(str,MAX_MSG);
		send(socketCliente, str, sizeof(str), 0);
        // recebe o control+c
		if(strcmp(str,"#exit")==0){
			flagSaida = true;
			tReceber.detach();	
			close(socketCliente);
			return;
		}
    }
}

// recebe a msg de outro usuario
void receberMensagem(int socketCliente) {
    while(1){
        // caso nao devesse receber msg, cancela
		if(flagSaida) return;
		char nome[MAX_MSG], str[MAX_MSG];
        
        // recebe o nome e confere se foi enviado um nome com tamanho nao nulo
		int bytes_received = recv(socketCliente, nome, sizeof(nome), 0);
		if(bytes_received<=0) continue; 

        // recebe a msg e trata ela
		recv(socketCliente, str, sizeof(str), 0);
		apagarTexto(6);
        
        // imprime as msgs como suas ou do outro usuario 
		if(strcmp(nome,"#NULL")!=0)
			cout << nome <<" : " << str <<endl;
		else
			cout << str << endl;
		cout << "Voce : ";
		fflush(stdout);
	}
}

void tratarControlC(int signal) {
    char str[MAX_MSG] = "#exit";
	send(socketCliente, str, sizeof(str), 0);
	flagSaida = true;
	tReceber.detach();
	tEnviar.detach();
	close(socketCliente);
	exit(signal);
}

//apaga n bytes
int apagarTexto(int cnt) {
    char tamanho = 8;
	for(int i = 0; i < cnt; i++){
		cout << tamanho;
	}
}