// The socket seam for the comm layer, mirroring what lse/core/platform.hpp
// does for files: one name per call on every platform, POSIX passes through
// unchanged, Windows maps to winsock.
//
// Deliberately NO function-like macros over POSIX names: #define close(x)
// would also rewrite member declarations and calls (StreamLink::close(),
// IListener::close()), so call sites name a sock_* wrapper once on every
// platform instead — the same rule platform.hpp states for file descriptors.
//
// Windows specifics the wrappers own (nothing above this header may know):
//  * SOCKET <-> int: winsock sockets are kernel handles, which fit in 32 bits,
//    and INVALID_SOCKET casts to exactly -1, matching the engine's "fd < 0 is
//    invalid" rule.
//  * WSAGetLastError() -> errno (winsock_compat.cpp owns the table).
//  * connect() on a non-blocking socket reports WSAEWOULDBLOCK where POSIX
//    reports EINPROGRESS; the wrapper translates so connect_impl's check
//    reads the same on both.
//  * getsockopt(SO_ERROR) yields a WSA* code; the wrapper converts the
//    retrieved value to an errno value so strerror() speaks one language.
//  * No SIGPIPE exists, so MSG_NOSIGNAL is 0 and sendmsg maps to WSASend
//    (WSASendMsg answers WSAEINVAL on stream sockets; see sock_sendmsg).
//  * SOCK_NONBLOCK/SOCK_CLOEXEC are not socket() type flags; the wrappers
//    strip them and apply FIONBIO (handles are non-inheritable by default on
//    every Windows this engine targets).
#pragma once

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <afunix.h>

#include <BaseTsd.h>

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <ctime>

// The engine spells readiness counts ssize_t and scatter types iovec/msghdr;
// on Windows those shapes live here. Nothing outside lse::comm sees them.
using ssize_t = SSIZE_T;

struct iovec {
  void* iov_base = nullptr;
  std::size_t iov_len = 0;
};

struct msghdr {
  void* msg_name = nullptr;
  socklen_t msg_namelen = 0;
  struct iovec* msg_iov = nullptr;
  // size_t, not int: stream_link.cpp assigns msg_iovlen from a size_t cast,
  // and this shape exists only so that line compiles unchanged here.
  std::size_t msg_iovlen = 0;
  void* msg_control = nullptr;
  socklen_t msg_controllen = 0;
  int msg_flags = 0;
};

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
#ifndef SHUT_WR
#define SHUT_WR SD_SEND
#endif
#ifndef SOCK_NONBLOCK
#define SOCK_NONBLOCK 0x20000000
#endif
#ifndef SOCK_CLOEXEC
#define SOCK_CLOEXEC 0x40000000
#endif

#else  // !_WIN32

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <ctime>

#endif  // _WIN32

namespace lse::comm {

#ifdef _WIN32

namespace ws {

// WSAStartup once per process; safe from any wrapper on any thread.
void ensure_started();
// WSAGetLastError()/WSA* code -> errno value. Pure mapping, sets nothing.
int errno_of_wsa(int wsa_error);
// errno = errno_of_wsa(WSAGetLastError()); returns the value set.
int set_errno_from_wsa();

inline SOCKET to_socket(int fd) noexcept {
  return static_cast<SOCKET>(static_cast<UINT_PTR>(static_cast<unsigned>(fd)));
}
inline int from_socket(SOCKET s) noexcept {
  return static_cast<int>(static_cast<unsigned>(s & 0xFFFFFFFFull));
}

}  // namespace ws

inline void sock_startup() { ws::ensure_started(); }

inline int sock_socket(int family, int type, int protocol) {
  ws::ensure_started();
  const SOCKET s =
      ::socket(family, type & ~(SOCK_NONBLOCK | SOCK_CLOEXEC), protocol);
  if (s == INVALID_SOCKET) {
    ws::set_errno_from_wsa();
    return -1;
  }
  if ((type & SOCK_NONBLOCK) != 0) {
    u_long nonblock = 1;
    // FIONBIO expands to an unsigned long; ioctlsocket wants a long. Same
    // bits, named once so the SDK macro does not spray sign-conversion noise.
    if (::ioctlsocket(s, static_cast<long>(FIONBIO), &nonblock) != 0) {
      const int e = ws::set_errno_from_wsa();
      ::closesocket(s);
      errno = e;
      return -1;
    }
  }
  return ws::from_socket(s);
}

inline int sock_accept4(int fd, sockaddr* addr, socklen_t* len, int flags) {
  const SOCKET s = ::accept(ws::to_socket(fd), addr, len);
  if (s == INVALID_SOCKET) {
    ws::set_errno_from_wsa();
    return -1;
  }
  if ((flags & SOCK_NONBLOCK) != 0) {
    u_long nonblock = 1;
    // FIONBIO expands to an unsigned long; ioctlsocket wants a long. Same
    // bits, named once so the SDK macro does not spray sign-conversion noise.
    if (::ioctlsocket(s, static_cast<long>(FIONBIO), &nonblock) != 0) {
      const int e = ws::set_errno_from_wsa();
      ::closesocket(s);
      errno = e;
      return -1;
    }
  }
  return ws::from_socket(s);
}

inline int sock_close(int fd) {
  if (::closesocket(ws::to_socket(fd)) == 0) return 0;
  ws::set_errno_from_wsa();
  return -1;
}

inline ssize_t sock_recv(int fd, void* buf, std::size_t len, int flags) {
  const int n = ::recv(ws::to_socket(fd), static_cast<char*>(buf),
                       static_cast<int>(len), flags);
  if (n == SOCKET_ERROR) {
    ws::set_errno_from_wsa();
    return -1;
  }
  return static_cast<ssize_t>(n);
}

inline ssize_t sock_send(int fd, const void* buf, std::size_t len, int flags) {
  (void)flags;  // only MSG_NOSIGNAL arrives here, and Windows has no SIGPIPE
  const int n = ::send(ws::to_socket(fd), static_cast<const char*>(buf),
                       static_cast<int>(len), 0);
  if (n == SOCKET_ERROR) {
    ws::set_errno_from_wsa();
    return -1;
  }
  return static_cast<ssize_t>(n);
}

inline ssize_t sock_sendmsg(int fd, const msghdr* msg, int flags) {
  (void)flags;
  // The engine's only sendmsg is a two-entry scatter (frame header + payload)
  // on a connected stream socket. WSASendMsg is the datagram/control-data
  // path: on Windows it refuses stream sockets with WSAEINVAL (verified on
  // 10.0.26100 for blocking and non-blocking alike, while the same call on a
  // UDP socket succeeds), so the scatter goes to WSASend, which takes the
  // same WSABUF array and keeps the partial-write semantics the caller
  // splits on.
  constexpr std::size_t kMaxIov = 16;
  WSABUF bufs[kMaxIov];
  const std::size_t n = msg->msg_iovlen < kMaxIov ? msg->msg_iovlen : kMaxIov;
  for (std::size_t i = 0; i < n; ++i) {
    bufs[i].buf = static_cast<char*>(msg->msg_iov[i].iov_base);
    bufs[i].len = static_cast<ULONG>(msg->msg_iov[i].iov_len);
  }
  DWORD sent = 0;
  if (::WSASend(ws::to_socket(fd), bufs, static_cast<DWORD>(n), &sent, 0,
                nullptr, nullptr) == SOCKET_ERROR) {
    ws::set_errno_from_wsa();
    return -1;
  }
  return static_cast<ssize_t>(sent);
}

inline int sock_shutdown(int fd, int how) {
  if (::shutdown(ws::to_socket(fd), how) == 0) return 0;
  ws::set_errno_from_wsa();
  return -1;
}

inline int sock_bind(int fd, const sockaddr* addr, socklen_t len) {
  if (::bind(ws::to_socket(fd), addr, len) == 0) return 0;
  ws::set_errno_from_wsa();
  return -1;
}

inline int sock_connect(int fd, const sockaddr* addr, socklen_t len) {
  if (::connect(ws::to_socket(fd), addr, len) == 0) return 0;
  const int e = ::WSAGetLastError();
  // A non-blocking connect in progress is EINPROGRESS on POSIX and
  // WSAEWOULDBLOCK here; the caller branches on EINPROGRESS.
  errno = (e == WSAEWOULDBLOCK) ? EINPROGRESS : ws::errno_of_wsa(e);
  return -1;
}

inline int sock_listen(int fd, int backlog) {
  if (::listen(ws::to_socket(fd), backlog) == 0) return 0;
  ws::set_errno_from_wsa();
  return -1;
}

inline int sock_getsockopt(int fd, int level, int option, void* value,
                           socklen_t* len) {
  if (::getsockopt(ws::to_socket(fd), level, option,
                   static_cast<char*>(value), len) != 0) {
    ws::set_errno_from_wsa();
    return -1;
  }
  if (level == SOL_SOCKET && option == SO_ERROR && len != nullptr &&
      *len >= static_cast<socklen_t>(sizeof(int))) {
    int* out = static_cast<int*>(value);
    if (*out != 0) *out = ws::errno_of_wsa(*out);
  }
  return 0;
}

inline int sock_setsockopt(int fd, int level, int option, const void* value,
                           socklen_t len) {
  if (::setsockopt(ws::to_socket(fd), level, option,
                   static_cast<const char*>(value), len) == 0) {
    return 0;
  }
  ws::set_errno_from_wsa();
  return -1;
}

inline int sock_getpeername(int fd, sockaddr* addr, socklen_t* len) {
  if (::getpeername(ws::to_socket(fd), addr, len) == 0) return 0;
  ws::set_errno_from_wsa();
  return -1;
}

inline int sock_getsockname(int fd, sockaddr* addr, socklen_t* len) {
  if (::getsockname(ws::to_socket(fd), addr, len) == 0) return 0;
  ws::set_errno_from_wsa();
  return -1;
}

// A bounded pause, so endpoint.cpp's resolve retry loop names one function on
// every platform (nanosleep does not exist on Windows).
inline void comm_nap_ns(std::uint64_t ns) {
  const std::uint64_t ms = (ns + 999'999ull) / 1'000'000ull;
  // Capped below INFINITE (0xffffffff): Sleep(INFINITE) means "never return".
  const std::uint64_t capped = ms < 0xffffffffull ? ms : 0xfffffffeull;
  ::Sleep(static_cast<DWORD>(capped));
}

#else  // !_WIN32 — pass-throughs, byte-for-byte the calls this replaces.

inline void sock_startup() {}

inline int sock_socket(int family, int type, int protocol) {
  return ::socket(family, type, protocol);
}

inline int sock_accept4(int fd, sockaddr* addr, socklen_t* len, int flags) {
  return ::accept4(fd, addr, len, flags);
}

inline int sock_close(int fd) { return ::close(fd); }

inline ssize_t sock_recv(int fd, void* buf, std::size_t len, int flags) {
  return ::recv(fd, buf, len, flags);
}

inline ssize_t sock_send(int fd, const void* buf, std::size_t len, int flags) {
  return ::send(fd, buf, len, flags);
}

inline ssize_t sock_sendmsg(int fd, const msghdr* msg, int flags) {
  return ::sendmsg(fd, msg, flags);
}

inline int sock_shutdown(int fd, int how) { return ::shutdown(fd, how); }

inline int sock_bind(int fd, const sockaddr* addr, socklen_t len) {
  return ::bind(fd, addr, len);
}

inline int sock_connect(int fd, const sockaddr* addr, socklen_t len) {
  return ::connect(fd, addr, len);
}

inline int sock_listen(int fd, int backlog) { return ::listen(fd, backlog); }

inline int sock_getsockopt(int fd, int level, int option, void* value,
                           socklen_t* len) {
  return ::getsockopt(fd, level, option, value, len);
}

inline int sock_setsockopt(int fd, int level, int option, const void* value,
                           socklen_t len) {
  return ::setsockopt(fd, level, option, value, len);
}

inline int sock_getpeername(int fd, sockaddr* addr, socklen_t* len) {
  return ::getpeername(fd, addr, len);
}

inline int sock_getsockname(int fd, sockaddr* addr, socklen_t* len) {
  return ::getsockname(fd, addr, len);
}

inline void comm_nap_ns(std::uint64_t ns) {
  const timespec nap{static_cast<time_t>(ns / 1'000'000'000ull),
                     static_cast<long>(ns % 1'000'000'000ull)};
  ::nanosleep(&nap, nullptr);
}

#endif  // _WIN32

}  // namespace lse::comm
