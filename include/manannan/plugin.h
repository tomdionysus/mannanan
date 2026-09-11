#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <yaml-cpp/yaml.h>

#include "manannan/loggers/logger.h"

namespace manannan {

inline constexpr std::uint32_t plugin_abi_version = 1;

enum class PluginType : std::uint32_t { source = 1, registry = 2 };

class Plugin {
 public:
  virtual ~Plugin() = default;
};

class SourcePlugin : public Plugin {
 public:
  virtual std::string get_value() = 0;
};

class RegistryPlugin : public Plugin {
 public:
  virtual std::string get_value() = 0;
  virtual void set_value(const std::string& value) = 0;
};

using CreatePlugin = Plugin* (*)(const YAML::Node&, std::shared_ptr<loggers::Logger>);
using DestroyPlugin = void (*)(Plugin*);

struct PluginDescriptor {
  std::uint32_t abi_version;
  PluginType type;
  const char* name;
  const char* yaml_root;
  CreatePlugin create;
  DestroyPlugin destroy;
};

using GetPluginDescriptor = const PluginDescriptor* (*)();

}  // namespace manannan

#define MANANNAN_PLUGIN_ENTRYPOINT "manannan_plugin_descriptor"

