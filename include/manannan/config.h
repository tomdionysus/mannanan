#pragma once
#include <chrono>
#include <filesystem>
#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>
namespace manannan {
struct Config {
  YAML::Node document;
  std::vector<std::filesystem::path> plugin_directories;
  std::string source_plugin;
  std::string registry_plugin;
  std::chrono::milliseconds cadence;
};
Config load_config(const std::filesystem::path& path);
YAML::Node resolve_yaml_root(const YAML::Node& document, const std::string& route);
}

