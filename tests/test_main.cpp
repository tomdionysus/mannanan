#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <yaml-cpp/yaml.h>
#include "manannan/config.h"
#include "manannan/duration.h"
int main() {
  using namespace std::chrono_literals;
  assert(manannan::parse_duration("250ms") == 250ms);
  assert(manannan::parse_duration("5m") == 5min);
  bool rejected = false; try { (void)manannan::parse_duration("0s"); } catch (...) { rejected = true; }
  assert(rejected);
  assert(manannan::parse_log_level("debug") == manannan::loggers::DEBUG);
  assert(manannan::parse_log_level("INFO") == manannan::loggers::INFO);
  assert(manannan::parse_log_level("warn") == manannan::loggers::WARN);
  assert(manannan::parse_log_level("error") == manannan::loggers::ERROR);
  rejected = false; try { (void)manannan::parse_log_level("verbose"); } catch (...) { rejected = true; }
  assert(rejected);
  const auto config_path = std::filesystem::path("manannan-test-config.yaml");
  {
    std::ofstream config_file(config_path);
    config_file << "manannan:\n"
                   "  cadence: 5m\n"
                   "  log_level: warn\n"
                   "  source: test_source\n"
                   "  registry: test_registry\n"
                   "  plugin_directories: [.]\n";
  }
  unsetenv("LOG_LEVEL");
  assert(manannan::load_config(config_path).log_level == manannan::loggers::WARN);
  setenv("LOG_LEVEL", "debug", 1);
  assert(manannan::load_config(config_path).log_level == manannan::loggers::DEBUG);
  unsetenv("LOG_LEVEL");
  std::filesystem::remove(config_path);
  const auto yaml = YAML::Load("source:\n  amazon_check_ip:\n    timeout: 10s\n");
  const auto node = manannan::resolve_yaml_root(yaml, "source.amazon_check_ip");
  assert(node["timeout"].as<std::string>() == "10s");
  assert(!manannan::resolve_yaml_root(yaml, "registry.missing"));
  std::cout << "all tests passed\n";
}
