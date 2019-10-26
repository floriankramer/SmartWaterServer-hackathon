#include "server.h"
#include <iostream>
#include <nlohmann/json.hpp>

namespace smartwater {

Server::Server(Database *db)
    : _port(8080), _address("0.0.0.0"), _database(db) {}

void Server::start() {
  _server.Get(
      "/sensors", [this](const httplib::Request &req, httplib::Response &res) {
        try {
          using nlohmann::json;
          json resp = encodeSensors(_database);
          std::string s = resp.dump();
          res.set_content(s.c_str(), s.length(), "application/json");
        } catch (const std::exception &e) {
          std::cout << e.what() << std::endl;
          res.set_content(e.what(), strlen(e.what()), "application/json");
        } catch (...) {
          res.set_content("Error", 4, "application/json");
        }
      });

  _server.Get("/sensor/history", [this](const httplib::Request &req,
                                        httplib::Response &res) {
    try {
      using nlohmann::json;
      if (req.has_param("id")) {
        uint64_t id = std::stoul(req.get_param_value("id"));
        json resp = encodeHistory(_database, id);
        std::string s = resp.dump();
        res.set_content(s.c_str(), s.length(), "application/json");
      } else {
        std::string s = "Invalid Request";
        res.set_content(s.c_str(), s.length(), "text/html");
      }
    } catch (const std::exception &e) {
      std::cout << e.what() << std::endl;
      res.set_content(e.what(), strlen(e.what()), "application/json");
    } catch (...) {
      res.set_content("Error", 4, "application/json");
    }
  });
  _server.Post("/sensor/add_measurement", [this](const httplib::Request &req,
                                                 httplib::Response &res) {
    try {
      using nlohmann::json;
      if (req.has_param("id")) {
        uint64_t id = std::stoul(req.get_param_value("id"));
        addMeasurements(req.body, id);
        res.set_content("Done", 4, "application/json");
      } else {
        std::string s = "Invalid Request";
        res.set_content(s.c_str(), s.length(), "text/html");
      }
    } catch (const std::exception &e) {
      std::cout << e.what() << std::endl;
      res.set_content(e.what(), strlen(e.what()), "application/json");
    } catch (...) {
      res.set_content("Error", 4, "application/json");
    }
  });
  _server.Post("/sensor/create", [this](const httplib::Request &req,
                                        httplib::Response &res) {
    using nlohmann::json;
    try {
      addSensor(req.body);
      res.set_content("Done", 4, "application/json");
    } catch (const std::exception &e) {
      std::cout << e.what() << std::endl;
      res.set_content(e.what(), strlen(e.what()), "application/json");
    } catch (...) {
      res.set_content("Error", 4, "application/json");
    }
  });

  _server.listen(_address.c_str(), _port);
}

void Server::addMeasurements(const std::string &body, uint64_t id) {
  using nlohmann::json;
  json j = json::parse(body);
  Measurement m;
  m.timestamp = j["payload_fields"]["timestamp"].get<uint64_t>();
  m.height = j["payload_fields"]["height"].get<double>();
  _database->addMeasurement(id, m);
}

void Server::addSensor(const std::string &body) {
  using nlohmann::json;
  json j = json::parse(body);
  Sensor s;
  s.id = _database->getSensors().size();
  s.name = j["name"].get<std::string>();
  s.latitude = j["lat"].get<double>();
  s.longitude = j["long"].get<double>();
  _database->addSensor(s);
}

nlohmann::json Server::encodeSensors(Database *database) {
  using nlohmann::json;
  std::vector<Sensor> sensors = database->getSensors();
  json::array_t root = json::array();
  for (const Sensor &sensor : sensors) {
    double last_m = database->getLastMeasurement(sensor.id);
    json s;
    s["id"] = sensor.id;
    s["long"] = sensor.longitude;
    s["lat"] = sensor.latitude;
    s["name"] = sensor.name;
    s["last_measurement"] = last_m;
    root.push_back(s);
  }
  json j = std::move(root);
  return j;
}

nlohmann::json Server::encodeHistory(Database *database, uint64_t id) {
  using nlohmann::json;
  std::vector<Measurement> sensors = database->getMeasurements(id);
  json::array_t root = json::array();
  for (const Measurement &measurement : sensors) {
    json s;
    s["time"] = measurement.timestamp;
    s["height"] = measurement.height;
    root.push_back(s);
  }
  json j = std::move(root);
  return j;
}

} // namespace smartwater
