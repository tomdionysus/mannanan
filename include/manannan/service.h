#pragma once
#include <chrono>
#include <memory>
#include "manannan/loggers/logger.h"
#include "manannan/plugin.h"
namespace manannan {
class Service {
 public:
  Service(SourcePlugin& source, RegistryPlugin& registry, std::chrono::milliseconds cadence,
          std::shared_ptr<loggers::Logger> logger);
  int run();
 private:
  void reconcile();
  SourcePlugin& source_private;
  RegistryPlugin& registry_private;
  std::chrono::milliseconds cadence_private;
  std::shared_ptr<loggers::Logger> logger_private;
};
}

