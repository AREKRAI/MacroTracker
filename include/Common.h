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

#if defined(WINDOWS) || defined(_WIN32) || defined(_WIN64)
  #define APP_WINDOWS true
  #define CLEAR_CONSOLE_COMMAND "cls"
#else
    // Assume POSIX
    #define APP_POSIX true
    #define CLEAR_CONSOLE_COMMAND "clear"
#endif

#ifdef _DEBUG
  #define APP_DEBUG true
#endif

#define CLEAR_CONSOLE(...) system(CLEAR_CONSOLE_COMMAND)
#define ENDL ""

#define EXPR_STR(expr) #expr

#ifdef _MSC_VER
  #define DEBUG_BREAK() __debugbreak()
#else
  #error "No other compiler supported"
#endif

#include <log.h>

#ifdef APP_DEBUG
#define DEBUG_ASSERT(expr, ...) do {                                           \
  if (expr) break;                                                             \
  log_error("Expression (" EXPR_STR(expr) ") has failed: " ENDL, __VA_ARGS__); \
  DEBUG_BREAK();                                                               \
  } while (0)
#else
#define DEBUG_ASSERT(...)
#endif

#define COLOR_WHITE       { 1.f, 1.f, 1.f, 1.f }
#define COLOR_RED         { 1.f, 0.f, 0.f, 1.f }
#define COLOR_TRANSPARENT { 0.f, 0.f, 0.f, 0.f }
#define COLOR_BLACK       { 0.f, 0.f, 0.f, 1.f }

#define HEX_TO_FLOAT(x)   { (x >> 24) / 256.f, ((x << 8) >> 24) / 256.f, ((x << 16) >> 24) / 256.f, ((x << 24) >> 24) / 256.f }
#define RGB_HEX_TO_FLOAT(x) HEX_TO_FLOAT(x ## FF)

#define COLOR_BASE      RGB_HEX_TO_FLOAT(0x93CEE1)
#define COLOR_PRIMARY   RGB_HEX_TO_FLOAT(0xE193CE)
#define COLOR_SECONDARY RGB_HEX_TO_FLOAT(0xCEE193)

#endif