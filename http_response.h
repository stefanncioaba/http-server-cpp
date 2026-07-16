#pragma once
#include "http_request.h"
#include <string>
#include <map>

struct HttpResponse {
    std::string version;
    int status_code;
    std::string reason_phrase;
    std::map<std::string, std::string> headers;
    std::string body;
};

HttpResponse create_response(const HttpRequest& request);