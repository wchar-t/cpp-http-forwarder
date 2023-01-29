#pragma once
#include <iostream>
#include "proxy.hpp"

class CRProxy {
private:
  Proxy server;
public:
  CRProxy(std::string ip, int port);
  void Start();
};