#include "database.h"
#include <iostream>

#include "database.h"

int main(int argc, char **argv) {
  if (argc != 4) {
    std::cout << "Expected 3 arguments, but got " << (argc - 1) << std::endl;
    std::cout << "Usage: " << argv[0] << " <lat> <long> <name>" << std::endl;
    return 1;
  }
  smartwater::Database db;
  smartwater::Sensor s;
  s.id = db.getSensors().size();
  s.latitude= std::atof(argv[1]);
  s.longitude = std::atof(argv[2]);
  s.name = argv[3];
  db.addSensor(s);
}
