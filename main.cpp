#include <stdio.h>
#include <iostream>
#include <future>
#include <netinet/in.h>
#include "lib/cr_handler.hpp"


int main() {
  CRProxy hls_proxy = CRProxy("0.0.0.0", 6868);
  hls_proxy.Start();

  return 0;
}