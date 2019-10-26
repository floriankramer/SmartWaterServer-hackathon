#pragma once

#include "qgram.h"
#include "sensor.h"
#include <fstream>

#include <cstdint>
#include <sqlite3.h>
#include <unordered_map>
#include <vector>

namespace smartwater {
class Database {
  // The database is stored in a custom file. The file is composed of 4k chunks.
  // The first chunk is an index. A data chunk contains entries for exactly one
  // table.

  static const int CHUNK_SIZE = 4096;
  static const int NUM_TABLES_INDEX = 512 - 2;
  static const int NUM_DATA_BYTES = 4096 - 8 * 3 - 2;

  struct IndexChunk {
    uint64_t num_tables;
    uint64_t tables[NUM_TABLES_INDEX];
    uint64_t next_chunk;
  };

  struct PositionedIndexChunk {
    uint64_t idx;
    IndexChunk data;
  };

  struct DataChunk {
    uint64_t entry_size;
    uint64_t num_entries;
    uint16_t bytes_used;
    char data[NUM_DATA_BYTES];
    uint64_t next_chunk;
  };

  struct PositionedDataChunk {
    uint64_t idx;
    DataChunk data;
  };

public:
  Database(const std::string &filename = "./db.sqlite");
  virtual ~Database();

  double getLastMeasurement(uint64_t id);
  std::vector<Measurement> getMeasurements(uint64_t id, size_t t_start = 0,
                                           size_t t_end = -1,
                                           size_t limit = -1);

  const std::vector<Measurement> &getMeasurementsCached(uint64_t id);

  std::vector<Sensor> getSensors(size_t limit = -1, size_t offset = 0);
  const std::vector<Sensor> &getSensorsCached();
  const Sensor &getSensorByIdCached(uint64_t id);
  std::vector<Sensor> searchForSensors(std::string name, size_t limit);

  void addSensor(const Sensor &sensor);
  void addMeasurement(uint64_t id, const Measurement &measurement);

  uint64_t sensorFromUID(const std::string &dev_uuid);

private:
  void load();
  void onSensorBlockLoaded(const DataChunk &chunk);
  void onMeasurementBlockLoaded(const DataChunk &chunk, uint64_t table_id);

  // Returns the table id
  uint64_t addTable(size_t element_size);

  // Creates a new block in the data file.
  uint64_t newFileBlock(void *data = nullptr);

  void init_db();
  void init_indices();

  sqlite3 *_db;
  std::fstream _db_file;
  // The block idx and block data
  std::vector<PositionedIndexChunk> _index_chunks;
  // The block idx and block data
  std::vector<PositionedDataChunk> _table_last_chunks;

  // indices and caches
  std::vector<Sensor> _sensors;
  std::vector<std::vector<Measurement>> _measurements;
  std::unordered_map<std::string, uint64_t> _uuid_to_id;
  QGramIndex<3> _sensor_search_index;
};
} // namespace smartwater
