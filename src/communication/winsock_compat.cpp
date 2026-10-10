// The non-inline half of the socket seam: WSAStartup and the WSAGetLastError
// -> errno table. Empty on every other platform so the build lists one source
// set (same pattern as src/core/platform_win.cpp).
#include "winsock_compat.hpp"

#ifdef _WIN32

namespace lse::comm::ws {

int errno_of_wsa(int e) {
  switch (e) {
    case 0: return 0;
    case WSAEINTR: return EINTR;
    case WSAEACCES: return EACCES;
    case WSAEBADF: return EBADF;
    case WSAEFAULT: return EFAULT;
    case WSAEINVAL: return EINVAL;
    case WSAEMFILE: return EMFILE;
    case WSAEWOULDBLOCK: return EWOULDBLOCK;
    case WSAEINPROGRESS: return EINPROGRESS;
    case WSAEALREADY: return EALREADY;
    case WSAENOTSOCK: return ENOTSOCK;
    case WSAEDESTADDRREQ: return EDESTADDRREQ;
    case WSAEMSGSIZE: return EMSGSIZE;
    case WSAEPROTOTYPE: return EPROTOTYPE;
    case WSAENOPROTOOPT: return ENOPROTOOPT;
    case WSAEPROTONOSUPPORT: return EPROTONOSUPPORT;
    case WSAEOPNOTSUPP: return EOPNOTSUPP;
    case WSAEAFNOSUPPORT: return EAFNOSUPPORT;
    case WSAEADDRINUSE: return EADDRINUSE;
    case WSAEADDRNOTAVAIL: return EADDRNOTAVAIL;
    case WSAENETDOWN: return ENETDOWN;
    case WSAENETUNREACH: return ENETUNREACH;
    case WSAENETRESET: return ENETRESET;
    case WSAECONNABORTED: return ECONNABORTED;
    case WSAECONNRESET: return ECONNRESET;
    case WSAENOBUFS: return ENOBUFS;
    case WSAEISCONN: return EISCONN;
    case WSAENOTCONN: return ENOTCONN;
    case WSAETIMEDOUT: return ETIMEDOUT;
    case WSAECONNREFUSED: return ECONNREFUSED;
    case WSAEHOSTUNREACH: return EHOSTUNREACH;
    default: return EIO;
  }
}

int set_errno_from_wsa() {
  errno = errno_of_wsa(::WSAGetLastError());
  return errno;
}

void ensure_started() {
  // Magic static: one WSAStartup per process, thread-safe by the standard.
  // A failed startup leaves every later call failing with WSANOTINITIALISED,
  // which the table reports as EIO — honest, and never a crash.
  static const bool started = [] {
    WSADATA data{};
    return ::WSAStartup(MAKEWORD(2, 2), &data) == 0;
  }();
  (void)started;
}

}  // namespace lse::comm::ws

#endif  // _WIN32
