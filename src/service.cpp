#include "manannan/service.h"
#include <cerrno>
#include <csignal>
#include <stdexcept>
#include <system_error>
#include <sys/select.h>
namespace manannan {
namespace {
volatile std::sig_atomic_t shutdown_requested{};

extern "C" void request_shutdown(int) {
  shutdown_requested = 1;
}

timespec to_timespec(std::chrono::steady_clock::duration duration) {
  const auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
  const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(nanoseconds);
  return {seconds.count(), (nanoseconds - seconds).count()};
}
}
Service::Service(SourcePlugin& source, RegistryPlugin& registry, std::chrono::milliseconds cadence,
                 std::shared_ptr<loggers::Logger> logger)
    : source_private(source), registry_private(registry), cadence_private(cadence), logger_private(std::move(logger)) {}
void Service::reconcile() {
  logger_private->debug("checking public IP");
  logger_private->debug("loading IP from source");
  const auto desired = source_private.get_value();
  logger_private->debug("checking registered IP");
  logger_private->debug("loading IP from registry");
  const auto registered = registry_private.get_value();
  if (desired == registered) { logger_private->debug("registered value is current: " + desired); return; }
  logger_private->debug("registered value differs; setting " + desired + " in registry");
  registry_private.set_value(desired);
  logger_private->info("set new IP to " + desired);
}
int Service::run() {
  shutdown_requested = 0;
  struct sigaction action {};
  action.sa_handler = request_shutdown;
  sigemptyset(&action.sa_mask);
  if (sigaction(SIGINT, &action, nullptr) < 0 || sigaction(SIGTERM, &action, nullptr) < 0) {
    throw std::system_error(errno, std::generic_category());
  }

  sigset_t shutdown_signals;
  sigemptyset(&shutdown_signals);
  sigaddset(&shutdown_signals, SIGINT);
  sigaddset(&shutdown_signals, SIGTERM);
  sigset_t wait_mask;
  if (sigprocmask(SIG_BLOCK, &shutdown_signals, &wait_mask) < 0) {
    throw std::system_error(errno, std::generic_category());
  }

  try { reconcile(); } catch (const std::exception& error) { logger_private->error(error.what()); }

  auto next_run = std::chrono::steady_clock::now() + cadence_private;
  while (!shutdown_requested) {
    const auto now = std::chrono::steady_clock::now();
    if (now >= next_run) {
      try { reconcile(); } catch (const std::exception& error) { logger_private->error(error.what()); }
      next_run = std::chrono::steady_clock::now() + cadence_private;
      continue;
    }

    const auto timeout = to_timespec(next_run - now);
    const int result = pselect(0, nullptr, nullptr, nullptr, &timeout, &wait_mask);
    if (result < 0 && errno != EINTR) throw std::system_error(errno, std::generic_category());
  }
  logger_private->debug("shutdown requested");
  return 0;
}
}
