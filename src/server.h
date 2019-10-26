#pragma once

#include <cstdint>
#include <string>
#include <httplib.h>

#include <nlohmann/json.hpp>

#include "database.h"

namespace smartwater {

class Server {
public:
  Server(Database *database);

  void start();

private:
  nlohmann::json encodeSensors(Database *database);
  nlohmann::json encodeHistory(Database *database, uint64_t id);

  void addMeasurements(const std::string &body, uint64_t id);
  void addSensor(const std::string &body);

  uint16_t _port;
  std::string _address;
  httplib::Server _server;
  Database *_database;

};
}
