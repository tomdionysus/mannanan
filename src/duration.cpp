#include "manannan/duration.h"
#include <cctype>
#include <limits>
#include <stdexcept>
namespace manannan {
std::chrono::milliseconds parse_duration(const std::string& value) {
  if (value.empty()) throw std::invalid_argument("duration is empty");
  std::size_t split = 0;
  while (split < value.size() && std::isdigit(static_cast<unsigned char>(value[split]))) ++split;
  if (split == 0 || split == value.size()) throw std::invalid_argument("invalid duration: " + value);
  const auto number = std::stoull(value.substr(0, split));
  const auto unit = value.substr(split);
  std::uint64_t multiplier = 0;
  if (unit == "ms") multiplier = 1;
  else if (unit == "s") multiplier = 1000;
  else if (unit == "m") multiplier = 60'000;
  else if (unit == "h") multiplier = 3'600'000;
  else throw std::invalid_argument("unsupported duration unit: " + unit);
  if (number == 0 || number > std::numeric_limits<std::uint64_t>::max() / multiplier)
    throw std::invalid_argument("duration is zero or too large");
  return std::chrono::milliseconds(number * multiplier);
}
}

