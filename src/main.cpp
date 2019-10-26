
#include "database.h"
#include "server.h"
#include <iostream>

int main(int argc, char **argv) {
  uint16_t port = 8080;
  std::string cert;
  std::string key;
  if (argc == 3) {
    cert = argv[1];
    key = argv[2];
  } else if (argc == 4) {
    cert = argv[1];
    key = argv[2];
    port = atoi(argv[3]);
  } else {
    std::cout << "Expected 2 or 3 arguments, but got " << (argc - 1)
              << std::endl;
    std::cout << "Usage: " << argv[0] << " <cert_path> <key_path> [port]"
              << std::endl;
    return 1;
  }
  smartwater::Database db;
  smartwater::Server server(&db, cert, key, port);
  server.start();
}
