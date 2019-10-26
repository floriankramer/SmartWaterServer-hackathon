#pragma once

#include <string>
#include <cstring>

namespace smartwater {
struct Sensor {
  uint64_t id;
  double longitude;
  double latitude;
  std::string name;
  std::string location_name;
  std::string dev_uid;
};

struct Measurement {
  uint64_t timestamp;
  double height;
};
} // namespace smartwater
