# IRC-Redes

## Membros
Gabriel Akio Urakawa - 11795912\
João Pedro Rodrigues Freitas - 11316552\
Samuel Victorio Bernacci - 12703455

Link para o video: https://drive.google.com/file/u/1/d/1WGTvAS1IM0nkfCv9hTOqvdbi6ITAU_uX/view?usp=sharing

## Descrição do cenário testado
Versão do Kernel: 5.15.90.1-microsoft-standard-WSL2\
Versão do Linux: Ubuntu 22.04 LTS\
Compilador: gcc

## Como compilar e executar:
Caso não possua, instalar a biblioteca pthread:
```
sudo apt-get install libpthread-stubs0-dev
```

Para compilar:
```
make
```

Para executar o servidor:
```
./server
```

Para executar um cliente:
```
./client
```