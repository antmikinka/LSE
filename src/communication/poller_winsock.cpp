// The Windows readiness seam: WSAPoll over a self-connected UDP loopback
// socket as the wake channel. Selected by CMake (elseif(WIN32)) exactly where
// poller_epoll.cpp is selected on Linux — same class, same contract, one file
// swapped.
//
// Two documented deviations from the epoll implementation:
//  * WSAPoll takes milliseconds; a nanosecond timeout is rounded UP so the
//    poller never wakes early (epoll_pwait2 is ns-precise).
//  * There is no epoll fd, so fd() returns the wake socket instead. It is a
//    genuinely waitable handle, which is the only property callers can rely
//    on; epoll_fd_ aliases wake_fd_ and the destructor closes it once.
#include <cstring>
#include <utility>
#include <vector>

#include "winsock_compat.hpp"

#include "lse/communication/poller.hpp"

namespace lse::comm {

namespace {

// Slot 0 of every WSAPoll call is the wake socket; the rest mirror interests_.
struct PendingEvent {
  int fd;
  short revents;  // NOLINT(google-runtime-int) — WSAPoll's own shape
};

short events_of(Interest i) noexcept {
  short mask = 0;
  if (wants_read(i)) mask |= POLLIN;
  if (wants_write(i)) mask |= POLLOUT;
  return mask;
}

}  // namespace

Result<Poller> Poller::create() {
  ws::ensure_started();
  Poller p;
  const int s = sock_socket(AF_INET, SOCK_DGRAM, 0);
  if (s < 0) {
    return LSE_ERROR(kIoError, "poll wake socket: ", std::strerror(errno));
  }
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;  // kernel picks a free port
  if (sock_bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    const std::string why = std::strerror(errno);
    sock_close(s);
    return LSE_ERROR(kIoError, "poll wake bind: ", why);
  }
  socklen_t len = sizeof(addr);
  addr = sockaddr_in{};
  if (sock_getsockname(s, reinterpret_cast<sockaddr*>(&addr), &len) != 0) {
    const std::string why = std::strerror(errno);
    sock_close(s);
    return LSE_ERROR(kIoError, "poll wake getsockname: ", why);
  }
  // Connect the socket to itself: send() needs no address and recv() only ever
  // sees this poller's own wake bytes.
  if (sock_connect(s, reinterpret_cast<sockaddr*>(&addr), len) != 0) {
    const std::string why = std::strerror(errno);
    sock_close(s);
    return LSE_ERROR(kIoError, "poll wake connect: ", why);
  }
  p.wake_fd_ = s;
  p.epoll_fd_ = s;  // aliased on purpose — see the file header.
  return p;
}

Poller::~Poller() {
  // epoll_fd_ aliases wake_fd_; close exactly once.
  if (wake_fd_ >= 0) sock_close(wake_fd_);
}

Poller::Poller(Poller&& other) noexcept
    : epoll_fd_(std::exchange(other.epoll_fd_, -1)),
      wake_fd_(std::exchange(other.wake_fd_, -1)),
      owners_(std::move(other.owners_)),
      scratch_(std::move(other.scratch_)),
      interests_(std::move(other.interests_)) {}

Poller& Poller::operator=(Poller&& other) noexcept {
  if (this != &other) {
    if (wake_fd_ >= 0) sock_close(wake_fd_);
    epoll_fd_ = std::exchange(other.epoll_fd_, -1);
    wake_fd_ = std::exchange(other.wake_fd_, -1);
    owners_ = std::move(other.owners_);
    scratch_ = std::move(other.scratch_);
    interests_ = std::move(other.interests_);
  }
  return *this;
}

Status Poller::add(int fd, Interest interest, IReady* owner) {
  if (interests_.count(fd) != 0) {
    // epoll answers EEXIST here; keep the shape, name the cause.
    return LSE_ERROR(kIoError, "poll add ", std::to_string(fd),
                     ": already registered");
  }
  interests_[fd] = interest;
  owners_[fd] = owner;
  return OkStatus();
}

Status Poller::modify(int fd, Interest interest) {
  const auto it = interests_.find(fd);
  if (it == interests_.end()) {
    return LSE_ERROR(kIoError, "poll modify ", std::to_string(fd),
                     ": not registered");
  }
  it->second = interest;
  return OkStatus();
}

void Poller::remove(int fd) noexcept {
  interests_.erase(fd);
  owners_.erase(fd);
}

Status Poller::wait(std::uint64_t timeout_ns) {
  const std::size_t n = interests_.size() + 1;  // + the wake slot
  scratch_.resize(n * sizeof(WSAPOLLFD));
  // Safe: vector storage comes from operator new, which is aligned for any
  // standard type; WSAPOLLFD's largest member is SOCKET (pointer-sized).
  auto* pfds = reinterpret_cast<WSAPOLLFD*>(scratch_.data());

  pfds[0].fd = ws::to_socket(wake_fd_);
  pfds[0].events = POLLIN;
  pfds[0].revents = 0;
  std::size_t slot = 1;
  for (const auto& [fd, interest] : interests_) {
    pfds[slot].fd = ws::to_socket(fd);
    pfds[slot].events = events_of(interest);
    pfds[slot].revents = 0;
    ++slot;
  }

  int timeout_ms = 0;
  if (timeout_ns != 0) {
    const std::uint64_t ms = (timeout_ns + 999'999ull) / 1'000'000ull;
    timeout_ms =
        ms > 0x7fffffffull ? 0x7fffffff : static_cast<int>(ms);  // round UP
  }

  const int ready = ::WSAPoll(pfds, static_cast<ULONG>(n), timeout_ms);
  if (ready == SOCKET_ERROR) {
    const int e = ws::set_errno_from_wsa();
    if (e == EINTR) return OkStatus();
    return LSE_ERROR(kIoError, "WSAPoll: ", std::strerror(e));
  }
  if (ready == 0) return OkStatus();

  if ((pfds[0].revents & POLLIN) != 0) {
    char drained[64];
    while (sock_recv(wake_fd_, drained, sizeof(drained), 0) > 0) {
    }
  }

  // Snapshot before dispatch: an on_ready callback may add/remove (mutating
  // interests_ and, on the next wait, scratch_) — ownership is re-looked-up
  // per event below, so a removed fd is skipped rather than called.
  std::vector<PendingEvent> pending;
  pending.reserve(static_cast<std::size_t>(ready));
  for (std::size_t i = 1; i < n; ++i) {
    if (pfds[i].revents == 0) continue;
    pending.push_back(PendingEvent{ws::from_socket(pfds[i].fd), pfds[i].revents});
  }

  for (const auto& ev : pending) {
    const auto it = owners_.find(ev.fd);
    if (it == owners_.end()) continue;
    Readiness r;
    r.readable = (ev.revents & POLLIN) != 0;
    r.writable = (ev.revents & POLLOUT) != 0;
    r.hangup = (ev.revents & (POLLHUP | POLLERR | POLLNVAL)) != 0;
    it->second->on_ready(ev.fd, r);
  }
  return OkStatus();
}

void Poller::wake() noexcept {
  if (wake_fd_ < 0) return;
  const char one = 1;
  const ssize_t written = sock_send(wake_fd_, &one, sizeof(one), 0);
  (void)written;
}

}  // namespace lse::comm
