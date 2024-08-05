#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>

#define PORT 8888
#define BUFFER_SIZE 1024

const size_t max_message_len = 4096;

//TODO: make a function that calls read and write functions, has its own buffer that scopes the transacrtion.
// 2. make sure reads and writes are buffered such that only a single syscall is made.

//read n bytes from file descriptor into buffer.
static int32_t read_all(int fd, char* buf, size_t n) {
    //reads all bytes including header and message
    while (n > 0){
        ssize_t read_value = read(fd, buf, n);
        if (read_value <= 0) {
            //error or unexpected EOF
            return -1;
        }
        assert((size_t)read_value <= n);
        n -= (size_t)read_value;
        //necessary in case of partial reads.
        //ensures data is not overwritten (corrupted)
        buf += read_value;
    }
    //terminate the message
    //buf[n] = '\0';
    return 0;
}


//write n bytes to the file descriptor from the bufer.
//TODO: make sure the buffer points to the data and not the terminator
static int32_t write_all(int fd, char *buf, size_t n) {
    while (n > 0) {
        ssize_t written_value = write(fd, buf, n);
        if (written_value <=0) {
            return -1;
        }
        assert((size_t)written_value <= n);
        n-= (size_t)written_value;
        //necessary for partial writes
        //ensures data is not rewritten(corrupted)
        buf += written_value;
    }
    return 0;
}


//one fo three operations [READ, WRITE, CLOSE]
void close_active_socket(int fd) {
    close(fd);
}

//one fo three operations [READ, WRITE, CLOSE]
int write_socket(int fd, char *buf, int32_t n) {
    return write_all(fd, buf, (4 + n));
}

//works on a single client
//one of three operations [READ, WRITE, CLOSE]
int read_socket(int fd) {
    char read_buffer[4 + max_message_len + 1];
    //read first 4 bytes for the length of the message.
    int32_t err = read_all(fd, read_buffer, 4); 
    if(err < 0) {
        printf("failed to read protocol bytes");
        return err;
    }

    const char *error_message = "message too long";
    const char *read_error = "failed to read message";
    uint32_t len = 0;

    memcpy(&len, read_buffer, 4);
    //TODO: handle long messages better.
    //long messages dont need to kill connection.
    if (len > max_message_len) {
        int error_sent = send(fd, error_message, strlen(error_message), 0);
        if (error_sent == -1) {
            perror("failed to send error message.");
            return -1;
        }
        return -1;
    }

    //NOTE:we use &read_buffer[4] because the modification in read_all and write_all is of a copy of the pointer and not the actual pointer.
    err = read_all(fd, &read_buffer[4], len);
    if (err < 0) {
        int error_sent = send(fd, error_message, strlen(read_error), 0);
        if (error_sent == -1) {
            perror("failed to send error message.");
            return -1;
        }
        return -1;
    }
    
    //set the last element to '\0' nil termination
    read_buffer[4 + len] = '\0';
    printf("client says: %s\n", &read_buffer[4]);

    //NOTE: respond with the same protocol.

    return write_all(fd, read_buffer, 4 + len);


}




int main() {
	//file descriptors for client and server.
    int server_fd, client_fd;
	//adresses
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
	//buffer for comms
    char buffer[BUFFER_SIZE];

    // create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set up server address struct
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind socket to address
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    while (1) {
        // Accept incoming connection
        if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len)) < 0) {
            perror("Accept failed");
            continue;
        }

        printf("New connection accepted\n");

        //keep track of connection
        int result;
        do {

            //the [READ, WRITE, CLOSE] functions are chained for now for convinience.
            //TODO: unchain functions for event loop implementation.
           result = read_socket(client_fd);

        } while (result >= 0);

        //cleanup connection.
        close_active_socket(client_fd);
        printf("Connection closed\n");
        printf("waiting for new connection...");
    }

    //TODO:; close server with signal.
    close(server_fd);
    return 0;
}
