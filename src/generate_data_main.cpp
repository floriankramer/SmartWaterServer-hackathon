#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iostream>

#include "database.h"
#include "sensor.h"

std::string rand_hex_str(size_t length, unsigned int *seed) {
  std::string s;
  s.resize(length, '0');
  for (size_t i = 0; i < length; i++) {
    int v = rand_r(seed) % 16;
    if (v < 10) {
      s[i] = '0' + v;
    } else {
      s[i] = 'A' + v - 10;
    }
  }
  return s;
}

int main(int argc, char **argv) {
  using smartwater::Database;
  using smartwater::Measurement;
  using smartwater::Sensor;
  if (argc != 4) {
    std::cout << "Expected 3 arguments, but got " << (argc - 1) << std::endl;
    std::cout << "Usage: " << argv[0]
              << " <num_sensors> <min_measurements> <max_measurements>"
              << std::endl;
    return 1;
  }

  size_t num_sensors = std::stoul(argv[1]);
  size_t min_measurements = std::stoul(argv[2]);
  size_t max_measurements = std::stoul(argv[3]);

  unsigned int seed = time(nullptr);
  time_t now = time(nullptr);

  Database db;

  size_t id_offset = db.getSensors().size();

  for (size_t i = 0; i < num_sensors; i++) {
    Sensor s;
    s.id = id_offset + i;
    s.name = "TestSensor" + std::to_string(i);
    s.dev_uid = rand_hex_str(16, &seed);
    s.latitude = -90 + 179 * (rand_r(&seed) / static_cast<double>(RAND_MAX));
    s.longitude = -180 + 359 * (rand_r(&seed) / static_cast<double>(RAND_MAX));
    s.location_name = "Nowhere";
    db.addSensor(s);

    size_t num_measurements =
        min_measurements + (rand_r(&seed) / static_cast<double>(RAND_MAX)) *
                               (max_measurements - min_measurements);

    for (size_t j = 0; j < num_measurements; j++) {
      Measurement m;
      m.timestamp = now - 15 * 60 * j;
      m.height =
          sin(j / 0.04) + rand_r(&seed) / static_cast<double>(RAND_MAX) * 0.2;
      db.addMeasurement(s.id, m);
    }
  }
}
