#include "database.h"

#include "logger.h"
#include <fstream>
#include <iostream>
#include <sys/stat.h>

namespace smartwater {
Database::Database(const std::string &filename) {
  bool is_db_initialized = true;
  struct stat s;
  int r = 0;
  if ((r = stat(filename.c_str(), &s)) != 0) {
    is_db_initialized = false;
    // Ensure the file exists
    std::ofstream out(filename);
  }

  LOG_INFO << "operating on " << filename << LOG_END;
  _db_file.open(filename, std::ios::in | std::ios::out | std::ios::binary);
  if (!_db_file.is_open()) {
    throw std::runtime_error("Unable to open the file at " + filename);
  }

  if (!is_db_initialized) {
    init_db();
  } else {
    load();
  }
}

Database::~Database() {}

double Database::getLastMeasurement(uint64_t id) {
  if (id < _measurements.size() && _measurements[id].size() > 0) {
    return _measurements[id].back().height;
  } else {
    return 0;
  }
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

const std::vector<Sensor> &Database::getSensorsCached() { return _sensors; }

const Sensor &Database::getSensorByIdCached(uint64_t id) {
  return _sensors[id];
}

const std::vector<Measurement> &Database::getMeasurementsCached(uint64_t id) {
  return _measurements[id];
}

void Database::addSensor(const Sensor &sensor) {
  // serialize the string
  std::vector<char> serialized;
  std::vector<char> buff;
  serializeSensor(sensor, &buff);
  serialized.resize(buff.size() + 4);
  uint32_t s_len = buff.size();
  std::memcpy(serialized.data(), &s_len, 4);
  std::memcpy(serialized.data() + 4, buff.data(), buff.size());

  size_t to_write = serialized.size();
  char *src = serialized.data();

  LOG_DEBUG << "Writing a sensor with " << buff.size() << " bytes" << std::endl;

  // Get the data chunk the sensor should be written to
  PositionedDataChunk *dc = &_table_last_chunks[0];
  while (to_write > 0) {
    LOG_DEBUG << to_write << " bytes left to write" << LOG_END;
    // Figure out how many bytes we want to write into the current chunk
    size_t num_remaining = NUM_DATA_BYTES - dc->data.bytes_used;
    size_t write_size = std::min(num_remaining, to_write);
    LOG_DEBUG << "Wrote " << write_size << " bytes at pos "
              << dc->data.bytes_used << LOG_END;
    std::memcpy(dc->data.data + dc->data.bytes_used, src, write_size);

    // Update the various values tracking write progress
    src += write_size;
    to_write -= write_size;
    dc->data.bytes_used += write_size;
    writeFileChunk(&dc->data, dc->idx);
    if (to_write > 0) {
      // We need a new chunk for the rest of the data
      DataChunk new_chunk;
      new_chunk.bytes_used = 0;
      new_chunk.next_chunk = 0;
      size_t block_idx = newFileBlock(&new_chunk);
      // Build the link
      dc->data.next_chunk = block_idx;
      writeFileChunk(&dc->data, dc->idx);
      // Update the latest entry in the linked list
      dc->data = new_chunk;
      dc->idx = block_idx;
    }
  }

  // Add a new table to store the sensors measurements
  addTable();

  _sensors.push_back(sensor);

  _uuid_to_id[sensor.dev_uid] = sensor.id;
  _sensor_search_index.addMapping(sensor.name, sensor.id);
  _sensor_search_index.addMapping(sensor.location_name, sensor.id);
}

void Database::addMeasurement(uint64_t id, const Measurement &measurement) {

  uint64_t table_id = id + 1;
  if (table_id >= _table_last_chunks.size()) {
    LOG_ERROR << "There is no table for measurements for the sensor with id "
              << id << LOG_END;
    return;
  }
  PositionedDataChunk *dc = &_table_last_chunks[table_id];
  if (NUM_DATA_BYTES < sizeof(Measurement) + dc->data.bytes_used) {
    // We need a new chunk for the table
    DataChunk new_chunk;
    new_chunk.bytes_used = 0;
    new_chunk.next_chunk = 0;
    size_t block_idx = newFileBlock(&new_chunk);
    // Build the link
    dc->data.next_chunk = block_idx;
    writeFileChunk(&dc->data, dc->idx);
    // Update the latest entry in the linked list
    dc->data = new_chunk;
    dc->idx = block_idx;
  }
  std::memcpy(dc->data.data + dc->data.bytes_used, &measurement,
              sizeof(Measurement));
  dc->data.bytes_used += sizeof(Measurement);
  writeFileChunk(&dc->data, dc->idx);

  if (_measurements.size() < id + 1) {
    _measurements.resize(id + 1);
  }
  _measurements[id].push_back(measurement);
}

void Database::init_db() {
  LOG_INFO << "Initializing the database" << LOG_END;
  // Create the first index chunk
  _index_chunks.emplace_back();
  _index_chunks.back().idx = 0;
  _index_chunks.back().data.next_chunk = 0;
  _index_chunks.back().data.num_tables = 0;
  newFileBlock(&_index_chunks[0].data);

  // Allocate two blocks for basic journaling
  newFileBlock();
  newFileBlock();
  addTable();
  _db_file.flush();
  LOG_INFO << "Database initialized" << LOG_END;
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
  _db_file.seekg(0, std::ios::end);
  uint64_t new_block_index =
      (static_cast<size_t>(_db_file.tellg()) + 1) / CHUNK_SIZE;
  if (data != nullptr) {
    writeFileChunk(data, new_block_index);
  } else {
    std::vector<char> buf(CHUNK_SIZE, 0);
    writeFileChunk(buf.data(), new_block_index);
  }
  return new_block_index;
}

void Database::writeFileChunk(void *data, uint64_t idx) {
  if (idx == 0) {
    LOG_DEBUG << "Writing idx 0" << LOG_END;
  }
  _db_file.seekp(idx * CHUNK_SIZE);
  _db_file.write(reinterpret_cast<char *>(data), CHUNK_SIZE);
}

uint64_t Database::addTable() {
  LOG_DEBUG << "Addign a new table" << LOG_END;
  LOG_DEBUG << _index_chunks.back().data.num_tables
            << " == " << NUM_TABLES_INDEX << LOG_END;
  if (_index_chunks.back().data.num_tables == NUM_TABLES_INDEX) {
    // Create a new chunk
    size_t last_chunk_idx = _index_chunks.size() - 1;
    _index_chunks.emplace_back();
    PositionedIndexChunk *last_chunk = &_index_chunks[last_chunk_idx];
    _index_chunks.back().data.num_tables = 0;
    _index_chunks.back().data.next_chunk = 0;
    _db_file.seekp(0, std::ios::end);
    uint64_t new_block_index = newFileBlock(&_index_chunks.back().data);
    last_chunk->data.next_chunk = new_block_index;
    LOG_DEBUG << "Set a new block idx " << new_block_index << " for index "
              << last_chunk->idx << LOG_END;
    writeFileChunk(&last_chunk->data, last_chunk->idx);
    _index_chunks.back().idx = new_block_index;
  }
  PositionedIndexChunk *last_chunk = &_index_chunks.back();

  _table_last_chunks.emplace_back();
  _table_last_chunks.back().data.bytes_used = 0;
  std::memset(_table_last_chunks.back().data.data, 0, NUM_DATA_BYTES);
  _table_last_chunks.back().data.next_chunk = 0;

  uint64_t new_id = newFileBlock(&_table_last_chunks.back().data);
  _table_last_chunks.back().idx = new_id;
  last_chunk->data.tables[last_chunk->data.num_tables] = new_id;
  last_chunk->data.num_tables++;
  // Update the index on disk
  writeFileChunk(&last_chunk->data, last_chunk->idx);
  return (_index_chunks.size() - 1) * NUM_TABLES_INDEX +
         last_chunk->data.num_tables - 1;
}

void Database::load() {
  LOG_INFO << "Loading the database" << LOG_END;
  _db_file.seekg(0);
  _index_chunks.clear();
  _index_chunks.resize(1);
  _db_file.read(reinterpret_cast<char *>(&_index_chunks[0].data), CHUNK_SIZE);
  _index_chunks[0].idx = 0;
  uint64_t num_tables = _index_chunks.back().data.num_tables;

  // read the entire index
  while (_index_chunks.back().data.next_chunk != 0) {
    LOG_DEBUG << "Index chunk " << _index_chunks.back().idx
              << " is followed by " << _index_chunks.back().data.next_chunk
              << LOG_END;
    uint64_t idx = _index_chunks.back().data.next_chunk;
    _db_file.seekg(_index_chunks.back().data.next_chunk * CHUNK_SIZE);
    _index_chunks.emplace_back();
    _db_file.read(reinterpret_cast<char *>(&_index_chunks.back().data),
                  CHUNK_SIZE);
    _index_chunks.back().idx = idx;
    num_tables += _index_chunks.back().data.num_tables;
  }
  LOG_INFO << "Found " << num_tables << " tables." << LOG_END;

  _table_last_chunks.resize(num_tables);
  if (num_tables > 0) {
    _sensors.reserve(num_tables - 1);
    _measurements.resize(num_tables - 1);
  } else {
    LOG_WARN << "no tables found (there should always be at least one)"
             << LOG_END;
  }

  for (size_t idx_id = 0; idx_id < _index_chunks.size(); idx_id++) {
    IndexChunk &idx = _index_chunks[idx_id].data;
    for (size_t table_offset = 0; table_offset < idx.num_tables;
         table_offset++) {
      // This buffer is used for loading block spanning data;
      std::vector<char> buffer;
      // Load the table
      uint64_t table_id = (idx_id * 510) + table_offset;
      uint64_t block_id = idx.tables[table_offset];
      LOG_DEBUG << "Loading table " << table_id << " block id" << block_id
                << LOG_END;
      // Load all of the linked chunks that make up the table
      while (block_id != 0) {
        _db_file.seekg(block_id * CHUNK_SIZE);
        _db_file.read(
            reinterpret_cast<char *>(&_table_last_chunks[table_id].data),
            CHUNK_SIZE);
        _table_last_chunks[table_id].idx = block_id;
        if (table_id == 0) {
          onSensorBlockLoaded(_table_last_chunks[table_id].data, &buffer);
        } else {
          onMeasurementBlockLoaded(_table_last_chunks[table_id].data, table_id);
        }
        block_id = _table_last_chunks[table_id].data.next_chunk;
      }
    }
  }
  LOG_INFO << "Done Loading" << LOG_END;
}

void Database::onSensorBlockLoaded(const DataChunk &chunk,
                                   std::vector<char> *buffer) {
  size_t offset = 0;

  while (offset < chunk.bytes_used) {
    LOG_DEBUG << "Loop offset " << offset << LOG_END;
    size_t bytes_available = chunk.bytes_used - offset;
    if (buffer->empty()) {
      if (buffer->size() + bytes_available < 4) {
        buffer->resize(bytes_available);
        memcpy(buffer->data(), chunk.data + offset, bytes_available);
        // We are done with the chunk
        break;
      } else {
        // Read the length of the serialized sensor
        buffer->resize(4);
        memcpy(buffer->data(), chunk.data + offset, 4);
        offset += 4;
        // Updates the bytes available
        bytes_available = chunk.bytes_used - offset;
      }
    }
    uint32_t bytes_required;
    memcpy(&bytes_required, buffer->data(), 4);

    LOG_DEBUG << "Expecting a sensor of size " << bytes_required << LOG_END;
    LOG_DEBUG << "Already got " << buffer->size() << " bytes in the buffer"
              << LOG_END;

    size_t to_read = bytes_required - (buffer->size() - 4);
    size_t read_size = std::min(to_read, bytes_available);
    size_t buffer_off = buffer->size();
    buffer->resize(buffer->size() + read_size);
    std::memcpy(buffer->data() + buffer_off, chunk.data + offset, read_size);
    offset += read_size;
    to_read -= read_size;
    if (to_read == 0) {
      LOG_DEBUG << "Found a sensor with " << buffer->size()
                << "bytes in the buffer" << LOG_END;
      // We read the entire sensor
      _sensors.emplace_back();
      deserializeSensor(&_sensors.back(), buffer->data() + 4);
      const Sensor &s = _sensors.back();
      _uuid_to_id[s.dev_uid] = s.id;
      _sensor_search_index.addMapping(s.name, s.id);
      _sensor_search_index.addMapping(s.location_name, s.id);
      buffer->clear();
    } else {
      // We reached the end of the data
      break;
    }
  }
}

void Database::onMeasurementBlockLoaded(const DataChunk &chunk,
                                        uint64_t table_id) {
  uint16_t num_measurements = chunk.bytes_used / sizeof(Measurement);
  std::vector<Measurement> &measurements = _measurements[table_id - 1];
  size_t measurement_offset = measurements.size();
  measurements.resize(measurements.size() + num_measurements);
  for (size_t i = 0; i < num_measurements; i++) {
    measurements[measurement_offset + i] =
        *reinterpret_cast<const Measurement *>(chunk.data +
                                               (i * sizeof(Measurement)));
  }
}

size_t Database::getNumSensors() { return _sensors.size(); }

void Database::serializeSensor(const Sensor &sensor,
                               std::vector<char> *buffer) {
  size_t num_bytes = 3 * 8;

  buffer->resize(num_bytes);
  size_t off = 0;
  std::memcpy(buffer->data() + off, &sensor.id, 8);
  off += 8;
  std::memcpy(buffer->data() + off, &sensor.longitude, 8);
  off += 8;
  std::memcpy(buffer->data() + off, &sensor.latitude, 8);
  off += 8;

  serializeString(sensor.name, buffer);
  serializeString(sensor.location_name, buffer);
  serializeString(sensor.dev_uid, buffer);
}

void Database::deserializeSensor(Sensor *sensor, const char *src) {
  size_t off = 0;
  std::memcpy(&sensor->id, src + off, 8);
  off += 8;
  std::memcpy(&sensor->longitude, src + off, 8);
  off += 8;
  std::memcpy(&sensor->latitude, src + off, 8);
  off += 8;

  sensor->name = deserializeString(src, &off);
  sensor->location_name = deserializeString(src, &off);
  sensor->dev_uid = deserializeString(src, &off);
}

std::string Database::deserializeString(const char *src, size_t *off) {
  uint16_t len;
  std::memcpy(&len, src + (*off), 2);
  (*off) += 2;
  std::vector<char> buf(len);
  std::memcpy(buf.data(), src + (*off), len);
  (*off) += len;
  return std::string(buf.begin(), buf.end());
}

void Database::serializeString(const std::string &data,
                               std::vector<char> *buffer) {
  uint16_t len = data.size();
  size_t off = buffer->size();
  buffer->resize(buffer->size() + 2 + len);
  std::memcpy(buffer->data() + off, &len, 2);
  std::memcpy(buffer->data() + off + 2, data.c_str(), len);
}

} // namespace smartwater
