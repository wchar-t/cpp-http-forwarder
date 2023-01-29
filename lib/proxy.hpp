#pragma once
#include <iostream>
#include <memory>
#include <sys/socket.h>

struct ProxyEvents {
  void (*on_connection)(int*, sockaddr_in*) = NULL;
};

class Proxy {
private:
  std::string ip;
  int port;
  ProxyEvents events;
  int sock;

  void Setup();
public:
  Proxy() {
    // NULL
  }
  Proxy(std::string ip, int port, ProxyEvents events = {});
  void Listen();
  void Close();
};