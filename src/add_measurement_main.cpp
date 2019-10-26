#include "database.h"
#include <iostream>

#include "database.h"

int main(int argc, char **argv) {
  if (argc != 4) {
    std::cout << "Expected 3 arguments, but got " << (argc - 1) << std::endl;
    std::cout << "Usage: " << argv[0] << " <id> <timestamp> <height>"
              << std::endl;
    return 1;
  }
  smartwater::Database db;
  uint64_t id = std::atol(argv[1]);
  smartwater::Measurement m;
  m.timestamp = std::atol(argv[2]);
  m.height = std::atof(argv[3]);
  db.addMeasurement(id, m);
}
