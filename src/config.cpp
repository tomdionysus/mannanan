#include "manannan/config.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include "manannan/duration.h"
namespace manannan {
loggers::LogLevel parse_log_level(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
  if (value == "debug") return loggers::DEBUG;
  if (value == "info") return loggers::INFO;
  if (value == "warn") return loggers::WARN;
  if (value == "error") return loggers::ERROR;
  throw std::runtime_error("invalid log level '" + value + "'; expected debug, info, warn, or error");
}

YAML::Node resolve_yaml_root(const YAML::Node& document, const std::string& route) {
  YAML::Node node = document;
  std::istringstream parts(route);
  std::string part;
  while (std::getline(parts, part, '.')) {
    if (part.empty() || !node || !node.IsMap()) return {};
    node.reset(node[part]);
  }
  return node;
}
Config load_config(const std::filesystem::path& path) {
  Config config;
  config.document = YAML::LoadFile(path.string());
  const auto service = config.document["manannan"];
  if (!service || !service.IsMap()) throw std::runtime_error("missing manannan configuration root");
  config.cadence = parse_duration(service["cadence"].as<std::string>());
  for (const auto& directory : service["plugin_directories"])
    config.plugin_directories.emplace_back(directory.as<std::string>());
  if (config.plugin_directories.empty()) throw std::runtime_error("no plugin directories configured");
  config.source_plugin = service["source"].as<std::string>();
  config.registry_plugin = service["registry"].as<std::string>();
  config.log_level = parse_log_level(service["log_level"].as<std::string>("info"));
  if (const auto* environment_level = std::getenv("LOG_LEVEL"))
    config.log_level = parse_log_level(environment_level);
  return config;
}
}
