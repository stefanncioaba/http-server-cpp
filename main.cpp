#include <iostream>
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <unistd.h>
#include <thread>
#include <vector>
#include "http_request.h"
#include "http_response.h"
#include "connection.h"
#include <unordered_map>
#include <memory>
#include <fcntl.h>
#include <sys/epoll.h>
#include <unordered_map>
#include <cerrno>

#define PORT 8080 // the port users will be connecting to
#define BACKLOG 4096 // how many pending connections queue holds
#define BUFFER_SIZE 1024 // size of the buffer for receiving data



int set_nonblocking(int fd) {
    // Get the current flags of the file descriptor
    int flags = fcntl(fd, F_GETFL, 0);
    // Check if fcntl failed
    if (flags == -1) {
        std::cerr << "fcntl F_GETFL failed" << std::endl;
        return -1;
    }
    // Set the file descriptor to non-blocking mode by using bitwise OR to add the O_NONBLOCK flag
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        std::cerr << "fcntl F_SETFL failed" << std::endl;
        return -1;
    }
    return 0;
}

// Function to close a connection and clean up resources
void close_connection(int fd, std::unordered_map<int, Connection>& connections, int epoll_fd) {
    // Remove the file descriptor from the epoll instance and erase it from the connections map
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
    connections.erase(fd);
    close(fd);
}

// Helper for preparing the response 
void prepare_response(Connection& conn) {
    HttpResponse response = create_response(conn.request);

    // Build the response string from the HttpResponse object
    std::string response_string;
    response_string += response.version + " " + std::to_string(response.status_code) + " " + response.reason_phrase + "\r\n";
    for (const auto& header : response.headers) {
        response_string += header.first + ": " + header.second + "\r\n";
    }
    response_string += "\r\n";
    response_string += response.body;

    conn.write_buf = response_string;
    conn.write_offset = 0;

    // Check if the response has a "Connection" header to determine if we should keep the connection alive
    if (response.headers.find("connection") != response.headers.end()) {
        conn.keep_alive = (response.headers["connection"] == "keep-alive");
    } 
}

// Function to handle a client connection that is ready for reading or writing
void handle_client_ready(int fd, std::unordered_map<int, Connection>& connections, int epoll_fd) {
    Connection& conn = connections[fd];
    
    // If the connection is in the READING_HEADERS state, we read data from the socket and accumulate it in the buffer. 
    // We check for the end of the headers (indicated by \r\n\r\n) and update the connection state accordingly. 
    // If the connection is closed or an error occurs, we close the connection and clean up resources.
    if (conn.state == ConnectionState::READING_HEADERS) {
        // Buffer to store the received data
        char buf[BUFFER_SIZE];
        ssize_t n = recv(fd, buf, BUFFER_SIZE, 0);
        // If we received data, we append it to the connection's buffer and check for the end of the headers.
        if (n > 0) {
            conn.buffer.append(buf, n);

            size_t pos = conn.buffer.find("\r\n\r\n");
            // If we found the end of the headers ...
            if (pos != std::string::npos) {
                conn.header_end = pos + 4;
                
                conn.request = parse_request(conn.buffer);

                // If the request method is empty, it indicates a parsing failure, and we close the connection.
                if (conn.request.method.empty()) {
                    std::cerr << "Failed to parse HTTP request" << std::endl;
                    close_connection(fd, connections, epoll_fd);
                    return;
                }

                if (conn.request.method == "POST") {
                    auto it = conn.request.headers.find("content-length");
                    if (it != conn.request.headers.end()) {
                        // Read the content length from the headers and store it in the connection object
                        conn.content_length = std::stoul(it->second);
                    }

                    // Body bytes that already arrived alongside the headers, in the same buffer
                    conn.request.body = conn.buffer.substr(conn.header_end);

                    // If the body is fully received, we transition to the WRITING state; otherwise, we transition to READING_BODY.
                    if (conn.request.body.size() >= conn.content_length) {
                        conn.state = ConnectionState::WRITING;
                        prepare_response(conn);
                        struct epoll_event mod_ev;
                        mod_ev.events = EPOLLOUT;
                        mod_ev.data.fd = fd;
                        epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &mod_ev);
                    } else {
                        conn.state = ConnectionState::READING_BODY;
                    }
                } else { // For GET and other methods, we transition directly to the WRITING state and prepare the response.
                    conn.state = ConnectionState::WRITING;
                    prepare_response(conn);
                    struct epoll_event mod_ev;
                    mod_ev.events = EPOLLOUT;
                    mod_ev.data.fd = fd;
                    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &mod_ev);
                }
            }
        } else if (n == 0) { // Connection closed by the client
            close_connection(fd, connections, epoll_fd);
        } else { // Error occurred while reading
            if (errno != EAGAIN && errno != EWOULDBLOCK) { // If the error is not EAGAIN or EWOULDBLOCK, we close the connection
                close_connection(fd, connections, epoll_fd);
            }
        }   
      
    } else if (conn.state == ConnectionState::READING_BODY) {
        char buf[BUFFER_SIZE];
        ssize_t n = recv(fd, buf, BUFFER_SIZE, 0);

        if (n > 0) {
            conn.request.body.append(buf, n);
            // If the body is fully received, we transition to the WRITING state and prepare the response.
            if (conn.request.body.size() >= conn.content_length) {
                conn.state = ConnectionState::WRITING;
                prepare_response(conn);
                struct epoll_event mod_ev;
                mod_ev.events = EPOLLOUT;
                mod_ev.data.fd = fd;
                epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &mod_ev);
            }
        } else if (n == 0) {
            close_connection(fd, connections, epoll_fd);
        } else {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                close_connection(fd, connections, epoll_fd);
            }
        }
    } else if (conn.state == ConnectionState::WRITING) {
        // Send starting from the current write_offset in the write_buf and only send the remaining bytes
        ssize_t n = send(fd, conn.write_buf.c_str() + conn.write_offset,
                      conn.write_buf.size() - conn.write_offset, 0);

        if (n > 0) {
            // Update the write_offset to reflect how many bytes have been sent so far
            conn.write_offset += n;
            
            if (conn.write_offset >= conn.write_buf.size()) {
                // Full response sent
                if (conn.keep_alive) {
                    conn.state = ConnectionState::READING_HEADERS;
                    conn.buffer.clear();
                    conn.header_end = 0;
                    conn.content_length = 0;
                    conn.write_buf.clear();
                    conn.write_offset = 0;
                    conn.request = HttpRequest{};

                    struct epoll_event mod_ev;
                    mod_ev.events = EPOLLIN;
                    mod_ev.data.fd = fd;
                    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &mod_ev);
                } else {
                    close_connection(fd, connections, epoll_fd);
                }
            }
        } else if (n < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                close_connection(fd, connections, epoll_fd);
            }
        }
    }
}

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

    set_nonblocking(sockfd);

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

    // Create an epoll instance to monitor multiple file descriptors
    int epoll_fd = epoll_create1(0);
    if(epoll_fd < 0) {
        std::cerr << "Failed to create epoll file descriptor" << std::endl;
        return 1;
    }
    
    struct epoll_event ev;
    // Set the events to monitor for the listening socket (EPOLLIN for incoming connections)
    ev.events = EPOLLIN;    // Notify when the fd is ready for reading (incoming connections) 
    ev.data.fd = sockfd;    // Which fd is this

    // Add the listening socket to the set of file descriptors monitored by epoll and when it fires
    // gives back a copy of ev 
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sockfd, &ev) < 0) {
        std::cerr << "epoll_ctl ADD (listen socket) failed" << std::endl;
        return 1;
    }

    const int MAX_EVENTS = 64;
    // Create an array to hold the events that epoll will return when file descriptors are ready
    struct epoll_event events[MAX_EVENTS];
    std::unordered_map<int, Connection> connections;

    while (true) {
        // Wait for events on the monitored file descriptors (blocking call)
        // epoll_wait returns the number of file descriptors that are ready 
        int num_ready = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (num_ready < 0) {
            std::cerr << "epoll_wait failed" << std::endl;
            continue;
        }

        for (int i = 0; i < num_ready; i++) {
            if (events[i].data.fd == sockfd) {
                // Listening socket is ready, meaning at least one new connection is incoming
                while (true) {
                    // Storage for the address of the incoming connection
                    struct sockaddr_storage their_addr;
                    socklen_t addr_size = sizeof(their_addr);
                    // Accept the incoming connection and get a new socket file descriptor for it
                    int client_fd = accept(sockfd, (struct sockaddr*)&their_addr, &addr_size); 

                    if (client_fd < 0) {
                        break; // No more incoming connections to accept
                    }

                    // Set the new client socket to non-blocking mode (returns EAGAIN instead of blocking if no data is available)
                    set_nonblocking(client_fd); 

                    // Create an epoll_event structure for the new client socket
                    struct epoll_event client_ev;
                    client_ev.events = EPOLLIN;
                    client_ev.data.fd = client_fd;

                    // Add the new client socket to the epoll instance to monitor it for incoming data
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_ev) < 0) {
                        std::cerr << "epoll_ctl ADD (client) failed" << std::endl;
                        close(client_fd);
                    }
                    
                    // Create a new Connection object for the new client and store it in the connections map
                    Connection conn;
                    conn.fd = client_fd;
                    connections[client_fd] = conn;
                }
            } else {
                int client_fd = events[i].data.fd;
                // If the event indicates an error or hang-up on the client socket, we close the connection and clean up resources.
                if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                    close_connection(client_fd, connections, epoll_fd);
                    continue;
                }
                handle_client_ready(client_fd, connections, epoll_fd);
            }
        }
    }

    return 0;
     
}