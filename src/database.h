#pragma once

#include "qgram.h"
#include "sensor.h"
#include <fstream>

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace smartwater {
class Database {
  // The database is stored in a custom file. The file is composed of 4k chunks.
  // The first chunk is an index. A data chunk contains entries for exactly one
  // table.

  static const int CHUNK_SIZE = 4096;
  static const int NUM_TABLES_INDEX = CHUNK_SIZE / 8 - 2;
  static const int NUM_DATA_BYTES = CHUNK_SIZE - 8 - 2;

  struct IndexChunk {
    uint64_t num_tables = 0;
    uint64_t tables[NUM_TABLES_INDEX];
    uint64_t next_chunk = 0;
  };

  struct PositionedIndexChunk {
    uint64_t idx = 0;
    IndexChunk data;
  };

  struct DataChunk {
    uint16_t bytes_used = 0;
    char data[NUM_DATA_BYTES];
    uint64_t next_chunk = 0;
  };

  struct JournalChunk {
    uint64_t dirty_chunk;
    char data[4088];
  };

  struct PositionedDataChunk {
    uint64_t idx = 0;
    DataChunk data;
  };

public:
  Database(const std::string &filename = "./db.sqlite");
  virtual ~Database();

  double getLastMeasurement(uint64_t id);

  const std::vector<Measurement> &getMeasurementsCached(uint64_t id);

  const std::vector<Sensor> &getSensorsCached();
  const Sensor &getSensorByIdCached(uint64_t id);
  std::vector<Sensor> searchForSensors(std::string name, size_t limit);

  void addSensor(const Sensor &sensor);
  void addMeasurement(uint64_t id, const Measurement &measurement);

  uint64_t sensorFromUID(const std::string &dev_uuid);

  size_t getNumSensors();

private:
  void load();
  void onSensorBlockLoaded(const DataChunk &chunk, std::vector<char> *buffer);
  void onMeasurementBlockLoaded(const DataChunk &chunk, uint64_t table_id);

  void serializeSensor(const Sensor &sensor, std::vector<char> *buffer);
  void deserializeSensor(Sensor *sensor, const char *src);
  std::string deserializeString(const char *src, size_t *off);
  void serializeString(const std::string &data, std::vector<char> *buffer);

  // Returns the table id
  uint64_t addTable();

  // Creates a new block in the data file.
  uint64_t newFileBlock(void *data = nullptr);
  void writeFileChunk(void *data, uint64_t idx);

  void init_db();

  std::fstream _db_file;
  // The block idx and block data
  std::vector<PositionedIndexChunk> _index_chunks;
  // The block idx and block data
  std::vector<PositionedDataChunk> _table_last_chunks;

  JournalChunk _journal_chunk;

  // indices and caches
  std::vector<Sensor> _sensors;
  std::vector<std::vector<Measurement>> _measurements;
  std::unordered_map<std::string, uint64_t> _uuid_to_id;
  QGramIndex<3> _sensor_search_index;

  bool _use_journaling;
};
} // namespace smartwater
