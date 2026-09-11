#include <cassert>
#include <chrono>
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
  const auto yaml = YAML::Load("source:\n  amazon_check_ip:\n    timeout: 10s\n");
  const auto node = manannan::resolve_yaml_root(yaml, "source.amazon_check_ip");
  assert(node["timeout"].as<std::string>() == "10s");
  assert(!manannan::resolve_yaml_root(yaml, "registry.missing"));
  std::cout << "all tests passed\n";
}
