#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8888
#define MAX_MESSAGE_LEN 2048

void fatal_error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char message[MAX_MESSAGE_LEN] = {0};
    char buffer[MAX_MESSAGE_LEN] = {0};

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fatal_error("Socket creation error");
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        fatal_error("Invalid address/ Address not supported");
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        fatal_error("Connection Failed");
    }

    printf("Connected to server. Enter a message (or 'quit' to exit):\n");

    while (1) {
        fgets(message, MAX_MESSAGE_LEN, stdin);

        message[strcspn(message, "\n")] = 0;

        if (strcmp(message, "quit") == 0) {
            break;
        }

        send(sock, message, strlen(message), 0);
        printf("Message sent to server\n");

        int valread = read(sock, buffer, MAX_MESSAGE_LEN);
        if (valread <= 0) {
            printf("failed to read from server");
            exit(EXIT_FAILURE);
        }
        printf("Server response: %s\n", buffer);

        memset(buffer, 0, MAX_MESSAGE_LEN);
    }

    close(sock);
    printf("Connection closed\n");

    return 0;
}
