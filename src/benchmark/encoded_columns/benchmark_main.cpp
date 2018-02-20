#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>

#include "single_distribution_benchmark.hpp"
#include "multi_distribution_column_benchmark.hpp"
#include "generate_calibration_description.hpp"

int main(int argc, char const *argv[]) {
  nlohmann::json description;

  if (argc > 1) {
    const auto file_name = argv[1];

    std::cout << file_name << std::endl;

    std::ifstream file_stream{file_name};
    file_stream >> description;
  } else {
    description = opossum::generate_calibration_description(opossum::CalibrationType::Materialization);
  }

  auto benchmark = opossum::MultiDistributionColumnBenchmark{std::move(description)};
  benchmark.run();

  // auto benchmark = opossum::SingleDistributionBenchmark{};
  // benchmark.run();

  return 0;
}
