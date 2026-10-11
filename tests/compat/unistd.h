// Shim: satisfies `#include <unistd.h>` in zero-touch test files on Windows.
// This directory enters the include path only under if(WIN32) (see
// lse_add_test in tests/CMakeLists.txt); POSIX builds never see it and get
// the real <unistd.h>.
#pragma once

#include "../compat.hpp"

// read/write/close/pipe/access/getpid arrive via <io.h>/<process.h> inside
// compat.hpp (POSIX names enabled by _CRT_NONSTDC_NO_DEPRECATE).
