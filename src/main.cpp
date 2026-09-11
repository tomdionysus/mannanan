#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include "manannan/config.h"
#include "manannan/loggers/logger_stdio.h"
#include "manannan/plugin_loader.h"
#include "manannan/service.h"
int main(int argc, char** argv) {
  if (argc != 2) { std::cerr << "usage: manannan <config.yaml>\n"; return 2; }
  auto logger = std::make_shared<manannan::loggers::LoggerStdIO>(manannan::loggers::INFO);
  try {
    const auto config = manannan::load_config(argv[1]);
    logger->set_level(config.log_level);
    manannan::PluginLoader loader(logger);
    loader.scan(config.plugin_directories);
    auto source_loaded = loader.create(manannan::PluginType::source, config.source_plugin, config.document);
    auto registry_loaded = loader.create(manannan::PluginType::registry, config.registry_plugin, config.document);
    auto* source = dynamic_cast<manannan::SourcePlugin*>(&source_loaded.instance());
    auto* registry = dynamic_cast<manannan::RegistryPlugin*>(&registry_loaded.instance());
    if (!source || !registry) throw std::runtime_error("plugin returned an object of the wrong type");
    logger->debug("Manannan started");
    return manannan::Service(*source, *registry, config.cadence, logger).run();
  } catch (const std::exception& error) { logger->error(error.what()); return 1; }
}
