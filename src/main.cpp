
#include "database.h"
#include "server.h"

int main(int argc, char **argv) {
  smartwater::Database db;
  smartwater::Server server(&db);
  server.start();
}
