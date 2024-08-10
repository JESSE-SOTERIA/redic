//make sure to compile with the -luring flag.
#define _GNU_SOURCE
#include <liburing.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>


#define PORT 8888
#define QUEUE_DEPTH 256
#define BACKLOG 10
#define MAX_MESSAGE_LEN 512
#define GLOBAL_BUFFER_SIZE 2048
#define MAX_CONNECTIONS 1024
#define MAX_EVENTS 56

//controll the operations to do on sockets and file descriptors
enum event_type {
    WRITE_EVENT,
    READ_EVENT,
    ACCEPT_EVENT,
};

//we need to manage the connections in pools and have functions to free connection data.
struct io_uring ring;

typedef struct connection_info{
    int fd; 
    char buffer[MAX_MESSAGE_LEN];
    //to be set by our protocol
    int bytes_done;
    struct sockaddr_in addr;
    socklen_t addr_len;
    enum event_type state;
}Connection;

Connection* connection_malloc() {
    Connection* new_connection = (Connection*)malloc(sizeof(Connection));
    
    if (new_connection == NULL) {
        fprintf(stderr, "Memory allocation failed for Connection\n");
        return NULL;
    }
    
    //default values.
    new_connection->fd = -1;  
    memset(new_connection->buffer, 0, MAX_MESSAGE_LEN);
    new_connection->bytes_done = 0;
    new_connection->state = READ_EVENT;
    
    return new_connection;
}

void connection_free(Connection *conn) {
    if (conn != NULL) {
        free(conn);
    }
}

//I can make this call back do anything.
//will decide what makes most sense.
typedef int (*callback_function)(struct connection_info *connection); 

//error handling fucntion
void fatal_error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

//write_event
void add_write_event(struct connection_info *conn) {
    //make an sqe
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    if (!sqe) {
        fatal_error("failed to get submission queue element");
    }

    io_uring_prep_send(sqe, conn -> fd , conn -> buffer, conn -> bytes_done, 0);
    io_uring_sqe_set_data64(sqe, READ_EVENT);
    io_uring_sqe_set_data(sqe, conn);
}

//read_event
void add_read_event(struct connection_info *conn) {
    //make an sqe
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    if (!sqe) {
        fatal_error("failed to get sqe for read");
    }

    io_uring_prep_read(sqe, conn -> fd, conn -> buffer,  conn -> bytes_done, 0);
    io_uring_sqe_set_data64(sqe, WRITE_EVENT);
    io_uring_sqe_set_data(sqe, conn);
}

//accept_event
void add_accept_event(int server_fd, struct sockaddr_in *client_addr, socklen_t *client_len) {
    //make an sqe
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    if (!sqe) {
        fatal_error("failed to get sqe for read");
    }
    io_uring_prep_accept(sqe, server_fd, (struct sockaddr *)client_addr, client_len, 0);
    io_uring_sqe_set_data64(sqe, READ_EVENT);
}

//function wrappers 
void handle_write(struct io_uring *ring, struct io_uring_cqe *cqe, Connection *conn) {
    //check if the previous operation was successful.
    if (cqe->res > 0) {
        printf("Received %d bytes\n", cqe->res);
        add_write_event(conn);
    } else {
        //NOTE: print read error because res is the success value of the previous operation.
        fprintf(stderr, "Read error: %s\n", strerror(-cqe->res));
        close(conn->fd);
    }
}

void handle_read(struct io_uring *ring, struct io_uring_cqe *cqe, Connection *conn) {
    if (cqe -> res > 0) {
        printf("written %d bytes\n", cqe -> res);
        add_read_event(conn);
    } else {
        fprintf(stderr, "Write error: %s\n", strerror(-cqe->res));
        close(conn -> fd);
    }
}

int set_up_listening_socket(int port){
    int server_fd;
    struct sockaddr_in server_addr;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        fatal_error("Socket creation failed");
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        fatal_error("Bind failed");
    }

    if (listen(server_fd, BACKLOG) < 0) {
        fatal_error("Listen failed");
    }

    return server_fd;
}

int main() {
    // ... (previous initialization code) ...

    int server_fd = set_up_listening_socket(PORT); 
    struct io_uring_cqe *cqe;
    struct io_uring_sqe *sqe;
    Connection *conn;
    int pending_accepts = 0;

    // Submit initial accept event
    add_accept_event(server_fd, NULL, NULL);
    pending_accepts++;

    while (1) {
        io_uring_submit(&ring);

        // Process completions
        unsigned head;
        unsigned completed = 0;
        io_uring_for_each_cqe(&ring, head, cqe) {
            completed++;

            enum event_type event_type = (enum event_type)io_uring_cqe_get_data64(cqe);
            conn = (Connection *)io_uring_cqe_get_data(cqe);

            switch (event_type) {
                case READ_EVENT:
                    handle_read(&ring, cqe, conn);
                    break;
                case WRITE_EVENT:
                    handle_write(&ring, cqe, conn);
                    break;
                case ACCEPT_EVENT:
                    if (cqe->res >= 0) {
                        int client_fd = cqe->res;
                        Connection *new_conn = connection_malloc();
                        if (new_conn) {
                            new_conn->fd = client_fd;
                            new_conn->addr_len = sizeof(new_conn->addr);
                            getpeername(client_fd, (struct sockaddr *)&new_conn->addr, &new_conn->addr_len);
                            add_read_event(new_conn);
                        } else {
                            close(client_fd);
                        }
                    }
                    // Submit a new accept event
                    add_accept_event(server_fd, NULL, NULL);
                    break;
            }
        }

        io_uring_cq_advance(&ring, completed);

        // Optionally, add more accept events if needed
        while (pending_accepts < MAX_EVENTS / 2) {
            add_accept_event(server_fd, NULL, NULL);
            pending_accepts++;
        }
    }

    io_uring_queue_exit(&ring);
    close(server_fd);

    return 0;
}
