#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#define PORT 8000

volatile sig_atomic_t wasSigHup = 0;

void sigHupHandler(int r) 
{
    wasSigHup = 1;
}

void signalHandling() {
    struct sigaction sa;
    sigaction(SIGHUP, NULL, &sa);
    sa.sa_handler = sigHupHandler;
    sa.sa_flags |= SA_RESTART;
    sigaction(SIGHUP, &sa, NULL);
}

int main() {
    int server = socket(AF_INET, SOCK_STREAM, 0);
    if(server == -1){
        perror("socket_failed");
        exit(EXIT_FAILURE);
    }
    signalHandling();

    sigset_t blockedMask, origMask;
    sigemptyset(&blockedMask);
    sigaddset(&blockedMask, SIGHUP);
    sigprocmask(SIG_BLOCK, &blockedMask, &origMask);

    struct sockaddr_in adr;
    memset(&adr, 0, sizeof(adr));
    adr.sin_family = AF_INET;
    adr.sin_addr.s_addr = INADDR_ANY;
    adr.sin_port = htons(PORT);

    if (bind(server, (struct sockaddr*)&adr, sizeof(adr)) == -1) {
        perror("bind_failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server, 1) == -1) {
        perror("listen_failed");
        exit(EXIT_FAILURE);
    }

    fd_set readfds;
    int maxFd = server;
    int clients[1];
    memset(clients, -1, sizeof(clients));

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server, &readfds);
        for (int i = 0; i < 1; i++) {
            if (clients[i] != -1) {
                FD_SET(clients[i], &readfds);
                maxFd = (maxFd > clients[i]) ? maxFd : clients[i];
            }
        }
        if (pselect(maxFd + 1, &readfds, NULL, NULL, NULL, &origMask) == -1) {
            if (errno == EINTR && wasSigHup) {
                wasSigHup = 0;
                printf("received SIGHUP\n");
                for (int i = 0; i < 1; i++) {
                    if (clients[i] != -1) {
                        close(clients[i]);
                        clients[i] = -1;
                    }
                }
            } else {
                perror("pselect_failed");
                exit(EXIT_FAILURE);
            }
        }
        if (FD_ISSET(server, &readfds)) {
            struct sockaddr_in clientAdr;
            socklen_t clientLen = sizeof(clientAdr);
            int newClientSocket = accept(server, (struct sockaddr*)&clientAdr, &clientLen);
            if (newClientSocket == -1) {
                perror("connection_failed");
            } else {
                printf("connection_from %s:%d\n", inet_ntoa(clientAdr.sin_addr), ntohs(clientAdr.sin_port));
                for (int i = 0; i < 1; i++) {
                    if (clients[i] != -1) {
                        close(clients[i]);
                        clients[i] = -1;
                    }
                }
                for (int i = 0; i < 1; i++) {
                    if (clients[i] == -1) {
                        clients[i] = newClientSocket;
                        break;
                    }
                }
            }
        }
        for (int i = 0; i < 1; i++) {
            if (clients[i] != -1 && FD_ISSET(clients[i], &readfds)) {
                char buffer[1024];
                ssize_t bytesRead = read(clients[i], buffer, sizeof(buffer));
                if (bytesRead > 0) {
                    printf("%zd bytes received from client\n", bytesRead);
                } else if (bytesRead == 0) {
                    printf("closed by client\n");
                    close(clients[i]);
                    clients[i] = -1;
                } else {
                    perror("reading_failed");
                }
            }
        }
    }

    close(server);
    for (int i = 0; i < 1; i++) {
        if (clients[i] != -1) {
            close(clients[i]);
        }
    }
    return 0;
}
