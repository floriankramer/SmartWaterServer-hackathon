#pragma once

#include "sensor.h"
#include <cstdint>
#include <sqlite3.h>
#include <vector>
#include <unordered_map>

namespace smartwater {
class Database {
public:
  Database();
  virtual ~Database();

  double getLastMeasurement(uint64_t id);
  std::vector<Measurement> getMeasurements(uint64_t id, size_t limit = -1);
  std::vector<Sensor> getSensors(size_t limit = -1);

  void addSensor(const Sensor &sensor);
  void addMeasurement(uint64_t id, const Measurement &measurement);

  uint64_t sensorFromUID(const std::string &dev_uuid);

private:
  void init_db();
  void init_uuid_to_id();

  sqlite3 *_db;

  std::unordered_map<std::string, uint64_t> _uuid_to_id;
};
} // namespace smartwater
