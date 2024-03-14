/** \~english
 * \file libraries.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief Portable dynamic libraries handling module.
 */

#ifndef __LIBBLU_MUXER__UTIL__LIBRARIES_H__
#define __LIBBLU_MUXER__UTIL__LIBRARIES_H__

#include "macros.h"
#include "errorCodes.h"
#include "common.h"

typedef struct LibbluLibraryHandleStruct
  LibbluLibraryHandle,
  *LibbluLibraryHandlePtr
;

LibbluLibraryHandlePtr loadLibbluLibrary(
  const lbc *name
);

void closeLibbluLibrary(
  LibbluLibraryHandlePtr handle
);

void * resolveSymLibbluLibrary(
  LibbluLibraryHandlePtr handle,
  const char *name
);

/** \~english
 * \brief Resolve a symbol using the resolveSymLibbluLibrary() function.
 *
 * \param h LibbluLibraryHandlePtr Library handle.
 * \param n const char *Symbol name to resolve.
 * \param d void *Resolved symbol destination.
 * \param e Executed instruction on resolve error.
 *
 * \code{.c}
 * struct {
 *  LibbluLibraryHandlePtr lib;
 *
 *  png_infop (*png_create_info_struct) (png_const_structrp png_ptr);
 * } handle;
 *
 * // Init handle->lib with loadLibbluLibrary().
 *
 * RESOLVE_SYM_LB_LIB(handle->lib, "png_create_info_struct", handle->png_create_info_struct, return -1);
 * \endcode
 */
#define RESOLVE_SYM_LB_LIB(h, n, d, e)                                        \
  do {                                                                        \
    if (NULL == (d = resolveSymLibbluLibrary(h, n)))                          \
      e;                                                                      \
  } while (0)

#endif