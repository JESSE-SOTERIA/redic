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
#include <stddef.h>
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

//NOTE: we have ring which all connections use.
struct Pool connection_pool;


typedef struct connection_info{
    int fd; 
    char buffer[MAX_MESSAGE_LEN];
    //to be set by our protocol
    int bytes_done;
    struct sockaddr_in addr;
    socklen_t addr_len;
    enum event_type state;
}Connection;



//error handling fucntion
void fatal_error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

//write_event
void add_write_event(struct connection_info *conn, struct io_uring *ring) {
    enum event_type state;
    state = READ_EVENT;

    //make an sqe
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    if (!sqe) {
        fatal_error("failed to get submission queue element for write event");
    }

    io_uring_prep_send(sqe, conn -> fd , conn -> buffer, conn -> bytes_done, 0);
    //change the state of the connection to read to prepare for data cominng from the client
    conn -> state = state;
    io_uring_sqe_set_data(sqe, conn);
}

//read_event
void add_read_event(struct connection_info *conn, struct io_uring *ring) {
    printf("adding  read event...");
    enum event_type state;
    state = WRITE_EVENT;
    //make an sqe
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    if (!sqe) {
        fatal_error("failed to get sqe for read");
    }

    //NOTE: you must say hello (or any oner five letter word really) to the server before you continue conversing.
    io_uring_prep_read(sqe, conn -> fd, conn -> buffer,  conn -> bytes_done, 0);
    conn -> state = state;
    io_uring_sqe_set_data(sqe, conn);
}

//should return a pointer to a Connection variable with all the connection for the related client set.
void add_accept_event(int server_fd, struct io_uring *new_ring, Pool *pool ){

    enum event_type state;
    state = READ_EVENT;

    Connection* new_connection = (Connection *)pool_alloc(pool); 

    //TODO: check if we can just write (new_connection || data)
    if (new_connection == NULL) {
        fatal_error("failed to allocate memory for new connection or client data.");
    }

    struct io_uring_sqe *sqe = io_uring_get_sqe(new_ring);
    if (!sqe) {
        fatal_error("failed to get sqe for read");
    }

    io_uring_prep_accept(sqe, server_fd, (struct sockaddr *)&new_connection -> addr , &new_connection -> addr_len, 0);
    new_connection -> state = state;
    io_uring_sqe_set_data(sqe, new_connection);

}


//function wrappers 
void handle_write(struct io_uring_cqe *cqe, Connection *conn, Pool *pool, struct io_uring *ring) {
    //check if the previous operation was successful.
    if (cqe->res > 0) {
        printf("queueing write...\n");
        add_write_event(conn, ring);
    } else {
        //NOTE: print read error because res is the success value of the previous operation.
        fatal_error("failed to write becaluse no data was read.");
        close(conn->fd);
        pool_free(pool, conn);
    }
}

void handle_read(struct io_uring *ring, struct io_uring_cqe *cqe, struct connection_info *conn, Pool *pool) {
    if (cqe -> res > 0) {
        printf("queueing read...\n");
        printf("cqe -> res %d\n", cqe->res);
        add_read_event(conn, ring);
    } else {
        fatal_error("failed to read beacause no data was written.");
        close(conn -> fd);
        pool_free(pool, conn);
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

    struct io_uring ring;
    //initialize io_uring struct
    int ret = io_uring_queue_init(32, &ring, 0);
    if (ret < 0) {
        fatal_error( "io_uring initialization failed\n");
    }
    int server_fd = set_up_listening_socket(PORT); 
    //array of individual connections
    Connection connections_buffer[MAX_CONNECTIONS];

    

    //connection pool, universal.
    pool_init(&connection_pool, connections_buffer, sizeof(connections_buffer), sizeof(Connection), _Alignof(Connection));

    add_accept_event(server_fd, &ring, &connection_pool);

    while (1) {
       io_uring_submit(&ring);
       struct io_uring_cqe *cqe;
        unsigned head;
        unsigned count = 0;

        io_uring_for_each_cqe(&ring, head, cqe) {
            count++;

            int res = cqe -> res;
            struct connection_info *user_data = (Connection *)io_uring_cqe_get_data(cqe);

            if (res < 0) {
                fatal_error("something bad happened while processing event.");
            } else {
                switch (user_data -> state) {
                    case READ_EVENT:
                        handle_read(&ring, cqe, user_data, &connection_pool);
                    break;
                    case WRITE_EVENT:
                        handle_write(cqe, user_data, &connection_pool, &ring);
                    break;
                    case ACCEPT_EVENT:
                        add_accept_event(server_fd, &ring, &connection_pool);
                    break;
                }
            }

            io_uring_cq_advance(&ring, count);
        }
    }

    io_uring_queue_exit(&ring);
    close(server_fd);

    return 0;
}
