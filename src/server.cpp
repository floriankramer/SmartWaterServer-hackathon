#include "server.h"

#include <iostream>
#include <nlohmann/json.hpp>

#include "logger.h"
#include "util.h"

namespace smartwater {

Server::Server(Database *db, const std::string &cert_path,
               const std::string &key_path, uint16_t port)
    : _port(port), _address("0.0.0.0"), _server(), _database(db) {}

void Server::start() {
  LOG_INFO << "Starting the webserver..." << LOG_END;

  _server.set_logger(
      [](const httplib::Request &req, const httplib::Response &res) {
        LOG_INFO << "Request for " << req.path << LOG_END;
      });

  _server.set_error_handler([this](const httplib::Request &req,
                                   httplib::Response &res) {
    const char *fmt = "<p>Error Status: <span style='color:red;'>%d</span></p>";
    char buf[BUFSIZ];
    snprintf(buf, sizeof(buf), fmt, res.status);
    res.set_content(buf, "text/html");
    setCommonHeaders(&res);
  });

  _server.Options(".*",
                  [this](const httplib::Request &req, httplib::Response &res) {
                    res.status = 200;
                    res.headers.clear();
                    res.set_header("Access-Control-Allow-Origin", "*");
                    res.set_header("Access-Control-Allow-Methods", "POST, GET");
                    res.set_header("Access-Control-Max-Age", "86400");
                    res.set_header("Access-Control-Allow-Headers", "*");
                  });

  _server.Get("/sensors", [this](const httplib::Request &req,
                                 httplib::Response &res) {
    try {
      using nlohmann::json;
      size_t limit = std::numeric_limits<size_t>::max();
      size_t offset = 0;
      if (req.has_param("limit")) {
        limit = std::stoul(req.get_param_value("limit"));
      }
      if (req.has_param("offset")) {
        offset = std::stoul(req.get_param_value("offset"));
      }

      json response;
      if (req.has_param("lat") && req.has_param("long") &&
          req.has_param("dist")) {
        double latitude = std::stod(req.get_param_value("lat"));
        double longitude = std::stod(req.get_param_value("long"));
        double dist = std::stod(req.get_param_value("dist"));

        std::vector<Sensor> sensors = _database->getSensorsCached();
        response =
            encodeSensors(sensors, _database,
                          [latitude, longitude, dist](const Sensor &s) -> bool {
                            return greatCircleDist(s.longitude, s.latitude,
                                                   longitude, latitude) <= dist;
                          });
      } else if (req.has_param("name")) {
        std::string name = req.get_param_value("name");
        std::vector<Sensor> sensors = _database->searchForSensors(name, limit);
        response = encodeSensors(sensors, _database);
      } else {
        std::vector<Sensor> sensors = _database->getSensorsCached();
        response = encodeSensors(sensors, _database);
      }
      std::string s = response.dump();
      res.set_content(s.c_str(), s.length(), "application/json");
      setCommonHeaders(&res);
    } catch (const std::exception &e) {
      LOG_ERROR << e.what() << LOG_END;
      res.set_content(e.what(), strlen(e.what()), "application/json");
      res.status = 400;
      setCommonHeaders(&res);
    } catch (...) {
      res.set_content("Error", 4, "application/json");
      res.status = 400;
      setCommonHeaders(&res);
    }
  });

  _server.Get("/sensor/history", [this](const httplib::Request &req,
                                        httplib::Response &res) {
    try {
      using nlohmann::json;
      size_t limit = std::numeric_limits<size_t>::max();
      if (req.has_param("limit")) {
        limit = std::stoul(req.get_param_value("limit"));
      }
      size_t t_start = 0;
      size_t t_end = std::numeric_limits<size_t>::max();
      if (req.has_param("from")) {
        t_start = std::stoul(req.get_param_value("from"));
      }
      if (req.has_param("to")) {
        t_end = std::stoul(req.get_param_value("to"));
      }

      if (req.has_param("id")) {
        uint64_t id = std::stoul(req.get_param_value("id"));
        std::vector<Measurement> measurements;
        for (const Measurement &m : _database->getMeasurementsCached(id)) {
          if (m.timestamp >= t_start && m.timestamp <= t_end) {
            measurements.push_back(m);
            if (measurements.size() >= limit) {
              break;
            }
          }
        }
        json resp = encodeHistory(measurements);
        std::string s = resp.dump();
        res.set_content(s.c_str(), s.length(), "application/json");
        setCommonHeaders(&res);
      } else {
        std::string s = "Invalid Request";
        res.set_content(s.c_str(), s.length(), "text/html");
        setCommonHeaders(&res);
      }
    } catch (const std::exception &e) {
      LOG_ERROR << e.what() << LOG_END;
      res.set_content(e.what(), strlen(e.what()), "application/json");
      res.status = 400;
      setCommonHeaders(&res);
    } catch (...) {
      res.set_content("Error", 4, "application/json");
      res.status = 400;
      setCommonHeaders(&res);
    }
  });
  _server.Post("/sensor/add_measurement", [this](const httplib::Request &req,
                                                 httplib::Response &res) {
    try {
      using nlohmann::json;
      addMeasurements(req.body);
      res.set_content("Done", 4, "application/json");
      setCommonHeaders(&res);
    } catch (const std::exception &e) {
      LOG_ERROR << e.what() << LOG_END;
      res.set_content(e.what(), strlen(e.what()), "application/json");
      res.status = 400;
      setCommonHeaders(&res);
    } catch (...) {
      res.set_content("Error", 4, "application/json");
      res.status = 400;
      setCommonHeaders(&res);
    }
  });
  _server.Post("/sensor/create", [this](const httplib::Request &req,
                                        httplib::Response &res) {
    using nlohmann::json;
    try {
      addSensor(req.body);
      res.status = 200;
      res.set_content("Done", 4, "application/json");
      setCommonHeaders(&res);
    } catch (const std::exception &e) {
      LOG_ERROR << e.what() << LOG_END;
      res.set_content(e.what(), strlen(e.what()), "application/json");
      res.status = 400;
      setCommonHeaders(&res);
    } catch (...) {
      res.set_content("Error", 4, "application/json");
      res.status = 400;
      setCommonHeaders(&res);
    }
  });

  LOG_INFO << "Starting to listen" << LOG_END;
  if (!_server.listen(_address.c_str(), _port)) {
    LOG_ERROR << "Error when binding to the socket" << LOG_END;
  }
} // namespace smartwater

void Server::setCommonHeaders(httplib::Response *response) {
  response->set_header("Access-Control-Allow-Origin", "*");
}

void Server::addMeasurements(const std::string &body) {
  using nlohmann::json;
  json j = json::parse(body);
  std::string uid = j["dev_id"].get<std::string>();

  Measurement m;
  m.timestamp = j["payload_fields"]["timestamp"].get<uint64_t>();
  m.height = j["payload_fields"]["height"].get<double>();
  _database->addMeasurement(_database->sensorFromUID(uid), m);
}

void Server::addSensor(const std::string &body) {
  using nlohmann::json;
  json j = json::parse(body);
  Sensor s;
  s.id = _database->getNumSensors();
  s.name = j["name"].get<std::string>();
  s.latitude = j["latitude"].get<double>();
  s.longitude = j["longitude"].get<double>();
  s.location_name = j["loc_name"].get<std::string>();
  s.dev_uid = j["dev_uid"].get<std::string>();
  _database->addSensor(s);
}

nlohmann::json
Server::encodeSensors(const std::vector<Sensor> &sensors, Database *db,
                      std::function<bool(const Sensor &)> _filter) {
  using nlohmann::json;
  json::array_t root = json::array();
  for (const Sensor &sensor : sensors) {
    if (_filter == nullptr || _filter(sensor)) {
      double last_m = db->getLastMeasurement(sensor.id);
      json s;
      s["id"] = sensor.id;
      s["long"] = sensor.longitude;
      s["lat"] = sensor.latitude;
      s["name"] = sensor.name;
      s["last_measurement"] = last_m;
      s["loc_name"] = sensor.location_name;
      root.push_back(s);
    }
  }
  json j = std::move(root);
  return j;
}

nlohmann::json
Server::encodeHistory(const std::vector<Measurement> &measurements) {
  using nlohmann::json;
  json::array_t root = json::array();
  for (const Measurement &measurement : measurements) {
    json s;
    s["time"] = measurement.timestamp;
    s["height"] = measurement.height;
    root.push_back(s);
  }
  json j = std::move(root);
  return j;
}

} // namespace smartwater
