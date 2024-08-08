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
#define MAX_MESSAGE_LEN 2048
#define MAX_CONNECTIONS 1024

enum event_type {
    EVENT_TYPE_ACCEPT,
    EVENT_TYPE_READ,
    EVENT_TYPE_WRITE
};

struct connection_info {
    int fd;
    char buffer[MAX_MESSAGE_LEN];
    int bytes_read;
};

struct io_uring ring;

void fatal_error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void add_accept_request(int server_fd, struct sockaddr_in *client_addr, socklen_t *client_len) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    if (!sqe) {
        fatal_error("io_uring_get_sqe for accept");
    }
    io_uring_prep_accept(sqe, server_fd, (struct sockaddr *)client_addr, client_len, 0);
    io_uring_sqe_set_data64(sqe, EVENT_TYPE_ACCEPT);
}

void add_read_request(struct connection_info *conn) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    if (!sqe) {
        fatal_error("io_uring_get_sqe for read");
    }
    io_uring_prep_recv(sqe, conn->fd, conn->buffer, MAX_MESSAGE_LEN, 0);
    io_uring_sqe_set_data64(sqe, EVENT_TYPE_READ);
    io_uring_sqe_set_data(sqe, conn);
}

void add_write_request(struct connection_info *conn) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    if (!sqe) {
        fatal_error("io_uring_get_sqe for write");
    }
    io_uring_prep_send(sqe, conn->fd, conn->buffer, conn->bytes_read, 0);
    io_uring_sqe_set_data64(sqe, EVENT_TYPE_WRITE);
    io_uring_sqe_set_data(sqe, conn);
}

int setup_listening_socket(int port) {
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

void handle_accept(struct io_uring *ring, int server_fd, struct sockaddr_in *client_addr, socklen_t *client_len) {
    struct connection_info *conn_info = malloc(sizeof(*conn_info));
    if (!conn_info) {
        fatal_error("malloc");
    }
    conn_info->fd = server_fd;
    add_read_request(conn_info);
    add_accept_request(server_fd, client_addr, client_len);
}

void handle_read(struct io_uring *ring, struct io_uring_cqe *cqe) {
    struct connection_info *conn_info = io_uring_cqe_get_data(cqe);
    if (cqe->res > 0) {
        conn_info->bytes_read = cqe->res;
        printf("Received %d bytes: %.*s\n", cqe->res, cqe->res, conn_info->buffer);
        add_write_request(conn_info);
    } else if (cqe->res == 0) {
        printf("Connection closed\n");
        close(conn_info->fd);
        free(conn_info);
    } else {
        fprintf(stderr, "Read error: %s\n", strerror(-cqe->res));
        close(conn_info->fd);
        free(conn_info);
    }
}

void handle_write(struct io_uring *ring, struct io_uring_cqe *cqe) {
    struct connection_info *conn_info = io_uring_cqe_get_data(cqe);
    if (cqe->res > 0) {
        printf("Sent %d bytes\n", cqe->res);
        add_read_request(conn_info);
    } else {
        fprintf(stderr, "Write error: %s\n", strerror(-cqe->res));
        close(conn_info->fd);
        free(conn_info);
    }
}

int main() {
    int server_fd;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    server_fd = setup_listening_socket(PORT);
    printf("Server listening on port %d\n", PORT);

    if (io_uring_queue_init(QUEUE_DEPTH, &ring, 0) < 0) {
        fatal_error("io_uring_queue_init");
    }

    add_accept_request(server_fd, &client_addr, &client_len);

    struct io_uring_cqe *cqe;
    while (1) {
        int ret = io_uring_submit(&ring);
        if (ret < 0) {
            fatal_error("io_uring_submit");
        }

        ret = io_uring_wait_cqe(&ring, &cqe);
        if (ret < 0) {
            fatal_error("io_uring_wait_cqe");
        }

        enum event_type event_type = (enum event_type)cqe->user_data;

        switch (event_type) {
            case EVENT_TYPE_ACCEPT:
                handle_accept(&ring, cqe->res, &client_addr, &client_len);
                break;
            case EVENT_TYPE_READ:
                handle_read(&ring, cqe);
                break;
            case EVENT_TYPE_WRITE:
                handle_write(&ring, cqe);
                break;
        }

        io_uring_cqe_seen(&ring, cqe);
    }

    io_uring_queue_exit(&ring);
    close(server_fd);

    return 0;
}
