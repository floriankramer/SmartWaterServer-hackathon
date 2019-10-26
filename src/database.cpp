#include "database.h"

#include <fstream>
#include <iostream>
#include <sys/stat.h>

namespace smartwater {
Database::Database(const std::string &filename) {
  bool is_db_initialized = false;
  struct stat s;
  if (stat(filename.c_str(), &s) != 0) {
    is_db_initialized = true;
  }
  _db_file.open(filename + ".custom");

  sqlite3_open(filename.c_str(), &_db);
  if (is_db_initialized) {
    init_db();
  }
  init_indices();
}

Database::~Database() { sqlite3_close(_db); }

double Database::getLastMeasurement(uint64_t id) {
  return _measurements[id].back().height;
}

std::vector<Sensor> Database::searchForSensors(std::string name, size_t limit) {
  std::vector<uint64_t> ids = _sensor_search_index.query(name, limit);
  std::vector<Sensor> filtered;
  filtered.reserve(std::min(ids.size(), limit));
  for (uint64_t id : ids) {
    filtered.push_back(_sensors[id]);
    if (filtered.size() >= limit) {
      break;
    }
  }
  return filtered;
}

std::vector<Sensor> Database::getSensors(size_t limit, size_t offset) {
  std::string sql = "SELECT * FROM `sensors`";
  sqlite3_stmt *stmt;
  int r = sqlite3_prepare_v2(_db, sql.c_str(), sql.size(), &stmt, nullptr);
  if (r != 0) {
    std::cout << "Error when compiling the sql " << sql << " : "
              << sqlite3_errstr(r) << std::endl;
  }

  std::vector<Sensor> sensors;
  while ((r = sqlite3_step(stmt)) == SQLITE_ROW && sensors.size() < limit) {
    if (offset > 0) {
      offset--;
    } else {
      Sensor sensor;
      sensor.id = sqlite3_column_int64(stmt, 0);
      sensor.longitude = sqlite3_column_double(stmt, 1);
      sensor.latitude = sqlite3_column_double(stmt, 2);
      sensor.name = std::string(
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3)));
      sensor.location_name = std::string(
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4)));
      sensor.dev_uid = std::string(
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 5)));
      sensors.push_back(sensor);
    }
  }

  return sensors;
}

const std::vector<Sensor> &Database::getSensorsCached() { return _sensors; }

const Sensor &Database::getSensorByIdCached(uint64_t id) {
  return _sensors[id];
}

std::vector<Measurement> Database::getMeasurements(uint64_t id, size_t t_start,
                                                   size_t t_end, size_t limit) {
  std::string sql = "SELECT * FROM `measurements_" + std::to_string(id) + "`";
  sqlite3_stmt *stmt;
  int r = sqlite3_prepare_v2(_db, sql.c_str(), sql.size(), &stmt, nullptr);
  if (r != 0) {
    std::cout << "Error when compiling the sql " << sql << " : "
              << sqlite3_errstr(r) << std::endl;
  }

  std::vector<Measurement> measurements;
  while ((r = sqlite3_step(stmt)) == SQLITE_ROW &&
         measurements.size() < limit) {
    Measurement debug;
    debug.timestamp = sqlite3_column_int64(stmt, 0);
    debug.height = sqlite3_column_double(stmt, 1);
    if (debug.timestamp >= t_start && debug.timestamp <= t_end) {
      measurements.push_back(debug);
    }
  }

  return measurements;
}

const std::vector<Measurement> &Database::getMeasurementsCached(uint64_t id) {
  return _measurements[id];
}

void Database::addSensor(const Sensor &sensor) {
  std::string sql = "INSERT INTO `sensors` VALUES (?, ?, ?, ?, ?, ?)";
  sqlite3_stmt *stmt;
  char *err;
  int r = sqlite3_prepare_v2(_db, sql.c_str(), sql.size(), &stmt, nullptr);
  if (r != 0) {
    std::cout << "Error when compiling the sql " << sql << " : "
              << sqlite3_errstr(r) << std::endl;
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
  r = sqlite3_bind_text(stmt, 5, sensor.location_name.c_str(),
                        sensor.location_name.size(), nullptr);
  if (r != 0) {
    std::cout << "Error when binding the sql '" << sql
              << "' for sensor insertion " << sqlite3_errstr(r) << std::endl;
  }
  r = sqlite3_bind_text(stmt, 6, sensor.dev_uid.c_str(), sensor.dev_uid.size(),
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

  _sensors.push_back(sensor);

  _uuid_to_id[sensor.dev_uid] = sensor.id;
  _sensor_search_index.addMapping(sensor.name, sensor.id);
  _sensor_search_index.addMapping(sensor.location_name, sensor.id);
}

void Database::addMeasurement(uint64_t id, const Measurement &measurement) {
  std::string table_name = "measurements_" + std::to_string(id);
  sqlite3_stmt *stmt;
  std::string sql = "INSERT INTO `" + table_name + "` VALUES (?, ?)";
  int r = sqlite3_prepare_v2(_db, sql.c_str(), sql.size(), &stmt, nullptr);
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
  if (_measurements.size() < id + 1) {
    _measurements.resize(id + 1);
  }
  _measurements[id].push_back(measurement);
}

void Database::init_db() {
  std::string sql =
      "CREATE TABLE `sensors` (id UNSIGNED BIG INT, longitude "
      "DOUBLE, latitude DOUBLE, name TEXT, location_name TEXT, dev_uuid TEXT);";

  sqlite3_exec(_db, sql.c_str(), nullptr, nullptr, nullptr);
}

void Database::init_indices() {
  for (const Sensor &s : getSensors()) {
    _uuid_to_id[s.dev_uid] = s.id;
    _sensor_search_index.addMapping(s.name, s.id);
    _sensor_search_index.addMapping(s.location_name, s.id);
    _sensors.push_back(s);
    _measurements.push_back(getMeasurements(s.id));
  }
}

uint64_t Database::sensorFromUID(const std::string &dev_uuid) {
  std::unordered_map<std::string, uint64_t>::iterator it =
      _uuid_to_id.find(dev_uuid);
  if (it == _uuid_to_id.end()) {
    return -1;
  }
  return it->second;
}

uint64_t Database::newFileBlock(void *data) {
  _db_file.seekp(0, std::ios::end);
  uint64_t new_block_index =
      (static_cast<size_t>(_db_file.tellp()) + 1) / CHUNK_SIZE;
  if (data != nullptr) {
    _db_file.write(reinterpret_cast<char *>(data), CHUNK_SIZE);
  } else {
    std::vector<char> buf(CHUNK_SIZE, 0);
    _db_file.write(buf.data(), CHUNK_SIZE);
  }
  return new_block_index;
}

uint64_t Database::addTable(size_t element_size) {
  if (_index_chunks.back().data.num_tables == NUM_TABLES_INDEX) {
    // Create a new chunk
    PositionedIndexChunk *last_chunk = &_index_chunks.back();
    _index_chunks.emplace_back();
    _index_chunks.back().data.num_tables = 0;
    _index_chunks.back().data.next_chunk = 0;
    _db_file.seekp(0, std::ios::end);
    uint64_t new_block_index = newFileBlock(&_index_chunks.back().data);
    last_chunk->data.next_chunk = new_block_index;
    _index_chunks.back().idx = new_block_index;
  }
  PositionedIndexChunk *last_chunk = &_index_chunks.back();

  DataChunk data;
  data.bytes_used = 0;
  data.entry_size = element_size;
  data.next_chunk = 0;

  uint64_t new_id = newFileBlock(&data);
  last_chunk->data.tables[last_chunk->data.num_tables] = new_id;
}

void Database::load() {
  _db_file.seekg(0);
  _index_chunks.clear();
  _index_chunks.resize(1);
  _db_file.read(reinterpret_cast<char *>(&_index_chunks[0].data), CHUNK_SIZE);
  _index_chunks[0].idx = 0;
  uint64_t num_tables = _index_chunks.back().data.num_tables;

  // read the entire index
  while (_index_chunks.back().data.next_chunk != 0) {
    uint64_t idx = _index_chunks.back().data.next_chunk;
    _db_file.seekg(_index_chunks.back().data.next_chunk * CHUNK_SIZE);
    _index_chunks.emplace_back();
    _db_file.read(reinterpret_cast<char *>(&_index_chunks.back().data),
                  CHUNK_SIZE);
    _index_chunks.back().idx = idx;
    num_tables += _index_chunks.back().data.num_tables;
  }

  _table_last_chunks.resize(num_tables);
  _sensors.reserve(num_tables - 1);
  _measurements.resize(num_tables - 1);

  for (size_t idx_id = 0; idx_id < _index_chunks.size(); idx_id++) {
    IndexChunk &idx = _index_chunks[idx_id].data;
    for (size_t table_offset = 0; table_offset < idx.num_tables;
         table_offset++) {
      // Load the table
      uint64_t table_id = (idx_id * 510) + table_offset;
      uint64_t block_id = idx.tables[table_offset];
      _db_file.seekg(block_id * CHUNK_SIZE);
      _db_file.read(
          reinterpret_cast<char *>(&_table_last_chunks[table_id].data),
          CHUNK_SIZE);
      _table_last_chunks[table_id].idx = block_id;
      if (table_id == 0) {
        onSensorBlockLoaded(_table_last_chunks[table_id].data);
      } else {
        onMeasurementBlockLoaded(_table_last_chunks[table_id].data, table_id);
      }
    }
  }
}

void Database::onSensorBlockLoaded(const DataChunk &chunk) {
  uint16_t num_sensors = chunk.bytes_used / sizeof(Sensor);
  size_t sensor_offset = _sensors.size();
  _sensors.resize(_sensors.size() + num_sensors);
  for (size_t i = 0; i < num_sensors; i++) {
    _sensors[sensor_offset + i] =
        *reinterpret_cast<const Sensor *>(chunk.data + (i * sizeof(Sensor)));
  }
}

void Database::onMeasurementBlockLoaded(const DataChunk &chunk,
                                        uint64_t table_id) {
  uint16_t num_measurements = chunk.bytes_used / sizeof(Measurement);
  std::vector<Measurement> &measurements = _measurements[table_id];
  size_t measurement_offset = measurements.size();
  measurements.resize(measurements.size() + num_measurements);
  for (size_t i = 0; i < num_measurements; i++) {
    measurements[measurement_offset + i] =
        *reinterpret_cast<const Measurement *>(chunk.data +
                                               (i * sizeof(Measurement)));
  }
}

} // namespace smartwater
