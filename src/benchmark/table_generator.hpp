#pragma once

#include <memory>

#include "types.hpp"

namespace opossum {

class Table;

class TableGenerator {
 public:
  std::shared_ptr<Table> get_table(const ChunkID chunk_size);
  std::shared_ptr<Table> get_table(const ChunkID chunk_size, size_t num_rows);

 protected:
  const size_t _num_columns = 10;
  const size_t _num_rows = 5 * 1000;
  const int _max_different_value = 1'000'000'000;
};

}  // namespace opossum
