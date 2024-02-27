#define LBC_IMPLEMENTATION
#include "string.h"

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>

#include "../libs/cwalk/include/cwalk.h"

#if defined(ARCH_UNIX)
/* ### Unix systems specific ############################################### */
#  include <unistd.h>

#elif defined(ARCH_WIN32)
/* ### Windows-API specific ################################################ */

#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <wchar.h>

static void _winInterpretLastError(
  const char * context_name
)
{
#define PRINT_ERROR(e)  fprintf(stderr, "[ERROR] %s: Windows API Error : '%s'.\n", context_name, e)

  switch (GetLastError()) {
  case ERROR_INSUFFICIENT_BUFFER:
    PRINT_ERROR("Supplied buffer size was not large enough");
    errno = ENOBUFS; break;
  case ERROR_INVALID_FLAGS:
    PRINT_ERROR("The values supplied for flags were not valid");
    errno = EINVAL; break;
  case ERROR_INVALID_PARAMETER:
    PRINT_ERROR("A parameter was invalid");
    errno = EINVAL; break;
  case ERROR_NO_UNICODE_TRANSLATION:
    PRINT_ERROR("Invalid Unicode character found");
    errno = EINVAL; break;
  }

#undef PRINT_ERROR
}

wchar_t * winUtf8ToWideChar(
  const lbc * string
)
{
  assert(NULL != string);

  int size = MultiByteToWideChar(CP_UTF8, 0, (char *) string, -1, NULL, 0);
  if (size <= 0) {
    _winInterpretLastError("UTF-8 to UTF-16");
    return NULL;
  }

  wchar_t * wc_string = calloc(size, sizeof(wchar_t));
  if (NULL == wc_string)
    return NULL;

  int ret = MultiByteToWideChar(CP_UTF8, 0, (char *) string, -1, wc_string, size);
  if (ret <= 0) {
    _winInterpretLastError("UTF-8 to UTF-16");
    free(wc_string);
    return NULL;
  }

  return wc_string;
}

lbc * winWideCharToUtf8(
  const wchar_t * string
)
{
  assert(NULL != string);

  int size = WideCharToMultiByte(CP_UTF8, 0, string, -1, NULL, 0, NULL, NULL);
  if (size <= 0) {
    _winInterpretLastError("UTF-16 to UTF-8");
    return NULL;
  }

  lbc * utf8_string = calloc(size, sizeof(lbc));
  if (NULL == utf8_string)
    return NULL;

  int ret = WideCharToMultiByte(CP_UTF8, 0, string, -1, (char *) utf8_string, size, NULL, NULL);
  if (ret <= 0) {
    _winInterpretLastError("UTF-16 to UTF-8");
    free(utf8_string);
    return NULL;
  }

  return utf8_string;
}

#endif

/* ### <stdlib.h> ########################################################## */

double lbc_strtod(
  const lbc *nptr,
  lbc **endptr
)
{
  return strtod((char *) nptr, (char **) endptr);
}

float lbc_strtof(
  const lbc * restrict nptr,
  lbc ** restrict endptr
)
{
  return strtof((char *) nptr, (char **) endptr);
}

long double lbc_strtold(
  const lbc *nptr,
  lbc **endptr
)
{
  return strtold((char *) nptr, (char **) endptr);
}

unsigned long lbc_strtoul(
  const lbc * restrict nptr,
  lbc ** restrict endptr,
  int base
)
{
  return strtoul((char *) nptr, (char **) endptr, base);
}

unsigned long long int lbc_strtoull(
  const lbc *nptr,
  lbc **endptr,
  int base
)
{
  return strtoull((char *) nptr, (char **) endptr, base);
}

long int lbc_strtol(
  const lbc *nptr,
  lbc **endptr,
  int base
)
{
  return strtol((char *) nptr, (char **) endptr, base);
}

long long int lbc_strtoll(
  const lbc *nptr,
  lbc **endptr,
  int base
)
{
  return strtoll((char *) nptr, (char **) endptr, base);
}

/* ### <stdio.h> ########################################################### */

#if defined(ARCH_UNIX)
/* Unix systems implementation */

FILE * lbc_fopen(
  const lbc * restrict pathname,
  const char * restrict mode
)
{
  return fopen((char *) pathname, mode);
}

#elif defined(ARCH_WIN32)
/* Windows API implementation */

FILE * lbc_fopen(
  const lbc * restrict pathname,
  const char * restrict mode
)
{
  wchar_t * wc_pathname = winUtf8ToWideChar(pathname);
  wchar_t * wc_mode     = winUtf8ToWideChar((unsigned char *) mode);
  if (NULL == wc_pathname || NULL == wc_mode)
    return NULL;

  FILE * fd = _wfopen(wc_pathname, wc_mode);

  free(wc_pathname);
  free(wc_mode);

  return fd;
}

#else
#  error Undefined arch
#endif

int lbc_printf(
  const lbc *format,
  ...
)
{
  va_list ap;
  va_start(ap, format);

  int ret = lbc_vprintf(format, ap);

  va_end(ap);
  return ret;
}

int lbc_fprintf(
  FILE *stream,
  const lbc *format,
  ...
)
{
  va_list ap;
  va_start(ap, format);

  int ret = lbc_vfprintf(stream, format, ap);

  va_end(ap);
  return ret;
}

int lbc_sprintf(
  lbc *str,
  const lbc *format,
  ...
)
{
  va_list ap;
  va_start(ap, format);

  int ret = lbc_vsprintf(str, format, ap);

  va_end(ap);
  return ret;
}

int lbc_snprintf(
  lbc *str,
  size_t size,
  const lbc *format,
  ...
)
{
  va_list ap;
  va_start(ap, format);

  int ret = lbc_vsnprintf(str, size, format, ap);

  va_end(ap);
  return ret;
}

#if defined(ARCH_UNIX) || 1
/* Unix systems implementation */

lbc *lbc_fgets(
  lbc *s,
  int size,
  FILE *stream
)
{
  return (lbc *) fgets((char *) s, size, stream);
}

#elif defined(ARCH_WIN32)
/* Windows API implementation */

lbc *lbc_fgets(
  lbc *s,
  int size,
  FILE *stream
)
{
  if (size <= 0 || NULL == s) {
    errno = EINVAL; // Invalid argument
    return NULL;
  }

  /* Read input file as wide char */
  wchar_t * wc_string = malloc(size * sizeof(wchar_t));
  if (NULL == wc_string)
    return NULL;

  if (wc_string != fgetws(wc_string, size, stream)) {
    free(wc_string);
    return NULL;
  }

  /* Convert readed string to UTF-8 */
  lbc * utf8_string = winWideCharToUtf8(wc_string);
  if (NULL == utf8_string) {
    free(wc_string);
    return NULL;
  }

  /* Paste the UTF-8 string to the output up to size bytes */
  lbc_strncpy(s, utf8_string, size-1);
  s[size-1] = '\0';

  free(wc_string);
  free(utf8_string);

  return s;
}

#else
#  error Undefined arch
#endif

int lbc_scanf(
  const lbc *format,
  ...
)
{
  va_list ap;
  va_start(ap, format);

  int ret = lbc_vscanf(format, ap);

  va_end(ap);
  return ret;
}

int lbc_fscanf(
  FILE *stream,
  const lbc *format,
  ...
)
{
  va_list ap;
  va_start(ap, format);

  int ret = lbc_vfscanf(stream, format, ap);

  va_end(ap);
  return ret;
}

int lbc_sscanf(
  const lbc *str,
  const lbc *format,
  ...
)
{
  va_list ap;
  va_start(ap, format);

  int ret = lbc_vsscanf(str, format, ap);

  va_end(ap);
  return ret;
}

/* ### <stdarg.h> ########################################################## */

#if defined(ARCH_UNIX)  || 1
/* Unix systems implementation */

int lbc_vprintf(
  const lbc *format,
  va_list ap
)
{
  return vprintf((char *) format, ap);
}

int lbc_vfprintf(
  FILE *stream,
  const lbc *format,
  va_list ap
)
{
  return vfprintf(stream, (char *) format, ap);
}

int lbc_vsprintf(
  lbc *str,
  const lbc *format,
  va_list ap
)
{
  return vsprintf((char *) str, (char *) format, ap);
}

int lbc_vsnprintf(
  lbc *str,
  size_t size,
  const lbc *format,
  va_list ap
)
{
  return vsnprintf((char *) str, size, (char *) format, ap);
}

int lbc_vscanf(
  const lbc *format,
  va_list ap
)
{
  return vscanf((char *) format, ap);
}

int lbc_vsscanf(
  const lbc *str,
  const lbc *format,
  va_list ap
)
{
  return vsscanf((char *) str, (char *) format, ap);
}

int lbc_vfscanf(
  FILE *stream,
  const lbc *format,
  va_list ap
)
{
  return vfscanf(stream, (char *) format, ap);
}

#elif defined(ARCH_WIN32)
/* Windows API implementation */

int lbc_vprintf(
  const lbc *format,
  va_list ap
)
{
  wchar_t * wc_format = winUtf8ToWideChar(format);
  if (NULL == wc_format)
    return -1;

  int ret = vwprintf_s(wc_format, ap);

  free(wc_format);

  return ret;
}

int lbc_vfprintf(
  FILE *stream,
  const lbc *format,
  va_list ap
)
{
  wchar_t * wc_format = winUtf8ToWideChar(format);
  if (NULL == wc_format)
    return -1;

  int ret = vfwprintf_s(stream, wc_format, ap);

  free(wc_format);

  return ret;
}

int lbc_vsnprintf(
  lbc *str,
  size_t size,
  const lbc *format,
  va_list ap
)
{
  if (0 < size && NULL == str) {
    errno = EINVAL;
    return -1;
  }

  wchar_t * wc_format = winUtf8ToWideChar(format);
  if (NULL == wc_format)
    return -1;

  fprintf(stderr, "wc_format=%p\n", wc_format);

  wchar_t * wc_buffer = NULL;
  int ret;

  if (0 < size) {
    wc_buffer = calloc(size, sizeof(wchar_t));
    if (NULL == wc_buffer)
      return -1;
    ret = _vsnwprintf(wc_buffer, size-1, wc_format, ap);

    if (0 <= ret && NULL != str) {
      /* Convert back to UTF-8 */
      lbc * utf8_string = winWideCharToUtf8(wc_buffer);
      if (NULL == utf8_string)
        return -1;
      fprintf(stderr, "utf8_string=%p\n", utf8_string);

      lbc_strncpy(str, utf8_string, size);
      str[size-1] = '\0';

      fprintf(stderr, "utf8_string=%p copied\n", utf8_string);

      free(utf8_string);
    }
  }
  else
    ret = _vsnwprintf(wc_buffer, 0, wc_format, ap);

  free(wc_format);

  return ret;
}

int lbc_vscanf(
  const lbc *format,
  va_list ap
)
{
  wchar_t * wc_format = winUtf8ToWideChar(format);
  if (NULL == wc_format)
    return -1;

  int ret = vwscanf(wc_format, ap);

  free(wc_format);

  return ret;
}

int lbc_vsscanf(
  const lbc *str,
  const lbc *format,
  va_list ap
)
{
  wchar_t * wc_str    = winUtf8ToWideChar(str);
  wchar_t * wc_format = winUtf8ToWideChar(format);
  if (NULL == wc_str || NULL == wc_format)
    return -1;

  int ret = vswscanf(wc_str, wc_format, ap);

  free(wc_str);
  free(wc_format);

  return ret;
}

int lbc_vfscanf(
  FILE *stream,
  const lbc *format,
  va_list ap
)
{
  wchar_t * wc_format = winUtf8ToWideChar(format);
  if (NULL == wc_format)
    return -1;

  int ret = vfwscanf(stream, wc_format, ap);

  free(wc_format);

  return ret;
}

#else
#  error Undefined arch
#endif

/* ### <string.h> ########################################################## */

size_t lbc_strlen(
  const lbc * str
)
{
  return strlen(
    (char *) str
  );
}

int lbc_strcmp(
  const lbc * str1,
  const lbc * str2
)
{
  return strcmp(
    (char *) str1,
    (char *) str2
  );
}

int lbc_strncmp(
  const lbc * str1,
  const lbc * str2,
  size_t size
)
{
  return strncmp(
    (char *) str1,
    (char *) str2,
    size
  );
}

lbc * lbc_strcpy(
  lbc * restrict dst,
  const lbc * restrict src
)
{
  return (lbc *) strcpy(
    (char *) dst,
    (char *) src
  );
}

lbc * lbc_strncpy(
  lbc * restrict dst,
  const lbc * restrict src,
  size_t size
)
{
  return (lbc *) strncpy(
    (char *) dst,
    (char *) src,
    size
  );
}

void lbc_strncat(
  lbc * s1,
  const lbc * s2,
  size_t size
)
{
  while (*s1 != '\0')
    s1++;

  while (*s2 != '\0' && 0 < --size)
    *(s1++) = *(s2++);
  *s1 = '\0';
}

lbc * lbc_strdup(
  const lbc * str
)
{
  if (NULL == str)
    return NULL;

  size_t size = 1 + lbc_strlen(str);

  lbc * dup = malloc(size);
  if (NULL == dup)
    return NULL;
  memcpy(dup, str, size);

  return dup;
}

lbc * lbc_strndup(
  const lbc * str,
  size_t size
)
{
  if (NULL == str || !size)
    return NULL;

  lbc * dup = calloc(size + 1, sizeof(lbc));
  if (NULL == dup)
    return NULL;
  lbc_strncpy(dup, str, size);

  return dup;
}

size_t lbc_strspn(
  const lbc *s,
  const lbc *accept
)
{
  return strspn((char *) s, (char *) accept);
}

size_t lbc_strcspn(
  const lbc *s,
  const lbc *reject
)
{
  return strcspn((char *) s, (char *) reject);
}

/* ### <ctype.h> ########################################################### */

int lbc_isalnum(
  lbc_int c
)
{
  return isalnum((int) c);
}

int lbc_isalpha(
  lbc_int c
)
{
  return isalpha((int) c);
}

int lbc_isascii(
  lbc_int c
)
{
  return isascii((int) c);
}

int lbc_isblank(
  lbc_int c
)
{
  return isblank((int) c);
}

int lbc_iscntrl(
  lbc_int c
)
{
  return iscntrl((int) c);
}

int lbc_isdigit(
  lbc_int c
)
{
  return isdigit((int) c);
}

int lbc_isgraph(
  lbc_int c
)
{
  return isgraph((int) c);
}

int lbc_islower(
  lbc_int c
)
{
  return islower((int) c);
}

int lbc_isprint(
  lbc_int c
)
{
  return isprint((int) c);
}

int lbc_ispunct(
  lbc_int c
)
{
  return ispunct((int) c);
}

int lbc_isspace(
  lbc_int c
)
{
  return isspace((int) c);
}

int lbc_isupper(
  lbc_int c
)
{
  return isupper((int) c);
}

int lbc_isxdigit(
  lbc_int c
)
{
  return isxdigit((int) c);
}

/* ### <cwalk.h> library ################################################### */

/* ###### Basics : ######################################################### */

void lbc_cwk_path_get_basename(
  const lbc *path,
  const lbc **basename,
  size_t *length
)
{
  return cwk_path_get_basename((char *) path, (const char **) basename, length);
}

size_t lbc_cwk_path_change_basename(
  const lbc *path,
  const lbc *new_basename,
  lbc *buffer,
  size_t buffer_size
)
{
  return cwk_path_change_basename((char *) path, (char *) new_basename, (char *) buffer, buffer_size);
}

void lbc_cwk_path_get_dirname(
  const lbc *path,
  size_t *length
)
{
  return cwk_path_get_dirname((char *) path, length);
}

void lbc_cwk_path_get_root(
  const lbc *path,
  size_t *length
)
{
  return cwk_path_get_root((char *) path, length);
}

size_t lbc_cwk_path_change_root(
  const lbc *path,
  const lbc *new_root,
  lbc *buffer,
  size_t buffer_size
)
{
  return cwk_path_change_root((char *) path, (char *) new_root, (char *) buffer, buffer_size);
}

bool lbc_cwk_path_is_absolute(
  const lbc *path
)
{
  return cwk_path_is_absolute((char *) path);
}

bool lbc_cwk_path_is_relative(
  const lbc *path
)
{
  return cwk_path_is_relative((char *) path);
}

size_t lbc_cwk_path_join(
  const lbc *path_a,
  const lbc *path_b,
  lbc *buffer,
  size_t buffer_size
)
{
  return cwk_path_join((char *) path_a, (char *) path_b, (char *) buffer, buffer_size);
}

size_t lbc_cwk_path_join_multiple(
  const lbc **paths,
  lbc *buffer,
  size_t buffer_size
)
{
  return cwk_path_join_multiple((const char **) paths, (char *) buffer, buffer_size);
}

size_t lbc_cwk_path_normalize(
  const lbc *path,
  lbc *buffer,
  size_t buffer_size
)
{
  return cwk_path_normalize((char *) path, (char *) buffer, buffer_size);
}

size_t lbc_cwk_path_get_intersection(
  const lbc *path_base,
  const lbc *path_other
)
{
  return cwk_path_get_intersection((char *) path_base, (char *) path_other);
}

/* ###### Navigation : ##################################################### */

size_t lbc_cwk_path_get_absolute(
  const lbc *base,
  const lbc *path,
  lbc *buffer,
  size_t buffer_size
)
{
  return cwk_path_get_absolute((char *) base, (char *) path, (char *) buffer, buffer_size);
}

size_t lbc_cwk_path_get_relative(
  const lbc *base_directory,
  const lbc *path,
  lbc *buffer,
  size_t buffer_size
)
{
  return cwk_path_get_relative((char *) base_directory, (char *) path, (char *) buffer, buffer_size);
}

/* ###### Extensions : ##################################################### */

bool lbc_cwk_path_get_extension(
  const lbc *path,
  const lbc **extension,
  size_t *length
)
{
  return cwk_path_get_extension((char *) path, (const char **) extension, length);
}

bool lbc_cwk_path_has_extension(
  const lbc *path
)
{
  return cwk_path_has_extension((char *) path);
}

size_t lbc_cwk_path_change_extension(
  const lbc *path,
  const lbc *new_extension,
  lbc *buffer,
  size_t buffer_size
)
{
  return cwk_path_change_extension((char *) path, (char *) new_extension, (char *) buffer, buffer_size);
}

/* ###### Segments : ####################################################### */

bool lbc_cwk_path_get_first_segment(
  const lbc *path,
  struct cwk_segment *segment
)
{
  return cwk_path_get_first_segment((char *) path, segment);
}

bool lbc_cwk_path_get_last_segment(
  const lbc *path,
  struct cwk_segment *segment
)
{
  return cwk_path_get_last_segment((char *) path, segment);
}

bool lbc_cwk_path_get_next_segment(
  struct cwk_segment *segment
)
{
  return cwk_path_get_next_segment(segment);
}

bool lbc_cwk_path_get_previous_segment(
  struct cwk_segment *segment
)
{
  return cwk_path_get_previous_segment(segment);
}

enum cwk_segment_type lbc_cwk_path_get_segment_type(
  const struct cwk_segment *segment
)
{
  return cwk_path_get_segment_type(segment);
}

size_t lbc_cwk_path_change_segment(
  struct cwk_segment *segment,
  const lbc *value,
  lbc *buffer,
  size_t buffer_size
)
{
  return cwk_path_change_segment(segment, (char *) value, (char *) buffer, buffer_size);
}

/* ###### Style : ########################################################## */

enum cwk_path_style lbc_cwk_path_guess_style(
  const lbc *path
)
{
  return cwk_path_guess_style((char *) path);
}

void lbc_cwk_path_set_style(
  enum cwk_path_style style
)
{
  return cwk_path_set_style(style);
}

enum cwk_path_style lbc_cwk_path_get_style(
  void
)
{
  return cwk_path_get_style();
}

/* ### other ############################################################### */

bool lbc_equal(
  const lbc * str1,
  const lbc * str2
)
{
  assert(NULL != str1);
  assert(NULL != str2);

  return (
    *str1 == *str2
    && 0 == strcmp(
      (char *) str1,
      (char *) str2
    )
  );
}

bool lbc_equaln(
  const lbc * str1,
  const lbc * str2,
  size_t size
)
{
  assert(NULL != str1);
  assert(NULL != str2);

  return (
    *str1 == *str2
    && 0 == lbc_strncmp(
      str1,
      str2,
      size
    )
  );
}

int lbc_atob(
  bool * dst,
  const lbc * str
)
{
  static const lbc * true_strings[] = {
    lbc_str("1"),
    lbc_str("true"),
    lbc_str("True"),
    lbc_str("TRUE")
  };

  static const lbc * false_strings[] = {
    lbc_str("0"),
    lbc_str("false"),
    lbc_str("False"),
    lbc_str("FALSE")
  };

  for (unsigned i = 0; i < ARRAY_SIZE(true_strings); i++) {
    if (lbc_equal(str, true_strings[i])) {
      *dst = true;
      return 0;
    }
  }

  for (unsigned i = 0; i < ARRAY_SIZE(false_strings); i++) {
    if (lbc_equal(str, false_strings[i])) {
      *dst = false;
      return 0;
    }
  }

  return -1;
}

int lbc_access_fp(
  const lbc * filepath,
  const char * mode
)
{
  FILE * fd = lbc_fopen(filepath, mode);
  if (NULL == fd)
    return -1;
  return fclose(fd);
}

int lbc_asprintf(
  lbc ** string,
  const lbc * format,
  ...
)
{
  va_list ap;
  va_start(ap, format);

  int ret = lbc_vasprintf(string, format, ap);

  va_end(ap);
  return ret;
}

int lbc_vasprintf(
  lbc ** string,
  const lbc * format,
  va_list ap
)
{
  lbc * res = NULL;

  va_list ap_copy;
  va_copy(ap_copy, ap);

  int size = lbc_vsnprintf(NULL, 0, format, ap);
  if (size < 0) {
    va_end(ap_copy);
    return -1;
  }

  if (NULL != string) {
    if (NULL == (res = malloc(size + 1)) || lbc_vsprintf(res, format, ap_copy) < 0)
      return -1;
  }

  *string = res;

  va_end(ap_copy);
  return size;
}

#if defined(ARCH_UNIX)
/* Posix implementation */

int lbc_chdir(
  const lbc * path
)
{
  return chdir((char *) path);
}

lbc * lbc_getcwd(
  lbc * buf,
  size_t size
)
{
  if (NULL != buf && !size) {
    errno = EINVAL;
    return NULL;
  }

  char * cur_wd = getcwd((char *) buf, size);

  if (NULL == cur_wd && NULL == buf && (errno == ERANGE || errno == EINVAL)) {
    /* Implementation not handling self-allocation extension */
    char * cwd_buf = NULL;
    size_t cwd_buf_size = 1024;

    long path_max = pathconf(".", _PC_PATH_MAX);
    if (0 < path_max)
      cwd_buf_size = MIN((size_t) path_max, 10240u);

    for (; NULL == cur_wd; cwd_buf_size <<= 1) {
      if (NULL == (cwd_buf = realloc(cwd_buf, cwd_buf_size)))
        return NULL;

      cur_wd = getcwd(cwd_buf, size);
      if (NULL == cur_wd && errno != ERANGE)
        return NULL;
    }

    errno = 0;
  }

  return (lbc *) cur_wd;
}

#elif defined(ARCH_WIN32)
/* Windows API implementation */

int lbc_chdir(
  const lbc * path
)
{
  wchar_t * wc_path = winUtf8ToWideChar(path);
  if (NULL == wc_path)
    return -1;

  int ret = _wchdir(wc_path);

  free(wc_path);

  return ret;
}

lbc * lbc_getcwd(
  lbc * buf,
  size_t size
)
{
  if (INT_MAX <= size || (NULL != buf && !size)) {
    errno = EINVAL;
    return NULL;
  }

  wchar_t * wc_buf = NULL;
  if (NULL != buf) {
    wc_buf = malloc(size * sizeof(wchar_t));
    if (NULL == wc_buf)
      return NULL;
  }

  wchar_t * ret = _wgetcwd(wc_buf, size);
  if (NULL == ret)
    return NULL;

  lbc * utf8_buf = winWideCharToUtf8(ret);
  if (NULL == utf8_buf)
    return NULL;
  free(ret);

  if (NULL != buf) {
    lbc_strncpy(buf, utf8_buf, size);
    if ('\0' != buf[size-1]) {
      free(utf8_buf);
      errno = ENAMETOOLONG;
      return NULL;
    }

    free(utf8_buf);
    return buf;
  }

  return utf8_buf;
}

#else
#  error Undefined arch
#endif