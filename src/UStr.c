#include "UStr.h"

void UStr_init(UStr_t *self, const char *str) {
  *self = (UStr_t) {
    .count = 0,
    .str = malloc(DEFAULT_BUF_CAP),
    .cap = DEFAULT_BUF_CAP,
  };
  
  UStr_reset(self);
  UStr_appendLiteral(self, str);
}

void UStr_pushUC(UStr_t *self, UC_t uc) {
  self->count += 1;
  while (self->cap < self->count * sizeof(UC_t)) {
    self->cap <<= 1;
  }

  self->str = realloc(self->str, self->cap);
  self->str[self->count - 1] = uc;
  __UStr_terminate(self);
}

void __UStr_terminate(UStr_t *self) {
  while (self->cap < (self->count + 1) * sizeof(UC_t)) {
    self->cap <<= 1;
  }

  self->str = realloc(self->str, self->cap);
  self->str[self->count] = '\0';
}

void UStr_destroy(UStr_t *self) {
  free(self->str);
}

void UStr_appendLiteral(UStr_t *self, const char *literal) {
  UC_t code_point = 0;
  while ((code_point = decode_code_point(&literal)) != '\0') {
    UStr_pushUC(self, code_point);
  }
}

void UStr_append(UStr_t *self, UStr_t *other) {
  for (UC_t *uc = other->str; uc < &other->str[other->count]; uc++) {
    UStr_pushUC(self, *uc);
  }
}

void UStr_trimEnd(UStr_t *self, size_t len) {
  size_t trim = len > self->count ? self->count : len;
  self->count -= trim;
  __UStr_terminate(self);
}

#pragma region RIPPED_FROM_GITHUB //https://gist.github.com/tylerneylon/9773800

int decode_code_point(char **s) {
    int k = **s ? COUNT_LEADING_BITS(~(**s << 24)) : 0; // Count # of leading 1 bits.
    int mask = (1 << (8 - k)) - 1;                 // All 1s with k leading 0s.
    int value = **s & mask;
    // k = 0 for one-byte code points; otherwise, k = #total bytes.
    for (++(*s), --k; k > 0 && IS_CONT(**s); --k, ++(*s)) {
        value <<= 6;
        value += (**s & 0x3F);
    }
    return value;
}

// This assumes that `code` is <= 0x10FFFF and ensures that nothing will be
// written at or beyond `end`. It advances *s so it's easy to use in a loop.
void encode_code_point(char **s, char *end, int code) {
    char val[4];
    int lead_byte_max = 0x7F;
    int val_index = 0;
    while (code > lead_byte_max) {
        val[val_index++] = (code & 0x3F) | 0x80;
        code >>= 6;
        lead_byte_max >>= (val_index == 1 ? 2 : 1);
    }
    val[val_index++] = (code & lead_byte_max) | (~lead_byte_max << 1);
    while (val_index-- && *s < end) {
        **s = val[val_index];
        (*s)++;
    }
}

// This returns 0 if no split was needed.
int split_into_surrogates(int code, int *surr1, int *surr2) {
    if (code <= 0xFFFF) return 0;
    *surr2 = 0xDC00 | (code & 0x3FF);        // Save the low 10 bits.
    code >>= 10;                             // Drop the low 10 bits.
    // If `code` now has low bits "uuu uuxx xxxx", then the bits of *surr are
    // "1101 10ww wwxx xxxx" where wwww = (uuuuu - 1).
    *surr1 = 0xD800 | ((code & 0x7FF) - 0x40);
    return 1;
}

// This expects to be used in a loop and see all code points in *code. Start
// *old at 0; this function updates *old for you - don't change it after
// initialization. This returns 0 when *code is the 1st of a surrogate pair;
// otherwise use *code as the final code point.
int join_from_surrogates(int *old, int *code) {
    if (*old) *code = (((*old & 0x3FF) + 0x40) << 10) + (*code & 0x3FF);
    *old = ((*code & 0xD800) == 0xD800 ? *code : 0);
    return !(*old);
}

#pragma endregion RIPPED_FROM_GITHUB
