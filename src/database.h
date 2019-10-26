#pragma once

#include "sensor.h"
#include <cstdint>
#include <sqlite3.h>
#include <vector>

namespace smartwater {
class Database {
public:
  Database();
  virtual ~Database();

  double getLastMeasurement(uint64_t id);
  std::vector<Measurement> getMeasurements(uint64_t id);
  std::vector<Sensor> getSensors();

  void addSensor(const Sensor &sensor);
  void addMeasurement(uint64_t id, const Measurement &measurement);

private:
  void init_db();
  sqlite3 *_db;
};
} // namespace smartwater
