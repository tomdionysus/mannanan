#include "manannan/config.h"
#include <sstream>
#include <stdexcept>
#include "manannan/duration.h"
namespace manannan {
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
  return config;
}
}

