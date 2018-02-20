#pragma once

#include "json.hpp"


namespace opossum {

enum class CalibrationType { CompleteTableScan, FilteredTableScan, Materialization };

nlohmann::json generate_calibration_description(CalibrationType type);

}
