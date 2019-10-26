#include "database.h"

#include <iostream>
#include <sys/stat.h>

namespace smartwater {
Database::Database() {
  bool is_db_initialized = false;
  struct stat s;
  if (stat("./db.sqlite", &s) != 0) {
    is_db_initialized = true;
  }

  sqlite3_open("./db.sqlite", &_db);
  if (is_db_initialized) {
    init_db();
  }
}

Database::~Database() { sqlite3_close(_db); }

double Database::getLastMeasurement(uint64_t id) { return 42; }

std::vector<Sensor> Database::getSensors() {
  std::string sql = "SELECT * FROM `sensors`";
  sqlite3_stmt *stmt;
  int r = sqlite3_prepare_v3(_db, sql.c_str(), sql.size(), 0, &stmt, nullptr);
  if (r != 0) {
    std::cout << "Error when compiling the sql " << sqlite3_errstr(r)
              << std::endl;
  }

  std::vector<Sensor> sensors;
  while ((r = sqlite3_step(stmt)) == SQLITE_ROW) {
    Sensor sensor;
    sensor.id = sqlite3_column_int64(stmt, 0);
    sensor.longitude = sqlite3_column_double(stmt, 1);
    sensor.latitude = sqlite3_column_double(stmt, 2);
    sensor.name = std::string(
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3)));
    sensors.push_back(sensor);
  }

  return sensors;
}

std::vector<Measurement> Database::getMeasurements(uint64_t id) {
  std::string sql = "SELECT * FROM `measurements_" + std::to_string(id) + "`";
  sqlite3_stmt *stmt;
  int r = sqlite3_prepare_v3(_db, sql.c_str(), sql.size(), 0, &stmt, nullptr);
  if (r != 0) {
    std::cout << "Error when compiling the sql " << sqlite3_errstr(r)
              << std::endl;
  }

  std::vector<Measurement> measurements;
  while ((r = sqlite3_step(stmt)) == SQLITE_ROW) {
    Measurement debug;
    debug.timestamp = sqlite3_column_int64(stmt, 0);
    debug.height = sqlite3_column_double(stmt, 1);
    measurements.push_back(debug);
  }

  return measurements;
}

void Database::addSensor(const Sensor &sensor) {
  std::string sql = "INSERT INTO `sensors` VALUES (?, ?, ?, ?)";
  sqlite3_stmt *stmt;
  char *err;
  int r = sqlite3_prepare_v3(_db, sql.c_str(), sql.size(), 0, &stmt, nullptr);
  if (r != 0) {
    std::cout << "Error when compiling the sql " << sqlite3_errstr(r)
              << std::endl;
  }
  r = sqlite3_bind_int64(stmt, 1, sensor.id);
  if (r != 0) {
    std::cout << "Error when binding the sql '" << sql
              << "' for sensor insertion " << sqlite3_errstr(r) << std::endl;
  }
  r = sqlite3_bind_double(stmt, 2, sensor.longitude);
  if (r != 0) {
    std::cout << "Error when binding the sql '" << sql
              << "' for sensor insertion " << sqlite3_errstr(r) << std::endl;
  }
  r = sqlite3_bind_double(stmt, 3, sensor.latitude);
  if (r != 0) {
    std::cout << "Error when binding the sql '" << sql
              << "' for sensor insertion " << sqlite3_errstr(r) << std::endl;
  }
  r = sqlite3_bind_text(stmt, 4, sensor.name.c_str(), sensor.name.size(),
                        nullptr);
  if (r != 0) {
    std::cout << "Error when binding the sql '" << sql
              << "' for sensor insertion " << sqlite3_errstr(r) << std::endl;
  }

  r = sqlite3_step(stmt);
  if (r != 0 && r != SQLITE_DONE) {
    std::cout << "Error when executing the sql '" << sql
              << "' for sensor insertion " << sqlite3_errstr(r) << "(" << r
              << ")" << std::endl;
  }
  sqlite3_finalize(stmt);

  sql = "CREATE TABLE `measurements_" + std::to_string(sensor.id) +
        "` (timestamp UNSIGNED BIG INT, height DOUBLE)";
  r = sqlite3_exec(_db, sql.c_str(), nullptr, nullptr, &err);
  if (r != 0) {
    std::cout << "Error when executing the sql for sensor insertion " << err
              << std::endl;
  }
}

void Database::addMeasurement(uint64_t id, const Measurement &measurement) {
  std::string table_name = "measurements_" + std::to_string(id);
  sqlite3_stmt *stmt;
  std::string sql = "INSERT INTO `" + table_name + "` VALUES (?, ?)";
  int r = sqlite3_prepare_v3(_db, sql.c_str(), sql.size(), -1, &stmt, nullptr);
  if (r != 0 && r != SQLITE_DONE) {
    std::cout << "Error when compiling '" << sql
              << "' for adding a measurement " << sqlite3_errstr(r) << "(" << r
              << ")" << std::endl;
  }
  r = sqlite3_bind_int64(stmt, 1, measurement.timestamp);
  if (r != 0 && r != SQLITE_DONE) {
    std::cout << "Error when binding " << measurement.timestamp
              << " to entry 1 in the sql '" << sql
              << "' for adding a measurement " << sqlite3_errstr(r) << "(" << r
              << ")" << std::endl;
  }
  r = sqlite3_bind_double(stmt, 2, measurement.height);
  if (r != 0 && r != SQLITE_DONE) {
    std::cout << "Error when binding " << measurement.height
              << " to entry 2 in the sql '" << sql
              << "' for adding a measurement " << sqlite3_errstr(r) << "(" << r
              << ")" << std::endl;
  }
  r = sqlite3_step(stmt);
  if (r != 0 && r != SQLITE_DONE) {
    std::cout << "Error when executing the sql '" << sql
              << "' for adding a measurement " << sqlite3_errstr(r) << "(" << r
              << ")" << std::endl;
  }
  r = sqlite3_finalize(stmt);
}

void Database::init_db() {
  std::string sql = "CREATE TABLE `sensors` (id UNSIGNED BIG INT, longitude "
                    "DOUBLE, latitude DOUBLE, name TEXT);";

  sqlite3_exec(_db, sql.c_str(), nullptr, nullptr, nullptr);
}

} // namespace smartwater
