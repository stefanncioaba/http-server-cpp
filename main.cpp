#include <iostream>
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <unistd.h>
#include "http_request.h"

#define PORT 8080 // the port users will be connecting to
#define BACKLOG 10 // how many pending connections queue holds
#define BUFFER_SIZE 1024 // size of the buffer for receiving data

int main() {
    int sockfd;

    // AF_INET : IPv4 protocol
    // SOCK_STREAM: TCP socket
    // 0: default protocol (TCP for SOCK_STREAM)
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        std::cerr << "Socket creation failed" << std::endl;
        return 1;
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT); // Port number
    address.sin_addr.s_addr = INADDR_ANY; // Bind to all available network interfaces
    // Bind the socket to the specified address and port
    int bind_result = bind(sockfd, (struct sockaddr*)&address, sizeof(address));
    
    if(bind_result < 0) {
        std::cerr << "Bind failed" << std::endl;
        return 1;
    }

    listen(sockfd, BACKLOG); // Listen for incoming connections

    // sockaddr_storage because the kernel writes to it
    struct sockaddr_storage their_addr;
    socklen_t addr_size = sizeof(their_addr);
    while (true) {
        // Accept a new connection (creates a new socket for the connection)
        int new_fd = accept(sockfd, (struct sockaddr*)&their_addr, &addr_size); // Accept a connection
        if (new_fd < 0) {
            std::cerr << "Accept failed" << std::endl;
            continue; // Continue to accept new connections
        }
        // Buffer to store the received message
        char buffer[BUFFER_SIZE];
        // Whole message received from the client
        ssize_t recv_bytes;
        std::string msg;
        
        // Keep receiving data until we find the end of the HTTP headers (indicated by \r\n\r\n)
        while (msg.find("\r\n\r\n") == std::string::npos) {
            ssize_t recv_bytes = recv(new_fd, buffer, BUFFER_SIZE, 0);
            if (recv_bytes <= 0) break;  // connection closed or error
            msg.append(buffer, recv_bytes);
        }
        
        HttpRequest request = parse_request(msg);

        if(request.method.empty()) {
            std::cerr << "Failed to parse HTTP request" << std::endl;
            close(new_fd); // Close the connection if parsing fails
            continue;
        }

        std::string response = request.version + " 200 OK\r\n\r\n";
        int send_bytes = send(new_fd, response.c_str(), response.size(), 0);

        close(new_fd); // Close the connection after sending the message
    }

    return 0;
     
}