#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "libraries.h"

#if defined(ARCH_WIN32)
/* ######################################################################### */
/* # WIN32                                                                #  */
/* ######################################################################### */
#  include <libloaderapi.h>

struct LibbluLibraryHandleStruct {
  lbc *name;
  HMODULE lib;
};

static LibbluLibraryHandlePtr createLibbluLibraryHandle(
  const lbc *name,
  HMODULE lib
)
{
  lbc *nameCopy;
  LibbluLibraryHandlePtr handle;

  if (NULL == (nameCopy = lbc_strdup(name)))
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");

  handle = (LibbluLibraryHandlePtr) malloc(sizeof(LibbluLibraryHandle));
  if (NULL == handle) {
    free(nameCopy);
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");
  }

  handle->name = nameCopy;
  handle->lib = lib;

  return handle;
}

static void destroyLibbluLibraryHandle(
  LibbluLibraryHandlePtr handle
)
{
  if (NULL == handle)
    return;

  free(handle->name);
  free(handle);
}

LibbluLibraryHandlePtr loadLibbluLibrary(
  const lbc *name
)
{
  lbc *err_msg = NULL;

  /* Convert library name to UTF-16 */
  wchar_t *wc_name = winUtf8ToWideChar(name);
  if (NULL == wc_name)
    return NULL;

  /* Load the library using Windows API */
  HMODULE lib = LoadLibraryW(wc_name);
  free(wc_name);

  if (NULL == lib) {
    /* Handle error */
    DWORD err_code;
    err_msg = getLastWindowsErrorString(&err_code);

    LIBBLU_ERROR_FRETURN(
      "Unable to load library '%" PRI_LBCS "', %" PRI_LBCS " "
      "(winerror: %lu).\n",
      name,
      err_msg,
      err_code
    );
  }

  LibbluLibraryHandlePtr handle = createLibbluLibraryHandle(name, lib);
  if (NULL == handle)
    goto free_return;
  return handle;

free_return:
  FreeLibrary(lib);
  free(err_msg);
  return NULL;
}

void closeLibbluLibrary(
  LibbluLibraryHandlePtr handle
)
{
  if (NULL == handle)
    return;

  FreeLibrary(handle->lib);
  destroyLibbluLibraryHandle(handle);
}

void *resolveSymLibbluLibrary(
  LibbluLibraryHandlePtr handle,
  const char *name
)
{
  lbc *err_msg = NULL;

  assert(NULL != handle);

  void *sym = GetProcAddress(handle->lib, name);
  if (NULL == sym) {
    /* Handle error */
    DWORD err_code;
    err_msg = getLastWindowsErrorString(&err_code);

    LIBBLU_ERROR_FRETURN(
      "Unable to resolve symbol '%s' from library '%" PRI_LBCS "', "
      "%" PRI_LBCS " (winerror: %lu).\n",
      name,
      handle->name,
      err_msg,
      err_code
    );
  }

  return sym;

free_return:
  free(err_msg);
  return NULL;
}

#else
/* ######################################################################### */
/* # Unix                                                                  # */
/* ######################################################################### */
#  include <dlfcn.h>

struct LibbluLibraryHandleStruct {
  lbc *name;
  void *lib;
};

static LibbluLibraryHandlePtr createLibbluLibraryHandle(
  const lbc *name,
  void *lib
)
{
  lbc *name_copy = lbc_strdup(name);
  if (NULL == name_copy)
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");

  LibbluLibraryHandlePtr handle = (LibbluLibraryHandlePtr) malloc(
    sizeof(LibbluLibraryHandle)
  );
  if (NULL == handle) {
    free(name_copy);
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");
  }

  handle->name = name_copy;
  handle->lib = lib;

  return handle;
}

static void destroyLibbluLibraryHandle(
  LibbluLibraryHandlePtr handle
)
{
  if (NULL == handle)
    return;

  free(handle->name);
  free(handle);
}

LibbluLibraryHandlePtr loadLibbluLibrary(
  const lbc *name
)
{
  void *lib;
  LibbluLibraryHandlePtr handle;

  if (NULL == (lib = dlopen((char *) name, RTLD_LAZY)))
    LIBBLU_ERROR_NRETURN(
      "Unable to load library '%" PRI_LBCS "', %s.\n",
      name,
      dlerror()
    );

  if (NULL == (handle = createLibbluLibraryHandle(name, lib)))
    goto free_return;
  return handle;

free_return:
  dlclose(lib);
  return NULL;
}

void closeLibbluLibrary(
  LibbluLibraryHandlePtr handle
)
{
  if (NULL == handle)
    return;

  dlclose(handle->lib);
  destroyLibbluLibraryHandle(handle);
}

void *resolveSymLibbluLibrary(
  LibbluLibraryHandlePtr handle,
  const char *name
)
{
  void *sym;

  assert(NULL != handle);

  if (NULL == (sym = dlsym(handle->lib, name)))
    LIBBLU_ERROR_NRETURN(
      "Unable to resolve symbol '%s' from library '%" PRI_LBCS "', "
      "%s.\n",
      name,
      dlerror()
    );

  return sym;
}

#endif

