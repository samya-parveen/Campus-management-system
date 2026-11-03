#pragma once

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace httplib {

struct Request {
  std::string method;
  std::string path;
  std::string body;
  std::smatch matches;
  std::map<std::string, std::string> headers;

  bool has_header(const std::string& name) const {
    return headers.find(lower(name)) != headers.end();
  }
  std::string get_header_value(const std::string& name) const {
    const auto item = headers.find(lower(name));
    return item == headers.end() ? "" : item->second;
  }

 private:
  static std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
  }
};

struct Response {
  int status = 200;
  std::string body;
  std::map<std::string, std::string> headers;

  void set_header(const std::string& name, const std::string& value) {
    headers[name] = value;
  }
  void set_content(const std::string& content, const std::string& type) {
    body = content;
    set_header("Content-Type", type);
  }
};

class Server {
 public:
  using Handler = std::function<void(const Request&, Response&)>;

  void Get(const std::string& path, Handler handler) { add("GET", path, handler); }
  void Post(const std::string& path, Handler handler) { add("POST", path, handler); }
  void Put(const std::string& path, Handler handler) { add("PUT", path, handler); }
  void Delete(const std::string& path, Handler handler) { add("DELETE", path, handler); }
  void Options(const std::string& path, Handler handler) { add("OPTIONS", path, handler); }

  bool set_mount_point(const std::string& mount, const std::string& directory) {
    if (mount != "/") return false;
    char fullPath[MAX_PATH];
    if (!GetFullPathNameA(directory.c_str(), MAX_PATH, fullPath, nullptr)) return false;
    staticRoot_ = fullPath;
    return true;
  }

  void set_error_handler(std::function<void(const Request&, Response&)> handler) {
    errorHandler_ = handler;
  }

  bool listen(const std::string& host, int port) {
    WSADATA startup;
    if (WSAStartup(MAKEWORD(2, 2), &startup) != 0) return false;
    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) {
      WSACleanup();
      return false;
    }
    BOOL reuse = TRUE;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<u_short>(port));
    address.sin_addr.s_addr = inet_addr(host.c_str());
    if (address.sin_addr.s_addr == INADDR_NONE ||
        bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
        ::listen(listener, SOMAXCONN) != 0) {
      closesocket(listener);
      listener = INVALID_SOCKET;
    }
    if (listener == INVALID_SOCKET) {
      WSACleanup();
      return false;
    }

    for (;;) {
      SOCKET client = accept(listener, nullptr, nullptr);
      if (client == INVALID_SOCKET) continue;
      serve(client);
      shutdown(client, SD_BOTH);
      closesocket(client);
    }
  }

 private:
  struct Route {
    std::string method;
    std::regex path;
    Handler handler;
  };

  std::vector<Route> routes_;
  std::function<void(const Request&, Response&)> errorHandler_;
  std::string staticRoot_;

  void add(const std::string& method, const std::string& path, Handler handler) {
    routes_.push_back(Route{method, std::regex(path), std::move(handler)});
  }

  static std::string trim(const std::string& value) {
    const size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
  }

  static std::string decodePath(const std::string& value) {
    std::string result;
    for (size_t i = 0; i < value.size(); ++i) {
      if (value[i] == '%' && i + 2 < value.size()) {
        char* end = nullptr;
        const long byte = std::strtol(value.substr(i + 1, 2).c_str(), &end, 16);
        if (end && *end == '\0') {
          result.push_back(static_cast<char>(byte));
          i += 2;
          continue;
        }
      }
      result.push_back(value[i] == '+' ? ' ' : value[i]);
    }
    return result;
  }

  static bool receiveRequest(SOCKET client, Request& request) {
    std::string data;
    char buffer[8192];
    size_t headerEnd = std::string::npos;
    size_t contentLength = 0;
    bool lengthKnown = false;
    while (headerEnd == std::string::npos || data.size() < headerEnd + 4 + contentLength) {
      const int received = recv(client, buffer, sizeof(buffer), 0);
      if (received <= 0) return false;
      data.append(buffer, static_cast<size_t>(received));
      if (data.size() > 1024 * 1024 + 16384) return false;
      if (headerEnd == std::string::npos) {
        headerEnd = data.find("\r\n\r\n");
        if (headerEnd != std::string::npos) {
          const std::string headers = data.substr(0, headerEnd);
          std::istringstream lines(headers);
          std::string line;
          if (!std::getline(lines, line)) return false;
          std::istringstream firstLine(line);
          std::string target;
          std::string version;
          if (!(firstLine >> request.method >> target >> version) ||
              version.compare(0, 5, "HTTP/") != 0) return false;
          const size_t query = target.find('?');
          request.path = decodePath(target.substr(0, query));
          while (std::getline(lines, line)) {
            const size_t separator = line.find(':');
            if (separator == std::string::npos) continue;
            std::string name = trim(line.substr(0, separator));
            std::transform(name.begin(), name.end(), name.begin(),
                           [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            request.headers[name] = trim(line.substr(separator + 1));
          }
          const auto length = request.headers.find("content-length");
          if (length != request.headers.end()) {
            try {
              contentLength = static_cast<size_t>(std::stoul(length->second));
              if (contentLength > 1024 * 1024) return false;
              lengthKnown = true;
            } catch (...) {
              return false;
            }
          }
          if (request.headers.find("transfer-encoding") != request.headers.end()) return false;
        }
      }
    }
    if (!lengthKnown && request.method != "GET" && request.method != "HEAD" &&
        request.method != "OPTIONS" && request.method != "DELETE") return false;
    if (headerEnd != std::string::npos) request.body = data.substr(headerEnd + 4, contentLength);
    return true;
  }

  static std::string reason(int status) {
    switch (status) {
      case 200: return "OK";
      case 201: return "Created";
      case 204: return "No Content";
      case 400: return "Bad Request";
      case 401: return "Unauthorized";
      case 403: return "Forbidden";
      case 404: return "Not Found";
      case 409: return "Conflict";
      case 413: return "Payload Too Large";
      case 500: return "Internal Server Error";
      default: return "Error";
    }
  }

  static void sendResponse(SOCKET client, Response& response) {
    if (response.status < 100 || response.status > 599) response.status = 500;
    if (response.headers.find("Content-Type") == response.headers.end()) {
      response.set_header("Content-Type", "text/plain; charset=utf-8");
    }
    response.set_header("Content-Length", std::to_string(response.body.size()));
    response.set_header("Connection", "close");
    std::ostringstream headers;
    headers << "HTTP/1.1 " << response.status << ' ' << reason(response.status) << "\r\n";
    for (const auto& header : response.headers) headers << header.first << ": " << header.second << "\r\n";
    headers << "\r\n";
    const std::string prefix = headers.str();
    auto sendAll = [client](const char* bytes, size_t count) {
      while (count > 0) {
        const int sent = send(client, bytes, static_cast<int>(std::min<size_t>(count, 1 << 20)), 0);
        if (sent <= 0) return false;
        bytes += sent;
        count -= static_cast<size_t>(sent);
      }
      return true;
    };
    if (!sendAll(prefix.data(), prefix.size())) return;
    sendAll(response.body.data(), response.body.size());
  }

  std::string contentType(const std::string& path) const {
    const size_t dot = path.find_last_of('.');
    const std::string extension = dot == std::string::npos ? "" : path.substr(dot);
    if (extension == ".html") return "text/html; charset=utf-8";
    if (extension == ".css") return "text/css; charset=utf-8";
    if (extension == ".js") return "application/javascript; charset=utf-8";
    if (extension == ".svg") return "image/svg+xml";
    if (extension == ".png") return "image/png";
    if (extension == ".jpg" || extension == ".jpeg") return "image/jpeg";
    if (extension == ".ico") return "image/x-icon";
    if (extension == ".csv") return "text/csv; charset=utf-8";
    return "application/octet-stream";
  }

  bool serveStatic(const Request& request, Response& response) const {
    if (staticRoot_.empty() || (request.method != "GET" && request.method != "HEAD")) return false;
    std::string relative = request.path == "/" ? "/index.html" : request.path;
    if (relative.find("..") != std::string::npos || relative.find('\\') != std::string::npos) return false;
    const std::string candidate = staticRoot_ + relative;
    char fullPath[MAX_PATH];
    if (!GetFullPathNameA(candidate.c_str(), MAX_PATH, fullPath, nullptr)) return false;
    std::string full(fullPath);
    std::string root(staticRoot_);
    std::transform(full.begin(), full.end(), full.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    std::transform(root.begin(), root.end(), root.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (full.compare(0, root.size(), root) != 0) return false;
    std::ifstream file(fullPath, std::ios::binary);
    if (!file) return false;
    response.body.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    response.set_header("Content-Type", contentType(fullPath));
    response.set_header("Cache-Control", "no-cache");
    response.set_header("X-Content-Type-Options", "nosniff");
    return true;
  }

  void serve(SOCKET client) {
    Request request;
    if (!receiveRequest(client, request)) {
      Response bad;
      bad.status = 400;
      bad.set_content("Bad Request", "text/plain; charset=utf-8");
      sendResponse(client, bad);
      return;
    }
    Response response;
    bool matched = false;
    for (Route& route : routes_) {
      std::smatch matches;
      if (request.method == route.method &&
          std::regex_match(request.path, matches, route.path)) {
        request.matches = matches;
        matched = true;
        route.handler(request, response);
        break;
      }
    }
    if (!matched && !serveStatic(request, response)) {
      response.status = 404;
      if (errorHandler_) errorHandler_(request, response);
      else response.set_content("Not Found", "text/plain; charset=utf-8");
    }
    sendResponse(client, response);
  }
};

}
