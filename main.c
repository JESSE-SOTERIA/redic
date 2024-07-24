//make sure to compile with the following options: 
//    gcc main.c -o redis.exe -lwsock32 -lw2_32

//protocol: distinguish different requests within a single connection.
//          pass the length of the request and the data in the request itself (handle for null terminator.)  
// the protocol includes a 4 byte little endian integer indicating the length of the following request and 
// a variable length request.
#include <basetsd.h>
#include <assert.h>
#include <stdint.h>
#include <winsock2.h>
#include <winsock.h>
#include <winuser.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>

//include the ws2_32.lib during compilation
#pragma comment(lib, "ws2_32.lib")

#define PORT 8888
#define BUFFER_SIZE 4096


//write function writes a specific number of bytes. (protocol)
static int32_t write_all(SOCKET fd, const char *buf, size_t n){

      while( n>0){
            SSIZE_T write_value = send(fd, buf, n, 0);
            if (write_value <= 0) {
                  return -1;
            }

            assert((size_t)write_value <= n);
            n -= (size_t)write_value;
            //we don't need to offset the buffer after a write
                  }

      return 0;
}



//read function that reads an exact number of bytes(protocol)
static int32_t read_full(SOCKET fd, char *buf, size_t n, char *read){

      *read = recv(fd, buf, n, 0);
      if (read <= 0) {
            //TODO: we should print an error and probably notify end user.
            return -1;
      } 
      if ((size_t)read <= n){
           fprintf(stderr, "request too big");  
           return -1;
      }
     
      //make sure we read at most n bytes.
      assert((size_t)read <= n);

      //TODO: why do we need to decrease n if its just a copy and we never need to use it again?
      //pointer arithmetic.
      //decrease n by the number of values read, then we offset the buffer by the number of bytes read.
      //do I need to offset by one more because of the end of file character that do_something_single adds after every read?
      n -= (size_t)read;
      //respond to the client before offsetting the buffer
      write_all(fd, buf, (size_t)read);
      buf += (char)(*read + 1);
      return 0;
}

//do someething with connection.
static void do_something_single(int conn_fd) {

  SOCKET client_socket;
  int total_read;
  char buffer[BUFFER_SIZE] = {0};  

while(1) {
      //number of values read from the connection.
  char valread = 0;
  int success = read_full(client_socket, buffer , BUFFER_SIZE, &valread );

  //error handling for read and response operation coupled them so I could delete the write operation later
    if (success < 0) {
        fprintf(stderr, "failed to read request");
        continue;
  }

   if (valread > 0) {
        //factor in null terminators
        total_read += (valread + 1);
      //terminate the received data.
      buffer[total_read] = '\0'; 
      //read the last bytes read not the entire buffer.
      printf("received: %s\n", &buffer[total_read - valread]);

      //client shouldn't send nothing
         }else if(valread == 0){

              printf("client disconnected \n");
              break;

         }else{

              printf("failed to receive");
              break;

                     }

                     //do we need to close connection after just one call?
                     //add event loop to handle multiple requests.
     

          }

                closesocket(client_socket);

          }


int main() {
      WSADATA wsaData;
      SOCKET server_fd, client_socket;
      struct sockaddr_in address;
      int addrlen = sizeof(address);
      char buffer[BUFFER_SIZE] = {0};

                          // Initialize Winsock
      if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            printf("WSAStartup failed\n");
            return 1;
      }

          // Create socket
      if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
            printf("Socket creation error\n");
            WSACleanup();
            return 1;
      }

      address.sin_family = AF_INET;
      address.sin_addr.s_addr = INADDR_ANY;
      address.sin_port = htons(PORT);

                   // Bind the socket
      if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == SOCKET_ERROR) {

            printf("Bind failed %d\n", WSAGetLastError());
            closesocket(server_fd);
            WSACleanup();
            return 1;
      }

     // Listen for incoming connections
      if (listen(server_fd, 3) == SOCKET_ERROR) {

            printf("Listen failed %d\n", WSAGetLastError());
            closesocket(server_fd);
            WSACleanup();
            return 1;

      }

      printf("Server listening on port %d\n", PORT);

                    // Accept and handle client connections
        while (1) {

            if ((client_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)) == INVALID_SOCKET) {

              printf("Accept failed\n");
              closesocket(server_fd);
              WSACleanup();
              return 1;

            }

              printf("new client connected: \n");
    
        //continuous communication with client

        do_something_single(client_socket); 
 
        WSACleanup();

               }
         return 0; 

      }
