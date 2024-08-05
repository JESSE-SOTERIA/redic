#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8888
#define BUFFER_SIZE 1024
#define MAX_MESSAGE_LEN 4096

// Helper function to read exactly n bytes
static int read_all(int fd, char* buf, size_t n) {
    size_t bytes_read = 0;
    while (bytes_read < n) {
        ssize_t result = read(fd, buf + bytes_read, n - bytes_read);
        if (result <= 0) {
            printf("server disconnected");
            return -1;
        }
        bytes_read += result;
    }
    return 0;
}

// Helper function to write exactly n bytes
static int write_all(int fd, char* buf, size_t n) {
    size_t bytes_written = 0;
    while (bytes_written < n) {
        ssize_t result = write(fd, buf + bytes_written, n - bytes_written);
        if (result <= 0) return -1;  // Error or connection closed
        bytes_written += result;
    }
    return 0;
}

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    
    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }
   
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
       
    // Convert IPv4 and IPv6 addresses from text to binary form
    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }
   
    // Connect to server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }
    
    while (1) {
        printf("Enter message (or 'quit' to exit): ");
        char message[MAX_MESSAGE_LEN];
        fgets(message, sizeof(message), stdin);
        
        // Remove newline character
        message[strcspn(message, "\n")] = 0;
        
        if (strcmp(message, "quit") == 0) {
            break;
        }
        
        uint32_t length = strlen(message);
        char send_buffer[4 + MAX_MESSAGE_LEN];
        
        // Prepare the message with length prefix
        memcpy(send_buffer, &length, 4);
        memcpy(send_buffer + 4, message, length);
        
        // Send the message
        if (write_all(sock, send_buffer, 4 + length) < 0) {
            perror("Send failed");
            break;
        }
        
        // Read the response
        char recv_buffer[4 + MAX_MESSAGE_LEN];
        if (read_all(sock, recv_buffer, 4) < 0) {
            perror("Read length failed");
            break;
        }
        
        uint32_t recv_length;
        memcpy(&recv_length, recv_buffer, 4);
        
        if (read_all(sock, recv_buffer + 4, recv_length) < 0) {
            perror("Read message failed");
            break;
        }
        
        recv_buffer[4 + recv_length] = '\0';
        printf("Server echoed: %s\n", recv_buffer + 4);
    }
    
    close(sock);
    return 0;
}
