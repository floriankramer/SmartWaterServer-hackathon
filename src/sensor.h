#pragma once

#include <string>
#include <cstring>

namespace smartwater {
struct Sensor {
  uint64_t id;
  double longitude;
  double latitude;
  std::string name;
};

struct Measurement {
  uint64_t timestamp;
  double height;
};
} // namespace smartwater
