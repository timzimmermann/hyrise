#include <memory>

#include "benchmark/benchmark.h"

#include "../base_fixture.hpp"
#include "../table_generator.hpp"

#include "operators/table_scan.hpp"
#include "operators/table_wrapper.hpp"

namespace opossum {

BENCHMARK_DEFINE_F(BenchmarkBasicFixture, BM_JoinSortMerge1Partition)(benchmark::State& state) {
  clear_cache();
  auto scan = std::make_shared<TableScan>(_table_wrapper_a, ColumnName("a"), ScanType::OpGreaterThanEquals, 1000);
  scan->execute();
  auto join = std::make_shared<JoinSortMerge>();
}

BENCHMARK_REGISTER_F(BenchmarkBasicFixture, BM_JoinSortMerge1Partition)->Apply(BenchmarkBasicFixture::ChunkSizeIn);

}  // namespace opossum