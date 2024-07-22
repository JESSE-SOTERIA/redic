//make sure to compile with the following options: 
//    gcc main.c -o redis.exe -lwsock32 -lw2_32

#include <winsock2.h>
#include <winsock.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>

#pragma comment(lib, "ws2_32.lib")

#define PORT 8888
#define BUFFER_SIZE 1024

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

              printf("new client connected:");

       /* int valread = recv(client_socket, buffer, BUFFER_SIZE, 0);
        printf("Received: %s\n", buffer);

        const char *response = "Hello from server";
        send(client_socket, response, strlen(response), 0);

        closesocket(client_socket);
        }

      closesocket(server_fd);
        WSACleanup(); */


        //continuous communication with client

        while (1) {
          int valread = recv(client_socket, buffer , BUFFER_SIZE , 0 );
          if (valread > 0) {
            buffer[valread] = '\0'; //terminate the received data.
            printf("received: %s\n", buffer);

            //echo back value received
            send(client_socket, buffer, BUFFER_SIZE, 0);
                     }else if(valread == 0){
                       printf("client disconnected");
                       break;
                     }else{
                       printf("failed to receive");
                       break;
                     }
                     closesocket(client_socket);

        }

        closesocket(server_fd);
        WSACleanup();

               }
         return 0; 

      }
        
        
        
        
        
        


