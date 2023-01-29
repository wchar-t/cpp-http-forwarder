#include <iostream>
#include <functional>
#include <map>
#include <vector>
#include <chrono>
#include <thread>
#include <memory>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include "lib/cr_handler.hpp"
#include "lib/proxy.hpp"
#include "lib/request.hpp"

using std::string, std::cout, std::map, std::vector;

// some headers should be avoided (origin, host, etc..)
// its not a proper proxy (but a http proxy) with hardcoded backend where we get the data
const vector<string> headers_avoid = {
  "origin", "allow-control-allow-origin", "host",
};
const map<string, string> origin_rules = {
  { "Ly9wcm9kLmdjY3J1bmNoeXJvbGwuY29tL2", "d1.com" },
  { "t=exp=", "d2.com" },
  { "Ly9wbC5jcnVuY2h5cm9sbC5jb20", "d3.com" },
  { "Ly92LnZydi5jby", "d4.com" },
};

string get_original_host(string seed) {
  string host = "undefined";

  for (auto pair : origin_rules) {
    if (seed.find(pair.first) != string::npos) {
      host = pair.second;
      break;
    }
  }

  return host;
}

void clean_headers(map<string, string> &headers) {
  // will remove headers listed by global headers_avoid
  for (auto pair = headers.begin() ; pair != headers.end() ; ) {
    string name;
    bool remove = false;
    
    for (int i = 0; i < pair->first.size(); i++) {
      name += std::tolower(pair->first[i]);
    }

    for (string avoid : headers_avoid) {
      if (name == avoid) {
        remove = true;
        break;
      }
    }

    if (remove)
      pair = headers.erase(pair);
    else
      ++pair;
  }
}

vector<string> split(string raw, string term) {
  vector<string> parts;
  string token;
  size_t pos = 0;
  size_t find_pos = 0;

  while ((find_pos = raw.find(term, pos)) != string::npos) {
    token = raw.substr(pos, find_pos - pos);

    if (token.size() != 0)
      parts.push_back(token);
    
    pos = find_pos + term.size();
  }

  return parts;
}

void on_proxy_connection(int *sock, sockaddr_in *client) {
  // drop POST requests
  string headers_raw;
  sockaddr_in client_addr = *client;
  int client_sock = *sock;
  int recv_total = 0; // capped at 8192 as it's the header limit
  int recv_limit = 8192;
  timeval timeout = { 10, 0 };

  setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

  while (true) {
    if (recv_total >= recv_limit)
      break;
    if (headers_raw.find("\r\n\r\n") != string::npos)
      break;
    
    char buff[1024]; // 1KB chunk
    int bytes = recv(client_sock, buff, sizeof(buff), 0);

    if (bytes <= 0)
      break;
    
    recv_total += bytes;
    headers_raw.append(buff);
  }

  if (headers_raw.size() < 5) {
    close(client_sock);
    delete client;
    delete sock;
    return;
  }

  std::function<void(void*, size_t)> on_curl_write_headers = [client_sock](void *data, size_t size) {
    send(client_sock, data, size, 0);
  };

  std::function<int(void*, size_t)> on_curl_write = [client_sock](void *data, size_t size) {
    void *tmp = malloc(size);
    memcpy(tmp, data, size);
    int ret, bytes = 0;

    while (bytes < size) {
      ret = send(client_sock, (void*)(tmp + bytes), size, 0);
      bytes += ret;
    }
    
    free(tmp);
    return ret < 0 ? ret : size;
  };
  //void (*on_curl_write_ptr)(void*, size_t) = static_cast<void (*)(void*, size_t)>(on_curl_write);

  vector<string> header_lines = split(headers_raw, "\r\n");
  string connection_method = header_lines[0];
  header_lines.erase(header_lines.begin());
  map<string, string> headers = Request::ParseHeaders(header_lines);
  string path = connection_method.substr(connection_method.find("/") + 1, connection_method.find(" HTTP") - connection_method.find("/") - 1);

  string original_host = get_original_host(path);
  clean_headers(headers);
  
  headers["Origin"] = "https://static.crunchyroll.com";
  headers["Referer"] = "https://static.crunchyroll.com/";
  headers["Host"] = original_host;
  cout << "Host: " << original_host << "\n" << std::flush;
  if (original_host == "undefined") {
    close(client_sock);
    delete client;
    delete sock;
    return;
  }

  RequestOptions options = {
    .headers = headers,
    .events = {
      .on_curl_write_headers = on_curl_write_headers,
      .on_curl_write = on_curl_write,
    },
    .save = {
      .headers_raw = false, // CHECK
      .data = false, // CHECK
    },
  };
  string url = "https://" + original_host + "/" + path;
  Response response = Request::Get(url, options);
  //Response response = Request::Get("http://docs.evostream.com/sample_content/assets/bunny.mp4", options);
  //Response response = Request::Get("http://127.0.0.1/djunk/trash.mp4", options);

  //cout << "Request headers: \n" << headers_raw << "\n" << std::flush;
  //cout << "Connection method: \n" << connection_method << "\n" << std::flush;
  //cout << "Header linse: \n" << header_lines.size() << "\n" << std::flush;
  cout << "Conexao respondida com sucesso!" << "\n" << std::flush;
  
  sleep(5);
  close(client_sock);
  delete client;
  delete sock;
}

CRProxy::CRProxy(string ip, int port) {
  ProxyEvents events = {
    .on_connection = on_proxy_connection,
  };
  this->server = Proxy(ip.c_str(), port, events);
}

void CRProxy::Start() {
  this->server.Listen();
}