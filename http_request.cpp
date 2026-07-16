#include "http_request.h"
#include <iostream>
#include <regex>
#include <set>
#include <sstream>
#include <algorithm>
#include <cctype>

/*
METHOD /path HTTP/version\r\n
Header-Name: value\r\n
Header-Name: value\r\n
\r\n
[optional body]
*/

std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return ""; // string is all whitespace
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

//Implement the parse_request function to parse the raw HTTP request string and return an HttpRequest object
HttpRequest parse_request(const std::string &raw) {
    HttpRequest request;
    std::set<std::string> valid_methods = {"GET", "POST", "PUT", "DELETE", "HEAD", "OPTIONS", "PATCH"};
    std::istringstream request_stream(raw);

    std::string method, path, version;
    request_stream >> method >> path >> version;

    // Check if the method, path, and version are not empty
    if(method.empty() || path.empty() || version.empty()) {
        std::cerr << "Invalid HTTP request line" << std::endl;
        return request; // Return an empty HttpRequest object
    }
    // Check if the method is valid
    if(valid_methods.find(method) == valid_methods.end()) {
        std::cerr << "Invalid HTTP method: " << method << std::endl;
        return request; // Return an empty HttpRequest object
    }
    // Check if the path starts with a '/'
    if(path[0] != '/') {
        std::cerr << "Invalid HTTP path: " << path << std::endl;
        return request; // Return an empty HttpRequest object
    }
    // Check if the version is valid
    std:: string version_number = version.substr(5);
    static const std::regex pattern(R"(^\d+\.\d+$)");
    if(version.substr(0, 5) != "HTTP/" || !std::regex_match(version_number, pattern)) {
        std::cerr << "Invalid HTTP version: " << version << std::endl;
        return request; // Return an empty HttpRequest object
    }
    request.method = method;
    request.path = path;
    request.version = version;

    request_stream.ignore(2, '\n'); // Ignore the \r\n after the request line

    // Parse headers
    std::string header_line;
    while(std::getline(request_stream, header_line) && header_line != "\r") {
        if(!header_line.empty() && header_line.back() == '\r') {
            header_line.pop_back(); // Remove the trailing \r
        }
        size_t colon_pos = header_line.find(':');
        if(colon_pos != std::string::npos) {
            std::string header_name = header_line.substr(0, colon_pos);
            std::string header_value = header_line.substr(colon_pos + 1);
            // Trim whitespace from header name and value
            header_name = trim(header_name);
            header_value = trim(header_value);
            request.headers[to_lower(header_name)] = header_value;
        }
    }

    return request; // Return an HttpRequest object with parsed values WITHOUT BODY
}

