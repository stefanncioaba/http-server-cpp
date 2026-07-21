#include <string>
#include "http_request.h"

enum class ConnectionState {
    READING_HEADERS, // Still accumulating bytes, haven't seen \r\n\r\n yet
    READING_BODY, // Headers are done, this is a POST, still waiting for the full body 
    WRITING // Response is built, sending it out
};

struct Connection {
    // Stores the socket file descriptor of the connection (what accept returns)
    int fd;
    // The current state of the connection
    ConnectionState state = ConnectionState::READING_HEADERS;
    // The buffer that gets all the data from the socket
    std::string buffer;
    // Where the headers end and the body begins (if it exists)
    size_t header_end = 0;
    // The length of the body
    size_t content_length = 0;
    // The parsed HTTP request 
    HttpRequest request;
    // The outgoing response fully formed as a string, ready to be sent
    std::string write_buf;
    // How many bytes of write_buf have been sent so far
    size_t write_offset = 0;
    // Whether the connection should be kept alive or closed after the response
    bool keep_alive = true; // Default to keep-alive
};

