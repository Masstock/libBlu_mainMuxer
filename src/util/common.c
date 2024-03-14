#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#define LBC_CWALK
#include "common.h"

#if defined(ARCH_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>

lbc * getLastWindowsErrorString(
  DWORD *err_ret
)
{
  DWORD code = GetLastError();

  if (NULL != err_ret)
    *err_ret = code;

  wchar_t *wc_err_string;

  DWORD ret = FormatMessageW(
    FORMAT_MESSAGE_ALLOCATE_BUFFER
    | FORMAT_MESSAGE_FROM_SYSTEM
    | FORMAT_MESSAGE_IGNORE_INSERTS,
    NULL,
    code,
    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
    (LPWSTR) &wc_err_string,
    0,
    NULL
  );
  if (0 == ret)
    return NULL;

  lbc *err_string = winWideCharToUtf8(wc_err_string);

  LocalFree(wc_err_string);

  return err_string;
}

#endif

uint32_t lbc_fnv1aStrHash(
  const lbc *str
)
{
  /* 32 bits FNV-1a hash */
  if (NULL == str)
    return 0;

  size_t length = lbc_strlen(str);

  uint32_t hash = 2166136261u;
  for (size_t i = 0; i < length; i++) {
    hash ^= ((uint8_t *) str)[i];
    hash *= 16777619;
  }

  return hash;
}

int lb_get_relative_fp_from_anchor(
  lbc *buf,
  size_t size,
  const lbc *filepath,
  const lbc *anchor
)
{
  size_t anchorDirectorySize;
  lbc anchorDirectory[PATH_BUFSIZE];

  /* Check if path is already absolute */
  if (lbc_cwk_path_is_absolute(filepath))
    return BOOL_TO_INT_RET(
      lbc_snprintf(buf, size, lbc_str("%" PRI_LBCS), filepath)
      < (int) size
    );

  lbc_cwk_path_get_dirname(anchor, &anchorDirectorySize);
  if (PATH_BUFSIZE <= anchorDirectorySize)
    return 0;
  if (!anchorDirectorySize)
    return BOOL_TO_INT_RET(
      lbc_snprintf(buf, size, lbc_str("%" PRI_LBCS), filepath)
      < (int) size
    );

  lbc_snprintf(anchorDirectory, anchorDirectorySize, lbc_str("%" PRI_LBCS), anchor);
  return BOOL_TO_INT_RET(
    lbc_snprintf(
      buf, size, lbc_str("%" PRI_LBCS "/%" PRI_LBCS), anchorDirectory, filepath
    ) < (int) size
  );
}

int lb_gen_anchor_absolute_fp(
  lbc ** dst,
  const lbc *base,
  const lbc *path
)
{
  size_t dirsize;
  lbc_cwk_path_get_dirname(base, &dirsize);

  lbc *relative;
  if (!lbc_cwk_path_is_absolute(path) && 0 < dirsize) {
    lbc *dir = calloc(dirsize + 1, sizeof(lbc));
    if (NULL == dir)
      LIBBLU_ERROR_RETURN("Memory allocation error.\n");
    memcpy(dir, base, dirsize *sizeof(lbc));

    size_t size = lbc_cwk_path_join(dir, path, NULL, 0);
    if (0 == size)
      LIBBLU_ERROR_RETURN("Unexpected zero sized path.\n");

    relative = malloc(++size *sizeof(lbc));
    if (NULL == relative)
      LIBBLU_ERROR_RETURN("Memory allocation error.\n");

    lbc_cwk_path_join(dir, path, relative, size);
    free(dir);
  }
  else {
    if (NULL == (relative = lbc_strdup(path)))
      LIBBLU_ERROR_RETURN("Memory allocation error.\n");
  }

  int ret = lb_gen_absolute_fp(dst, relative);
  free(relative);

  return ret;
}

int lb_gen_absolute_fp(
  lbc ** dst,
  const lbc *path
)
{
  assert(NULL != dst);
  assert(NULL != path);

  lbc *cur_wd = lbc_getcwd(NULL, 0);
  if (NULL == cur_wd)
    LIBBLU_ERROR_RETURN(
      "Unable to get working directory: %s (errno: %d).\n",
      strerror(errno),
      errno
    );

  size_t size = lbc_cwk_path_get_absolute(cur_wd, path, NULL, 0);
  if (0 == size)
    LIBBLU_ERROR_RETURN("Unexpected zero sized filepath.\n");

  lbc *abs_path = malloc(++size *sizeof(lbc));
  if (NULL == abs_path)
    LIBBLU_ERROR_RETURN("Memory allocation error.\n");

  lbc_cwk_path_get_absolute(cur_wd, path, abs_path, size);

  free(cur_wd);

  *dst = abs_path;
  return 0;
}