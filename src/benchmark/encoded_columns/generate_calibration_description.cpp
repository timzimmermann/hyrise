#include "generate_calibration_description.hpp"

#include <vector>

#include "utils/assert.hpp"

namespace opossum {

nlohmann::json generate_calibration_description(CalibrationType type) {
  const auto context = [&]() {
    switch (type) {
      case CalibrationType::CompleteTableScan:
        return nlohmann::json();
      case CalibrationType::FilteredTableScan:
        return nlohmann::json{{"null_fraction", 0.0f}};
      case CalibrationType::Materialization:
        return nlohmann::json{{"null_fraction", 0.0f}, {"selectivity", 1.0f}};
      default:
        Fail("Unrecognized type encountered.");
    }
  }();

  // Used for debugging ...
  // nlohmann::json context{
  //     {"row_count", 1'000'000},
  //     {"max_value", 4'095},
  //     {"null_fraction", 0.0},
  //     {"selectivity", 0.2},
  //     {"sorted", false}};

  // static const auto row_counts = std::vector<int>{2'000'000, 1'000'000, 500'000, 100'000};
  // static const auto max_values = std::vector<int>{63, 255, 1'023, 4'095, 16'382, 131'071, 262'143, 2'000'000};
  static const auto relative_selectivities = std::vector<float>{0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
  static const auto null_fractions = std::vector<float>{0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
  static const auto sorteds = std::vector<bool>{false, true};

  const auto point_access_factors = [&]() {
    switch (type) {
      case CalibrationType::FilteredTableScan:
        return std::vector<float>{0.05f, 0.075f, 0.10f, 0.125f, 0.15f, 0.175f, 0.20f};
      case CalibrationType::Materialization:
        return std::vector<float>{0.00001f, 0.00005f, 0.0001f, 0.0005f, 0.001f};
      default:
        Fail("Unrecognized type encountered.");
    }
  }();

  // Used for debugging ...
  static const auto row_counts = std::vector<int>{1'000'000};
  static const auto max_values = std::vector<int>{512};
  // static const auto null_fractions = std::vector<float>{0.2};
  // static const auto relative_selectivities = std::vector<float>{0.5};
  // static const auto sorteds = std::vector<bool>{false};
  static const auto encodings = std::vector<nlohmann::json>{
    {{"encoding_type", "Dictionary"}, {"vector_compression_type", "SIMD-BP128"}}};

  // static const auto encodings = std::vector<nlohmann::json>{
  //   {{"encoding_type", "Unencoded"}},
  //   {{"encoding_type", "Dictionary"}, {"vector_compression_type", "Fixed-size byte-aligned"}},
  //   {{"encoding_type", "Dictionary"}, {"vector_compression_type", "SIMD-BP128"}},
  //   {{"encoding_type", "Run Length"}}};

  nlohmann::json benchmarks;

  switch (type) {
    case CalibrationType::CompleteTableScan:
      for (auto encoding : encodings)
        for (auto row_count : row_counts)
          for (auto max_value : max_values)
            for (auto null_fraction : null_fractions)
              for (auto relative_selectivity : relative_selectivities)
                for (bool sorted : sorteds) {
                  const auto selectivity = (1.0f - null_fraction) * relative_selectivity;

                  auto benchmark = nlohmann::json{
                      {"row_count", row_count},
                      {"max_value", max_value},
                      {"null_fraction", null_fraction},
                      {"selectivity", selectivity},
                      {"sorted", sorted}};

                  benchmark.insert(encoding.begin(), encoding.end());
                  benchmarks.push_back(benchmark);
                }
      break;
    case CalibrationType::FilteredTableScan:
      for (auto encoding : encodings)
        for (auto row_count : row_counts)
          for (auto max_value : max_values)
            for (auto point_access_factor : point_access_factors)
              for (auto relative_selectivity : relative_selectivities)
                for (bool sorted : sorteds) {
                  const auto selectivity = relative_selectivity;

                  auto benchmark = nlohmann::json{
                      {"row_count", row_count},
                      {"max_value", max_value},
                      {"point_access_factor", point_access_factor},
                      {"selectivity", selectivity},
                      {"sorted", sorted}};

                  benchmark.insert(encoding.begin(), encoding.end());
                  benchmarks.push_back(benchmark);
                }
      break;
    case CalibrationType::Materialization:
      for (auto encoding : encodings)
          for (auto row_count : row_counts)
            for (auto max_value : max_values)
              for (auto point_access_factor : point_access_factors)
                for (bool sorted : sorteds) {
                  auto benchmark = nlohmann::json{
                      {"row_count", row_count},
                      {"max_value", max_value},
                      {"point_access_factor", point_access_factor},
                      {"sorted", sorted}};

                  benchmark.insert(encoding.begin(), encoding.end());
                  benchmarks.push_back(benchmark);
                }
      break;
    default:
      Fail("Unrecognized type encountered.");
  }

  return {{"context", context}, {"benchmarks", benchmarks}};
}

}  // namespace opossum
