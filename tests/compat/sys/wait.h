// Shim: satisfies `#include <sys/wait.h>` on Windows. The status word here
// is the raw DWORD from GetExitCodeProcess, so the macros are degenerate:
// a Windows child always "exited" — there is no signal-death encoding.
// Delta: WIFSIGNALED is always false and WTERMSIG always 0; callers needing
// signal-death semantics (test_server_shutdown case 1) are ported in-file
// to the CreateProcess doctrine (S4 §6 P2 / ID-C2-8), not served by this
// shim.
#pragma once

#include "../../compat.hpp"  // pid_t

#ifndef WIFEXITED
#define WIFEXITED(status) (1)
#endif
#ifndef WEXITSTATUS
#define WEXITSTATUS(status) (static_cast<int>(status))
#endif
#ifndef WIFSIGNALED
#define WIFSIGNALED(status) (0)
#endif
#ifndef WTERMSIG
#define WTERMSIG(status) (0)
#endif
