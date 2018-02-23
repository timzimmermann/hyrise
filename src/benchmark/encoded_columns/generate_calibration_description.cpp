#include "generate_calibration_description.hpp"

#include <vector>
#include <map>

#include "utils/assert.hpp"

namespace opossum {

std::string to_string(CalibrationType type) {
  static const auto string_for_type = std::map<CalibrationType, std::string>{
    { CalibrationType::CompleteTableScan, "complete_table_scan" },
    { CalibrationType::FilteredTableScan, "filtered_table_scan" },
    { CalibrationType::Materialization, "materialization" }};

  return string_for_type.at(type);
}

nlohmann::json generate_calibration_description(CalibrationType type) {
  const auto context = [&]() {
    switch (type) {
      case CalibrationType::CompleteTableScan:
        return nlohmann::json{{"point_access_factor", 0.0f}};
      case CalibrationType::FilteredTableScan:
        return nlohmann::json{{"null_fraction", 0.0f}};
      case CalibrationType::Materialization:
        return nlohmann::json{{"null_fraction", 0.0f}};
      default:
        Fail("Unrecognized type encountered.");
    }
  }();

  const auto point_access_factors = [&]() {
    switch (type) {
      case CalibrationType::FilteredTableScan:
        return std::vector<float>{0.001f, 0.025f, 0.05f, 0.075f, 0.10f, 0.125f, 0.15f, 0.175f, 0.20f, 0.3f, 0.4f, 0.5f, 0.6f};
      case CalibrationType::Materialization:
        return std::vector<float>{0.00001f, 0.00005f, 0.0001f, 0.0005f, 0.001f};
      default:
        return std::vector<float>{};
    }
  }();

  const auto max_values = [&]() {
    auto v = std::vector<int>(20u);
    std::generate(v.begin(), v.end(), [n=2u]() mutable { return (1u << n++) - 1u; });
    return v;
  }();

  // static const auto max_values = std::vector<int>{7, 15, 31, 63, 127, 255, 511, 1'023, 4'095, 8'192, 16'382, 131'071, 262'143, 2'000'000};
  static const auto row_counts = std::vector<int>{1'000'000};
  static const auto null_fractions = std::vector<float>{0.0};
  static const auto sorteds = std::vector<bool>{false};

  static const auto encodings = std::vector<nlohmann::json>{
    /*{{"encoding_type", "Unencoded"}},*/
    {{"encoding_type", "Dictionary"}, {"vector_compression_type", "Fixed-size byte-aligned"}},
    {{"encoding_type", "Dictionary"}, {"vector_compression_type", "SIMD-BP128"}},
    /*{{"encoding_type", "Run Length"}}*/};

  nlohmann::json benchmarks;

  switch (type) {
    case CalibrationType::CompleteTableScan:
      for (auto encoding : encodings)
        for (auto row_count : row_counts)
          for (auto max_value : max_values)
            for (auto null_fraction : null_fractions)
              for (bool sorted : sorteds) {
                auto benchmark = nlohmann::json{
                    {"row_count", row_count},
                    {"max_value", max_value},
                    {"null_fraction", null_fraction},
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
