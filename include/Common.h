#ifndef _H_COMMON_
#define _H_COMMON_

typedef enum _Result {
  RESULT_FAIL = 0,
  RESULT_SUCCESS = 1
} Result_t;

#define DEFAULT_BUF_CAP (1 << 10)

#if defined(_WIN64) || defined(_WIN32)
#define WINDOWS_PLATFORM
#else
#error "Linux and MacOS not supported"
#endif

#endif