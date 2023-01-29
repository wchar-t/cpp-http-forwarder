#include <iostream>
#include <future>
#include <functional>
#include <thread>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include "lib/proxy.hpp"

using namespace std;

Proxy::Proxy(string ip, int port, ProxyEvents events) {
  this->ip = ip;
  this->port = port;
  this->events = events;
  this->Setup();
}

void Proxy::Setup() {
  sockaddr_in addr = {
    .sin_family = AF_INET,
    .sin_port = htons(this->port),
    .sin_addr = {
      .s_addr = inet_addr(this->ip.c_str()),
    },
  };
  this->sock = socket(AF_INET, SOCK_STREAM, 0);
  int on = 1;
  timeval timeout = {
    .tv_sec = 10,
  };
  //int fcntl_flags = fcntl(this->sock, F_GETFL, 0);

  setsockopt(this->sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  setsockopt(this->sock, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));
  setsockopt(this->sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeval));
  //fcntl(this->sock, F_SETFL, fcntl_flags | O_NONBLOCK);
  
  if (bind(this->sock, (sockaddr*)&addr, sizeof(addr)) < 0)
    cout << "Error binding socket on " << this->ip << ":" << this->port << endl;
}

void Proxy::Listen() {
  listen(this->sock, 128);
  cout << "Listening on port: " << this->ip << ":" << this->port << endl;

  socklen_t client_size = sizeof(sockaddr_in);
  //future<void> *call = new future<void>; // PERIGOSO PRA CARALHO
  vector<future<void>> calls;
  
  while (true) {
    //sockaddr_in *client_addr = new sockaddr_in;
    //int *client_sock = new int;
    sockaddr_in *client_addr = new sockaddr_in;
    int *client_sock = new int;
    
    // future<void(int*, sockaddr_in*)>
    *client_sock = accept(this->sock, (sockaddr*)client_addr, &client_size);

    if (*client_sock < 0) {
      continue;
    }

    //*call = std::async(std::launch::async, this->events.on_connection, client_sock, client_addr);
    calls.push_back(std::async(std::launch::async, this->events.on_connection, client_sock, client_addr));
    //thread(this->events.on_connection, client_sock, client_addr);
    
    // should work without problems.. as accept has timeout
    calls.erase(std::remove_if(calls.begin(), calls.end(), [](future<void> &e) {
      return e.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }), calls.end());
  }
}

void Proxy::Close() {
  close(this->sock);
}