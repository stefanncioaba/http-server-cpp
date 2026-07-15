#include <iostream>
#include <sys/socket.h> 
#include <netinet/in.h> 

#define PORT 8080 // the port users will be connecting to
#define BACKLOG 10 // how many pending connections queue holds

int main() {
    int sockfd, new_fd;

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
        new_fd = accept(sockfd, (struct sockaddr*)&their_addr, &addr_size); // Accept a connection
        if (new_fd < 0) {
            std::cerr << "Accept failed" << std::endl;
            continue; // Continue to accept new connections
        }
        std::cout << "Connection accepted! fd = " << new_fd << std::endl;

    }

    return 0;
     
}