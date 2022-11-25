#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "common.h"

#if defined(ARCH_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <iconv.h>

void getWindowsError(
  DWORD * err,
  lbc * buf,
  size_t size
)
{
  DWORD code;

  code = GetLastError();

  if (NULL != err)
    *err = code;

  if (NULL != buf) {
    FormatMessageW(
      FORMAT_MESSAGE_FROM_SYSTEM
      | FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL,
      code,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      buf,
      size,
      NULL
    );
  }
}

#define ICONV_INIT_ERROR  ((iconv_t) (-1))
#define ICONV_CONV_ERROR  ((size_t) (-1))

/** \~english
 * \brief
 *
 * Maximum size allowed by UTF-8 is 4 bytes. But because of its legacy, 6 is
 * chosen as a security.
 *
 * tools.ietf.org/html/rfc3629
 */
#define UTF8_MAX_CHAR_SIZE  6

static iconv_t iconv_ctx_to_utf8 = ICONV_INIT_ERROR;
static iconv_t iconv_ctx_from_utf8 = ICONV_INIT_ERROR;

int lb_init_iconv(
  void
)
{
  assert(ICONV_INIT_ERROR == iconv_ctx_to_utf8);
  assert(ICONV_INIT_ERROR == iconv_ctx_from_utf8);

  if (ICONV_INIT_ERROR == (iconv_ctx_to_utf8 = iconv_open("UTF-8", "WCHAR_T")))
    LIBBLU_ERROR_RETURN(
      "Unable to init iconv, %s (errno: %d).\n",
      strerror(errno),
      errno
    );
  if (ICONV_INIT_ERROR == (iconv_ctx_from_utf8 = iconv_open("WCHAR_T", "UTF-8")))
    LIBBLU_ERROR_RETURN(
      "Unable to init iconv, %s (errno: %d).\n",
      strerror(errno),
      errno
    );

  return 0;
}

char * lb_iconv_wchar_to_utf8(
  const wchar_t * src
)
{
  size_t src_nbchars;

  size_t src_bufsize;
  char * src_buf;

  size_t dst_bufsize;
  char * dst_buf, * res;

  size_t ret;

  assert(ICONV_INIT_ERROR != iconv_ctx_to_utf8);

  src_nbchars = wcslen(src) + 1;

  src_bufsize = src_nbchars * sizeof(wchar_t);
  src_buf = (char *) src;

  dst_bufsize = src_nbchars * UTF8_MAX_CHAR_SIZE;
  if (NULL == (dst_buf = (char *) malloc(dst_bufsize)))
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");
  res = dst_buf;

  ret = iconv(iconv_ctx_to_utf8, &src_buf, &src_bufsize, &dst_buf, &dst_bufsize);
  if (ICONV_CONV_ERROR == ret)
    LIBBLU_ERROR_NRETURN(
      "Characters conversion error, %s (errno: %d).\n",
      strerror(errno),
      errno
    );

  return res;
}

static inline size_t uchar_strlen(
  const unsigned char * string
)
{
  size_t size;

  for (size = 0; *(string++) != (unsigned char) '\0'; size++)
    ;

  return size;
}

wchar_t * lb_iconv_utf8_to_wchar(
  const unsigned char * src
)
{
  size_t src_nbchars;

  size_t src_bufsize;
  char * src_buf;

  size_t dst_bufsize;
  char * dst_buf, * res;

  size_t ret;

  assert(ICONV_INIT_ERROR != iconv_ctx_from_utf8);

  src_nbchars = uchar_strlen(src) + 1;

  src_bufsize = src_nbchars;
  src_buf = (char *) src;

  dst_bufsize = src_nbchars * sizeof(wchar_t);
  if (NULL == (dst_buf = (char *) malloc(dst_bufsize)))
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");
  res = dst_buf;

  ret = iconv(iconv_ctx_from_utf8, &src_buf, &src_bufsize, &dst_buf, &dst_bufsize);
  if (ICONV_CONV_ERROR == ret)
    LIBBLU_ERROR_NRETURN(
      "Characters conversion error, %s (errno: %d).\n",
      strerror(errno),
      errno
    );

  return (wchar_t *) res;
}

int lb_close_iconv(
  void
)
{
  assert(ICONV_INIT_ERROR != iconv_ctx_to_utf8);
  assert(ICONV_INIT_ERROR != iconv_ctx_from_utf8);

  if (iconv_close(iconv_ctx_to_utf8) < 0)
    LIBBLU_ERROR_RETURN(
      "Unable to close iconv, %s (errno: %d).\n",
      strerror(errno),
      errno
    );
  if (iconv_close(iconv_ctx_from_utf8) < 0)
    LIBBLU_ERROR_RETURN(
      "Unable to close iconv, %s (errno: %d).\n",
      strerror(errno),
      errno
    );

  return 0;
}

#endif

#if !defined(_GNU_SOURCE)

int lb_vasprintf(
  char ** string,
  const char * format,
  va_list ap
)
{
  va_list apCopy;
  int size;
  char * res;

  va_copy(apCopy, ap);
  if ((size = vsnprintf(NULL, 0, format, ap)) < 0)
    goto free_return;

  if (NULL == string) {
    va_end(apCopy);
    return size;
  }

  if (NULL == (res = malloc((size + 1) * sizeof(char))))
    return -1;

  if ((size = vsnprintf(res, size + 1, format, apCopy)) < 0) {
    free(res);
    goto free_return;
  }
  va_end(apCopy);

  *string = res;

  return size;

free_return:
  va_end(apCopy);
  return -1;
}

int lb_asprintf(
  char ** string,
  const char * format,
  ...
)
{
  va_list ap;
  int ret;

  va_start(ap, format);
  ret = lb_vasprintf(string, format, ap);
  va_end(ap);

  return ret;
}

#endif

int lb_atob(
  bool * dst,
  const char * str
)
{
  unsigned i;

  static const char * trueStrings[] = {
    "1",
    "true",
    "True",
    "TRUE"
  };

  static const char * falseStrings[] = {
    "0",
    "false",
    "False",
    "FALSE"
  };

  for (i = 0; i < ARRAY_SIZE(trueStrings); i++) {
    if (lb_str_equal(str, trueStrings[i])) {
      *dst = true;
      return 0;
    }
  }

  for (i = 0; i < ARRAY_SIZE(falseStrings); i++) {
    if (lb_str_equal(str, falseStrings[i])) {
      *dst = true;
      return 0;
    }
  }

  return -1;
}

#if defined(ARCH_WIN32)

int lb_watob(
  bool * dst,
  const wchar_t * str
)
{
  unsigned i;

  static const wchar_t * trueStrings[] = {
    L"1",
    L"true",
    L"True",
    L"TRUE"
  };

  static const wchar_t * falseStrings[] = {
    L"0",
    L"false",
    L"False",
    L"FALSE"
  };

  for (i = 0; i < ARRAY_SIZE(trueStrings); i++) {
    if (lb_wstr_equal(str, trueStrings[i])) {
      *dst = true;
      return 0;
    }
  }

  for (i = 0; i < ARRAY_SIZE(falseStrings); i++) {
    if (lb_wstr_equal(str, falseStrings[i])) {
      *dst = true;
      return 0;
    }
  }

  return -1;
}

int lb_wasprintf(
  wchar_t ** string,
  const wchar_t * format,
  ...
)
{
  va_list args;
  int size;
  wchar_t * res;

  va_start(args, format);

  if ((size = vswprintf(NULL, 0, format, args)) < 0)
    goto free_return;

  va_end(args);

  if (NULL == string)
    return size;

  if (NULL == (res = (wchar_t *) malloc((size + 1) * sizeof(wchar_t))))
    return -1;

  va_start(args, format);

  if ((size = vswprintf(res, size + 1, format, args)) < 0) {
    free(res);
    goto free_return;
  }

  va_end(args);

  *string = res;

  return size;

free_return:
  va_end(args);
  return -1;
}

#endif

#if defined(ARCH_UNIX)

#include <errno.h>
#include <string.h>
#include <unistd.h>

int lb_get_wd(
  char * buf,
  size_t size
)
{
  if (NULL == getcwd(buf, size)) {
    if (errno == ERANGE)
      LIBBLU_ERROR_RETURN(
        "Working directory is too long (more than %zu characters).\n",
        size
      );
    LIBBLU_ERROR_RETURN(
      "Unable to get working directory, %s (errno: %d).\n",
      strerror(errno),
      errno
    );
  }

  return 0;
}

#elif defined(ARCH_WIN32)

int lb_get_wd(
  char * buf,
  size_t size
)
{
  DWORD pathSize;

  pathSize = GetCurrentDirectory(size, buf);
  if (!pathSize) {
    DWORD errorCode;
    lbc errorMsg[STR_BUFSIZE];

    getWindowsError(&errorCode, errorMsg, STR_BUFSIZE);

    /* Fail */
    LIBBLU_ERROR_RETURN(
      "Unable to get working directory, %" PRI_LBCS " (winerror: %lu).\n",
      errorMsg,
      errorCode
    );
  }
  if (PATH_BUFSIZE < pathSize)
    LIBBLU_ERROR_RETURN(
      "Working directory is too long (more than %zu characters).\n",
      size
    );

  return 0;
}

int lb_wget_wd(
  wchar_t * buf,
  size_t size
)
{
  DWORD pathSize;

  pathSize = GetCurrentDirectoryW(size, buf);
  if (!pathSize) {
    DWORD errorCode;
    lbc errorMsg[STR_BUFSIZE];

    getWindowsError(&errorCode, errorMsg, STR_BUFSIZE);

    /* Fail */
    LIBBLU_ERROR_RETURN(
      "Unable to get working directory, %" PRI_LBCS " (winerror: %lu).\n",
      errorMsg,
      errorCode
    );
  }
  if (PATH_BUFSIZE < pathSize)
    LIBBLU_ERROR_RETURN(
      "Working directory is too long (more than %zu characters).\n",
      size
    );

  return 0;
}

#else
#  error No portable implementation of lb_get_wd() for this system.
#endif