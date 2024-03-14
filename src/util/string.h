

#ifndef __LIBBLU_MUXER__UTIL__STRING_H__
#define __LIBBLU_MUXER__UTIL__STRING_H__

#include "macros.h"

#include <stdbool.h>
#include <stddef.h>

typedef unsigned char lbc;
typedef unsigned int lbc_int;

#define PRI_LBC  "c"
#define PRI_LBCS  "s"
#define PRI_LBCP  ""

#define lbc_str(s) (unsigned char *) s
#define lbc_char(c)  c

#if defined(ARCH_WIN32)
/* ### Windows-API specific ################################################ */

wchar_t *winUtf8ToWideChar(
  const lbc *string
);

lbc *winWideCharToUtf8(
  const wchar_t *string
);

#endif

/* ### <stdlib.h> ########################################################## */

double lbc_strtod(
  const lbc *nptr,
  lbc **endptr
);

float lbc_strtof(
  const lbc *restrict nptr,
  lbc **restrict endptr
);

long double lbc_strtold(
  const lbc *nptr,
  lbc **endptr
);

unsigned long lbc_strtoul(
  const lbc *restrict nptr,
  lbc ** restrict endptr,
  int base
);

unsigned long long int lbc_strtoull(
  const lbc *nptr,
  lbc **endptr,
  int base
);

long int lbc_strtol(
  const lbc *nptr,
  lbc **endptr,
  int base
);

long long int lbc_strtoll(
  const lbc *nptr,
  lbc **endptr,
  int base
);

/* ### <stdio.h> ########################################################### */

#include <stdio.h>

FILE *lbc_fopen(
  const lbc *restrict pathname,
  const char *restrict mode
);

int lbc_printf(
  const lbc *format,
  ...
);

int lbc_fprintf(
  FILE *stream,
  const lbc *format,
  ...
);

int lbc_sprintf(
  lbc *str,
  const lbc *format,
  ...
);

int lbc_snprintf(
  lbc *str,
  size_t size,
  const lbc *format,
  ...
);

lbc *lbc_fgets(
  lbc *s,
  int size,
  FILE *stream
);

int lbc_scanf(
  const lbc *format,
  ...
);

int lbc_fscanf(
  FILE *stream,
  const lbc *format,
  ...
);

int lbc_sscanf(
  const lbc *str,
  const lbc *format,
  ...
);

/* ### <stdarg.h> ########################################################## */

#if defined(LBC_VARIADIC) || defined(LBC_IMPLEMENTATION)

int lbc_vprintf(
  const lbc *format,
  va_list ap
);

int lbc_vfprintf(
  FILE *stream,
  const lbc *format,
  va_list ap
);

int lbc_vsprintf(
  lbc *str,
  const lbc *format,
  va_list ap
);

int lbc_vsnprintf(
  lbc *str,
  size_t size,
  const lbc *format,
  va_list ap
);

int lbc_vscanf(
  const lbc *format,
  va_list ap
);

int lbc_vsscanf(
  const lbc *str,
  const lbc *format,
  va_list ap
);

int lbc_vfscanf(
  FILE *stream,
  const lbc *format,
  va_list ap
);

#endif

/* ### <string.h> ########################################################## */

size_t lbc_strlen(
  const lbc *str
);

int lbc_strcmp(
  const lbc *str1,
  const lbc *str2
);

int lbc_strncmp(
  const lbc *str1,
  const lbc *str2,
  size_t size
);

lbc *lbc_strcpy(
  lbc *restrict dst,
  const lbc *restrict src
);

lbc *lbc_strncpy(
  lbc *restrict dst,
  const lbc *restrict src,
  size_t size
);

void lbc_strncat(
  lbc *s1,
  const lbc *s2,
  size_t size
);

lbc *lbc_strdup(
  const lbc *str
);

lbc *lbc_strndup(
  const lbc *str,
  size_t size
);

size_t lbc_strspn(
  const lbc *s,
  const lbc *accept
);

size_t lbc_strcspn(
  const lbc *s,
  const lbc *reject
);

/* ### <ctype.h> ########################################################### */

int lbc_isalnum(
  lbc_int c
);

int lbc_isalpha(
  lbc_int c
);

int lbc_isascii(
  lbc_int c
);

int lbc_isblank(
  lbc_int c
);

int lbc_iscntrl(
  lbc_int c
);

int lbc_isdigit(
  lbc_int c
);

int lbc_isgraph(
  lbc_int c
);

int lbc_islower(
  lbc_int c
);

int lbc_isprint(
  lbc_int c
);

int lbc_ispunct(
  lbc_int c
);

int lbc_isspace(
  lbc_int c
);

int lbc_isupper(
  lbc_int c
);

int lbc_isxdigit(
  lbc_int c
);

/* ### <cwalk.h> library ################################################### */

/** Libblu char cwalk library compatibility layer
 * https://github.com/likle/cwalk
 */

#if defined(LBC_CWALK) || defined(LBC_IMPLEMENTATION)
#  include "../libs/cwalk/include/cwalk.h"

/* ###### Basics : ######################################################### */

void lbc_cwk_path_get_basename(
  const lbc *path,
  const lbc **basename,
  size_t *length
);

size_t lbc_cwk_path_change_basename(
  const lbc *path,
  const lbc *new_basename,
  lbc *buffer,
  size_t buffer_size
);

void lbc_cwk_path_get_dirname(
  const lbc *path,
  size_t *length
);

void lbc_cwk_path_get_root(
  const lbc *path,
  size_t *length
);

size_t lbc_cwk_path_change_root(
  const lbc *path,
  const lbc *new_root,
  lbc *buffer,
  size_t buffer_size
);

bool lbc_cwk_path_is_absolute(
  const lbc *path
);

bool lbc_cwk_path_is_relative(
  const lbc *path
);

size_t lbc_cwk_path_join(
  const lbc *path_a,
  const lbc *path_b,
  lbc *buffer,
  size_t buffer_size
);

size_t lbc_cwk_path_join_multiple(
  const lbc **paths,
  lbc *buffer,
  size_t buffer_size
);

size_t lbc_cwk_path_normalize(
  const lbc *path,
  lbc *buffer,
  size_t buffer_size
);

size_t lbc_cwk_path_get_intersection(
  const lbc *path_base,
  const lbc *path_other
);

/* ###### Navigation : ##################################################### */

size_t lbc_cwk_path_get_absolute(
  const lbc *base,
  const lbc *path,
  lbc *buffer,
  size_t buffer_size
);

size_t lbc_cwk_path_get_relative(
  const lbc *base_directory,
  const lbc *path,
  lbc *buffer,
  size_t buffer_size
);

/* ###### Extensions : ##################################################### */

bool lbc_cwk_path_get_extension(
  const lbc *path,
  const lbc **extension,
  size_t *length
);

bool lbc_cwk_path_has_extension(
  const lbc *path
);

size_t lbc_cwk_path_change_extension(
  const lbc *path,
  const lbc *new_extension,
  lbc *buffer,
  size_t buffer_size
);

/* ###### Segments : ####################################################### */

bool lbc_cwk_path_get_first_segment(
  const lbc *path,
  struct cwk_segment *segment
);

bool lbc_cwk_path_get_last_segment(
  const lbc *path,
  struct cwk_segment *segment
);

bool lbc_cwk_path_get_next_segment(
  struct cwk_segment *segment
);

bool lbc_cwk_path_get_previous_segment(
  struct cwk_segment *segment
);

enum cwk_segment_type lbc_cwk_path_get_segment_type(
  const struct cwk_segment *segment
);

size_t lbc_cwk_path_change_segment(
  struct cwk_segment *segment,
  const lbc *value,
  lbc *buffer,
  size_t buffer_size
);

/* ###### Style : ########################################################## */

enum cwk_path_style lbc_cwk_path_guess_style(
  const lbc *path
);

void lbc_cwk_path_set_style(
  enum cwk_path_style style
);

enum cwk_path_style lbc_cwk_path_get_style(
  void
);

#endif // defined(LBC_CWALK)

/* ### other ############################################################### */

bool lbc_equal(
  const lbc *str1,
  const lbc *str2
);

bool lbc_equaln(
  const lbc *str1,
  const lbc *str2,
  size_t size
);

int lbc_atob(
  bool *dst,
  const lbc *str
);

int lbc_access_fp(
  const lbc *filepath,
  const char *mode
);

int lbc_asprintf(
  lbc ** string,
  const lbc *format,
  ...
);

#if defined(LBC_VARIADIC) || defined(LBC_IMPLEMENTATION)

int lbc_vasprintf(
  lbc ** string,
  const lbc *format,
  va_list ap
);

#endif // defined(LBC_VARIADIC)

int lbc_chdir(
  const lbc *path
);

lbc *lbc_getcwd(
  lbc *buf,
  size_t size
);

#endif