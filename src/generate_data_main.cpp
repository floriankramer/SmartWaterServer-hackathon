#include <cmath>
#include <cstdlib>
#include <ctime>
#include <fstream>
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
  if (argc != 5) {
    std::cout << "Expected 3 arguments, but got " << (argc - 1) << std::endl;
    std::cout << "Usage: " << argv[0]
              << " <num_sensors> <min_measurements> <max_measurements> <file>"
              << std::endl;
    return 1;
  }

  size_t num_sensors = std::stoul(argv[1]);
  size_t min_measurements = std::stoul(argv[2]);
  size_t max_measurements = std::stoul(argv[3]);

  std::vector<std::string> city_names;
  {
    std::ifstream in("./staedte_osm.txt");
    std::string line;
    while (!in.eof()) {
      std::getline(in, line);
      city_names.push_back(line);
    }
  }

  unsigned int seed = time(nullptr);
  time_t now = time(nullptr);

  Database db((std::string(argv[4])));

  size_t id_offset = db.getNumSensors();

  for (size_t i = 0; i < num_sensors; i++) {
    std::cout << "Sensor " << i << " / " << num_sensors << std::endl;
    Sensor s;
    s.id = id_offset + i;
    s.name = city_names[rand_r(&seed) % city_names.size()];
    s.dev_uid = rand_hex_str(16, &seed);

    // lat, lon
    // 5.98865807458, 47.3024876979 ; 15.0169958839, 54.983104153
    s.longitude = 6 + 9 * (rand_r(&seed) / static_cast<double>(RAND_MAX));
    s.latitude = 47 + 6 * (rand_r(&seed) / static_cast<double>(RAND_MAX));
    s.location_name = city_names[rand_r(&seed) % city_names.size()];
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
