
#include "database.h"
#include "server.h"

int main(int argc, char **argv) {
  uint16_t port = 8080;
  if (argc == 2) {
    port = atoi(argv[1]);
  }
  smartwater::Database db;
  smartwater::Server server(&db, port);
  server.start();
}
