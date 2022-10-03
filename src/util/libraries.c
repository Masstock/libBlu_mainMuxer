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
  lbc * name;
  HMODULE lib;
};

static LibbluLibraryHandlePtr createLibbluLibraryHandle(
  const lbc * name,
  HMODULE lib
)
{
  lbc * nameCopy;
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
  const lbc * name
)
{
  HMODULE lib;
  LibbluLibraryHandlePtr handle;

  if (NULL == (lib = LoadLibraryW(name))) {
    DWORD errCode;
    lbc errMsg[STR_BUFSIZE];

    getWindowsError(&errCode, errMsg, STR_BUFSIZE);
    LIBBLU_ERROR_NRETURN(
      "Unable to load library '%" PRI_LBCS "', %" PRI_LBCS " "
      "(winerror: %lu).\n",
      name,
      errMsg,
      errCode
    );
  }

  if (NULL == (handle = createLibbluLibraryHandle(name, lib)))
    goto free_return;
  return handle;

free_return:
  FreeLibrary(lib);
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

void * resolveSymLibbluLibrary(
  LibbluLibraryHandlePtr handle,
  const char * name
)
{
  void * sym;

  assert(NULL != handle);

  if (NULL == (sym = GetProcAddress(handle->lib, name))) {
    DWORD errCode;
    lbc errMsg[STR_BUFSIZE];

    getWindowsError(&errCode, errMsg, STR_BUFSIZE);
    LIBBLU_ERROR_NRETURN(
      "Unable to resolve symbol '%s' from library '%" PRI_LBCS "', "
      "%" PRI_LBCS " (winerror: %lu).\n",
      name,
      handle->name,
      errMsg,
      errCode
    );
  }

  return sym;
}

#else
/* ######################################################################### */
/* # Unix                                                                  # */
/* ######################################################################### */
#  include <dlfcn.h>

struct LibbluLibraryHandleStruct {
  char * name;
  void * lib;
};

static LibbluLibraryHandlePtr createLibbluLibraryHandle(
  const lbc * name,
  void * lib
)
{
  lbc * nameCopy;
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
  const lbc * name
)
{
  void * lib;
  LibbluLibraryHandlePtr handle;

  if (NULL == (lib = dlopen(name, RTLD_LAZY)))
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

void * resolveSymLibbluLibrary(
  LibbluLibraryHandlePtr handle,
  const char * name
)
{
  void * sym;

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

