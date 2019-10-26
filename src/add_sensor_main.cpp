#include "database.h"
#include <iostream>

#include "database.h"

int main(int argc, char **argv) {
  if (argc != 6) {
    std::cout << "Expected 5 arguments, but got " << (argc - 1) << std::endl;
    std::cout << "Usage: " << argv[0]
              << " <lat> <long> <name> <loc_name> <dev-uid>" << std::endl;
    return 1;
  }
  smartwater::Database db;
  smartwater::Sensor s;
  s.id = db.getNumSensors();
  s.latitude = std::atof(argv[1]);
  s.longitude = std::atof(argv[2]);
  s.name = argv[3];
  s.location_name = argv[4];
  s.dev_uid = argv[5];
  db.addSensor(s);
}
