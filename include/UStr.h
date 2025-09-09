#ifndef _H_USTR_
#define _H_USTR_

#include <stdlib.h>
#include <string.h>
#include "Common.h"

// UNICODE CHARACTER
typedef unsigned int UC_t;

// UNUSED
typedef struct __UStrView_t {
  size_t count;
  UC_t *str;
} UStrView_t;

// UNUSED
typedef struct __UStr_t {
  size_t count;
  size_t cap;
  UC_t *str;
} UStr_t;

#define STR_TO_VIEW(str) ((StrView_t) { .count = str.count, .str = str.str })

void UStr_init(UStr_t *self, const char *str);
void UStr_pushUC(UStr_t *self, UC_t uc);
void __UStr_terminate(UStr_t *self);
void UStr_destroy(UStr_t *self);
inline void UStr_reset(UStr_t *self) {
  self->count = 0;
  self->str[self->count] = '\0';
}

void UStr_appendLiteral(UStr_t *self, const char *literal);
void UStr_append(UStr_t *self, UStr_t *other);
void UStr_trimEnd(UStr_t *self, size_t len);

#pragma region RIPPED_FROM_GITHUB //https://gist.github.com/tylerneylon/9773800

// This macro tests if a char is a continuation byte in utf8.
#define IS_CONT(x) (((x) & 0xc0) == 0x80)

// This returns the code point encoded at **s and advances *s to point to the
// next character. Thus it can easily be used in a loop.
#ifdef _MSC_VER
  #define COUNT_LEADING_BITS(x) __lzcnt(x)
#else
  #define COUNT_LEADING_BITS(x) __builtin_clz(x)
#endif

int decode_code_point(char **s);
void encode_code_point(char **s, char *end, int code);
int split_into_surrogates(int code, int *surr1, int *surr2);
int join_from_surrogates(int *old, int *code);
#pragma endregion RIPPED_FROM_GITHUB


#endif