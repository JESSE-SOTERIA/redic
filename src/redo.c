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
#include "conn_allocator.c"


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
//TODO: make a pool allocator(buddie) and have it allocate memory for ring and connection info.

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
    struct io_uring *ring;
}Connection;

Connection* connection_malloc(struct io_uring *io_uring) {
    Connection* new_connection = (Connection*)malloc(sizeof(Connection));
    if (new_connection == NULL) {
        fprintf(stderr, "Memory allocation failed for Connection\n");
        return NULL;
    }
    
    //default values.
    new_connection->fd = -1;  
    memset(new_connection->buffer, 0, MAX_MESSAGE_LEN);
    new_connection->bytes_done = 5;
    new_connection->state = READ_EVENT;
    new_connection->ring = io_uring;
    
    return new_connection;
}

void connection_free(Connection *conn) {
    if (conn != NULL) {
        free(conn);
    }
}

struct accept_data {
    struct sockaddr_in client_address;
    socklen_t client_len;
};

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
    //TODO: make it so that ther isn't a submit after every event prep.
    io_uring_submit(conn -> ring);
}

//read_event
void add_read_event(struct connection_info *conn) {
    //make an sqe
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    if (!sqe) {
        fatal_error("failed to get sqe for read");
    }

    //NOTE: you must say hello (or any oner five letter word really) to the server before you continue conversing.
    io_uring_prep_read(sqe, conn -> fd, conn -> buffer,  conn -> bytes_done, 0);
    io_uring_sqe_set_data64(sqe, WRITE_EVENT);
    io_uring_sqe_set_data(sqe, conn);
    //TODO: make it so that ther isn't a submit after every event prep.
    io_uring_submit(conn -> ring);
}

//should return a pointer to a Connection variable with all the connection for the related client set.
Connection *add_accept_event(int server_fd, struct io_uring *new_ring ){

    Connection* new_connection = connection_malloc(new_ring);
    struct accept_data *data = malloc(sizeof(*data));
    data -> client_len = sizeof(data -> client_address);

    if (new_connection || data == NULL) {
        fatal_error("failed to allocate memory for new connection or client data.");
    }

    //TODO: make these changes inthe completion entry.
    //new_connection -> addr = data -> client_address;
    //new_connection ->addr_len = data -> client_len; 
    struct io_uring_sqe *sqe = io_uring_get_sqe(new_ring);

    if (!sqe) {
        fatal_error("failed to get sqe for read");
    }

    io_uring_prep_accept(sqe, server_fd, (struct sockaddr *)&data -> client_address, &data ->client_len, 0);
    //TODO: the read event enum value can only be accessed from the function io_uring_get_data64. 
    io_uring_sqe_set_data64(sqe, READ_EVENT);
    io_uring_submit(new_connection -> ring);

    return new_connection;
}

//function wrappers 
void handle_write(struct io_uring_cqe *cqe, Connection *conn) {
    //check if the previous operation was successful.
    if (cqe->res > 0) {
        printf("queueing write...\n");
        add_write_event(conn);
    } else {
        //NOTE: print read error because res is the success value of the previous operation.
        fprintf(stderr, "Read error: %s\n", strerror(-cqe->res));
        close(conn->fd);
        connection_free(conn);
    }
}

void handle_read(struct io_uring *ring, struct io_uring_cqe *cqe, Connection *conn) {
    if (cqe -> res > 0) {
        printf("queueing read...\n");
        add_read_event(conn);
    } else {
        fprintf(stderr, "Write error: %s\n", strerror(-cqe->res));
        close(conn -> fd);
        connection_free(conn);
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
    int server_fd = set_up_listening_socket(PORT); 
    // TODO: implement our allocator to allocato to this buffer.
    Connection connections[MAX_CONNECTIONS];
    struct io_uring rings[MAX_CONNECTIONS];

    while (1) {
        struct io_uring new_ring = ring_malloc();
        add_accept_event(server_fd, struct io_uring *new_ring);
        
        
    }

    io_uring_queue_exit(&ring);
    close(server_fd);

    return 0;
}
