#include "http_response.h"
#include <iostream>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

bool is_safe_path(const std::string& path) {
    return path.find("..") == std::string::npos;
}

// Function to guess the content type based on the file extension
std::string guess_content_type(const fs::path& path) {
    std::string ext = path.extension().string();

    if (ext == ".html" || ext == ".htm") return "text/html";
    if (ext == ".css")  return "text/css";
    if (ext == ".js")   return "application/javascript";
    if (ext == ".json") return "application/json";
    if (ext == ".txt")  return "text/plain";
    if (ext == ".png")  return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif")  return "image/gif";
    if (ext == ".svg")  return "image/svg+xml";
    if (ext == ".pdf")  return "application/pdf";

    return "application/octet-stream"; // fallback
}

HttpResponse create_response(const HttpRequest& request) {
    HttpResponse response;
    response.version = request.version;
    if(request.method == "GET") {
        // Use canonical to resolve the absolute path (ex: /home/user/project/public/index.html)
        fs::path base = fs::canonical("./public");
        // Use weakly_canonical to resolve the requested path relative to the base directory 
        fs::path file_path = fs::weakly_canonical(base / request.path.substr(1));
        // Check if the requested path is within the base directory
        fs::path relative_path = file_path.lexically_relative(base);

        // Check if the file exists and is a regular file
        if(!is_safe_path(relative_path.string())) {
            response.status_code = 403;
            response.reason_phrase = "Forbidden";
        } else if (!fs::exists(file_path) || !fs::is_regular_file(file_path)) {
            response.status_code = 404;
            response.reason_phrase = "Not Found";
        } else {
            // Open the file in binary mode and read its contents into file stream
            std::ifstream file(file_path, std::ios::binary);
            // Lets me write to the string stream ss
            std::ostringstream ss;
            // .rdbuf() returns a pointer to the underlying stream buffer of the file stream, which is then read into the string stream ss
            ss << file.rdbuf();
            // Set the body of the response to the contents of the file
            response.body = ss.str();
            // Set the content type based on the file extension
            response.headers["Content-Type"] = guess_content_type(file_path);
            response.status_code = 200;
            response.reason_phrase = "OK";
        }
    } else if (request.method == "POST") {
        // Construct the file path based on the request path
        fs::path base = fs::canonical("./public");
        // Use weakly_canonical to resolve the requested path relative to the base directory 
        fs::path file_path = fs::weakly_canonical(base / request.path.substr(1));
        // Check if the requested path is within the base directory
        fs::path relative_path = file_path.lexically_relative(base);
        
        if(!is_safe_path(relative_path.string())) {
            response.status_code = 403;
            response.reason_phrase = "Forbidden";
        } else {
            // Create the parent directories if they don't exist
            fs::path parent_dir = file_path.parent_path();
            fs::create_directories(parent_dir);
            // Open the file in binary mode and write the request body to it
            if (fs::exists(file_path)) {
                response.status_code = 409;
                response.reason_phrase = "Conflict error";
            } else {
                fs::path parent_dir = file_path.parent_path();
                fs::create_directories(parent_dir);
                std::ofstream file(file_path, std::ios::binary);
                if (!file) {
                    response.status_code = 500;
                    response.reason_phrase = "Internal Server Error";
                } else {
                    file << request.body;
                    response.status_code = 201;
                    response.reason_phrase = "Created";
                }
            }
        }
    } else if (request.method == "DELETE") {
        // Construct the file path based on the request path
        fs::path base = fs::canonical("./public");
        fs::path file_path = fs::weakly_canonical(base / request.path.substr(1));
        // Check if the requested path is within the base directory
        fs::path relative_path = file_path.lexically_relative(base);
        // Check if the file exists and is a regular file
        if(!is_safe_path(relative_path.string())) {
            response.status_code = 403;
            response.reason_phrase = "Forbidden";
        } else if(!fs::exists(file_path) || !fs::is_regular_file(file_path)) {
            response.status_code = 404;
            response.reason_phrase = "Not Found";
        } else {
            // Delete the file
            fs::remove(file_path);
            response.status_code = 200;
            response.reason_phrase = "OK";
        }
    } else {
        response.status_code = 405;
        response.reason_phrase = "Method Not Yet Implemented";
    }

    response.headers["Content-Length"] = std::to_string(response.body.size());

    return response;
}