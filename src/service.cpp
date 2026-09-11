#include "manannan/service.h"
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <system_error>
#include <poll.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <sys/timerfd.h>
#include <unistd.h>
namespace manannan {
namespace {
class FileDescriptor {
 public:
  explicit FileDescriptor(int descriptor) : descriptor_private(descriptor) {
    if (descriptor_private < 0) throw std::system_error(errno, std::generic_category());
  }
  ~FileDescriptor() { if (descriptor_private >= 0) close(descriptor_private); }
  int get() const { return descriptor_private; }
 private: int descriptor_private;
};
itimerspec periodic_timer(std::chrono::milliseconds cadence) {
  const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(cadence);
  const auto remainder = cadence - seconds;
  timespec value{seconds.count(), std::chrono::duration_cast<std::chrono::nanoseconds>(remainder).count()};
  return {value, value};
}
}
Service::Service(SourcePlugin& source, RegistryPlugin& registry, std::chrono::milliseconds cadence,
                 std::shared_ptr<loggers::Logger> logger)
    : source_private(source), registry_private(registry), cadence_private(cadence), logger_private(std::move(logger)) {}
void Service::reconcile() {
  const auto desired = source_private.get_value();
  const auto registered = registry_private.get_value();
  if (desired == registered) { logger_private->debug("registered value is current: " + desired); return; }
  logger_private->info("registered value differs; applying " + desired);
  registry_private.set_value(desired);
}
int Service::run() {
  sigset_t signals; sigemptyset(&signals); sigaddset(&signals, SIGINT); sigaddset(&signals, SIGTERM);
  if (pthread_sigmask(SIG_BLOCK, &signals, nullptr) != 0) throw std::runtime_error("cannot block shutdown signals");
  FileDescriptor signal_fd(signalfd(-1, &signals, SFD_CLOEXEC | SFD_NONBLOCK));
  FileDescriptor timer_fd(timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC));
  const auto timer = periodic_timer(cadence_private);
  if (timerfd_settime(timer_fd.get(), 0, &timer, nullptr) < 0) throw std::system_error(errno, std::generic_category());
  try { reconcile(); } catch (const std::exception& error) { logger_private->error(error.what()); }
  pollfd descriptors[2]{{signal_fd.get(), POLLIN, 0}, {timer_fd.get(), POLLIN, 0}};
  while (true) {
    const int result = poll(descriptors, 2, -1);
    if (result < 0) { if (errno == EINTR) continue; throw std::system_error(errno, std::generic_category()); }
    if (descriptors[0].revents & POLLIN) { signalfd_siginfo info{}; (void)read(signal_fd.get(), &info, sizeof(info)); logger_private->info("shutdown requested"); return 0; }
    if (descriptors[1].revents & POLLIN) {
      std::uint64_t expirations{}; (void)read(timer_fd.get(), &expirations, sizeof(expirations));
      try { reconcile(); } catch (const std::exception& error) { logger_private->error(error.what()); }
    }
  }
}
}

