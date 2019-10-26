#pragma once
#define CPPHTTPLIB_OPENSSL_SUPPORT

#include <cstdint>
#include <functional>
#include <httplib.h>
#include <string>

#include <nlohmann/json.hpp>

#include "database.h"

namespace smartwater {

class Server {
public:
  Server(Database *database, const std::string &cert_path,
         const std::string &key_path, uint16_t port = 8080);

  void start();

private:
  nlohmann::json
  encodeSensors(const std::vector<Sensor> &sensors, Database *db,
                std::function<bool(const Sensor &)> _filter = nullptr);
  nlohmann::json encodeHistory(const std::vector<Measurement> &measurements);

  void setCommonHeaders(httplib::Response *response);

  void addMeasurements(const std::string &body);
  void addSensor(const std::string &body);

  uint16_t _port;
  std::string _address;
  httplib::Server _server;
  Database *_database;
};
} // namespace smartwater
