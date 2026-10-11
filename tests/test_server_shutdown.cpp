// CPU-only lifecycle tests: no model, backend, runtime, or device is opened.
#ifndef _WIN32  // cpp-httplib's poll() path is POSIX-only; Windows uses select().
#define CPPHTTPLIB_USE_POLL
#endif
#include "httplib.h"
#include "lse/server/shutdown.hpp"
#ifdef _WIN32
// P2 port (ID-C2-8): Windows has no fork. The stuck-handler child is a
// CreateProcess self-relaunch of this binary (--child-stuck <handle>), with
// the marker pipe's write end duplicated inheritable and handed over as a
// numeric handle value (inheritance preserves the value; the child re-wraps
// it with _open_osfhandle). The mandatory positive control (--child-clean)
// runs the SAME pipe + destructor path without the stuck watch and must
// deliver the byte — otherwise "marker empty" in the stuck case could pass
// on a dead pipe (anti-vacuity).
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <cstdlib>
#include <cstring>
#include <string>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif
#include <atomic>
#include <chrono>
#include <cstdio>
#include <future>
#include <thread>

using namespace std::chrono_literals;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); std::abort(); } } while (false)

template <class Predicate> void wait_for(Predicate pred) {
  const auto limit = std::chrono::steady_clock::now() + 3s;
  while (!pred()) { CHECK(std::chrono::steady_clock::now() < limit); std::this_thread::sleep_for(1ms); }
}

#ifdef _WIN32
namespace {

// The child half of the marker-pipe protocol: identical Resource destructor
// in both modes; only the stuck watch differs.
struct Resource {
  int fd;
  ~Resource() { const char c = 'x'; (void)::_write(fd, &c, 1); }
};

int child_main(const char* mode, const char* handle_text) {
  const auto handle = reinterpret_cast<HANDLE>(
      static_cast<intptr_t>(std::strtoull(handle_text, nullptr, 10)));
  const int fd = ::_open_osfhandle(reinterpret_cast<intptr_t>(handle), 0);
  CHECK(fd >= 0);
  Resource resource{fd};
  if (std::strcmp(mode, "--child-clean") == 0) {
    return 0;  // destructor runs at return: the byte MUST reach the parent
  }
  std::atomic<bool> request{true};
  lse::server::detail::ShutdownWatch watch(request, 80ms, [] {});
  std::this_thread::sleep_for(5s);
  return 99;  // never reached: the watch _Exit(1)s at ~80ms
}

// Spawn this binary in `mode`, wait for it, and return the marker read fd.
int spawn_child(const char* mode, DWORD* exit_code) {
  int marker[2];
  CHECK(::_pipe(marker, 4096, _O_BINARY) == 0);
  HANDLE write_handle =
      reinterpret_cast<HANDLE>(::_get_osfhandle(marker[1]));
  HANDLE inherited = nullptr;
  CHECK(::DuplicateHandle(::GetCurrentProcess(), write_handle,
                          ::GetCurrentProcess(), &inherited, 0, TRUE,
                          DUPLICATE_SAME_ACCESS) != 0);
  CHECK(::_close(marker[1]) == 0);  // the duplicate is the only write end now

  wchar_t exe[MAX_PATH + 1]{};
  CHECK(::GetModuleFileNameW(nullptr, exe, MAX_PATH) != 0);
  const std::wstring wmode(mode, mode + std::strlen(mode));
  std::wstring cmd = std::wstring(L"\"") + exe + L"\" " + wmode + L" " +
                     std::to_wstring(
                         static_cast<unsigned long long>(
                             reinterpret_cast<intptr_t>(inherited)));
  STARTUPINFOW si{};
  si.cb = sizeof si;
  PROCESS_INFORMATION pi{};
  CHECK(::CreateProcessW(exe, cmd.data(), nullptr, nullptr, TRUE, 0, nullptr,
                         nullptr, &si, &pi) != 0);
  ::CloseHandle(pi.hThread);
  CHECK(::WaitForSingleObject(pi.hProcess, 20000) == WAIT_OBJECT_0);
  CHECK(::GetExitCodeProcess(pi.hProcess, exit_code) != 0);
  ::CloseHandle(pi.hProcess);
  ::CloseHandle(inherited);
  return marker[0];
}

}  // namespace

int main(int argc, char** argv) {
  if (argc >= 3 && (std::strcmp(argv[1], "--child-stuck") == 0 ||
                    std::strcmp(argv[1], "--child-clean") == 0))
    return child_main(argv[1], argv[2]);

  // Positive control first: the clean child's destructor must deliver the
  // byte over the exact pipe mechanism the stuck case asserts over.
  DWORD clean_exit = ~0u;
  const int clean_fd = spawn_child("--child-clean", &clean_exit);
  CHECK(clean_exit == 0);
  char probe = 0;
  CHECK(::_read(clean_fd, &probe, 1) == 1 && probe == 'x');
  CHECK(::_close(clean_fd) == 0);

  // A stuck handler must produce a failing exit without running resource
  // destructors: exit 1 from _Exit, and the marker pipe stays empty.
  DWORD stuck_exit = ~0u;
  const int stuck_fd = spawn_child("--child-stuck", &stuck_exit);
  CHECK(stuck_exit == 1);
  char c = 0;
  CHECK(::_read(stuck_fd, &c, 1) == 0);  // EOF: no destructor byte
  CHECK(::_close(stuck_fd) == 0);
#else
int main() {
  // A stuck handler must produce a failing exit without running resource
  // destructors. Run before creating threads so fork inherits no held locks.
  int marker[2]; CHECK(pipe(marker) == 0);
  const pid_t child = fork(); CHECK(child >= 0);
  if (child == 0) {
    close(marker[0]);
    struct Resource { int fd; ~Resource() { const char c = 'x'; (void)write(fd, &c, 1); } } resource{marker[1]};
    std::atomic<bool> request{true};
    lse::server::detail::ShutdownWatch watch(request, 80ms, [] {});
    std::this_thread::sleep_for(5s);
    return 99;
  }
  close(marker[1]);
  int status = 0; CHECK(waitpid(child, &status, 0) == child);
  CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 1);
  char c = 0; CHECK(read(marker[0], &c, 1) == 0); close(marker[0]);
#endif

  // Finishing normally (including bind failure) must join a watcher even when
  // no shutdown signal ever arrived.
  {
    std::atomic<bool> request{false};
    int stops = 0;
    lse::server::detail::ShutdownWatch watch(request, 1s, [&] { ++stops; });
    watch.finish(); CHECK(stops == 0);
  }

  // Shutdown requested before listen starts is retried after readiness.
  {
    httplib::Server server;
    const int port = server.bind_to_any_port("127.0.0.1"); CHECK(port > 0);
    std::atomic<bool> request{true}, issued{false};
    lse::server::detail::ShutdownWatch watch(request, 2s, [&] {
      lse::server::detail::stop_listener_once(server, issued);
    });
    std::this_thread::sleep_for(30ms); CHECK(!issued.load());
    CHECK(server.listen_after_bind());
    watch.finish(); CHECK(issued.load());
  }

  // Real socket worker stays active beyond several watchdog polls. Listening
  // cannot finish (and backing cannot be destroyed) until that handler exits.
  {
    httplib::Server server;
    server.new_task_queue = [] { return new httplib::ThreadPool(2); };
    std::atomic<bool> entered{false}, release{false}, returned{false};
    std::atomic<bool> request{false}, issued{false};
    std::atomic<int> stops{0};
    server.Get("/hold", [&](const httplib::Request&, httplib::Response& res) {
      entered.store(true);
      while (!release.load()) std::this_thread::sleep_for(1ms);
      res.set_content("drained", "text/plain");
    });
    const int port = server.bind_to_any_port("127.0.0.1"); CHECK(port > 0);
    std::thread listener([&] { CHECK(server.listen_after_bind()); returned.store(true); });
    wait_for([&] { return server.is_running(); });
    auto client = std::async(std::launch::async, [&] {
      httplib::Client http("127.0.0.1", port);
      auto result = http.Get("/hold"); CHECK(result && result->body == "drained");
    });
    wait_for([&] { return entered.load(); });
    lse::server::detail::ShutdownWatch watch(request, 2s, [&] {
      ++stops;
      lse::server::detail::stop_listener_once(server, issued);
    });
    request.store(true);
    wait_for([&] { return issued.load(); });
    // Wait for actual watchdog progress; a fixed sleep does not guarantee
    // another thread receives three time slices on a loaded CI runner.
    wait_for([&] {
      CHECK(!returned.load());
      return stops.load() >= 3;
    });
    CHECK(stops.load() >= 3); CHECK(!returned.load());
    release.store(true); client.get(); listener.join(); watch.finish();
    CHECK(returned.load());
  }
  std::puts("server shutdown: timeout, normal return, startup race, socket worker drain PASS");
}
