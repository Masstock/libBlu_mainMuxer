/** \~english
 * \file messages.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief Terminal message printing module.
 */

#ifndef __LIBBLU_MUXER__UTIL__MESSAGES_H__
#define __LIBBLU_MUXER__UTIL__MESSAGES_H__

#include "macros.h"

#define ENABLE_DEBUG_MESSAGES_STR "--debug"
#define MAX_VERBOSE_LEVEL                 3

#define DEFAULT_DEBUG_STATE false
#define DEFAULT_VERBOSE_LEVEL MAX_VERBOSE_LEVEL

void enableDebug(void);
void defineDebugVerboseLevel(int level);

bool isDebugEnabled(void);
int getDebugVerboseLevel(void);

void echoError(const lbc * format, ...);
void echoErrorNoHeader(const lbc * format, ...);
void echoWarning(const lbc * format, ...);
void echoInfo(const lbc * format, ...);

#if defined(NDEBUGMSG)
#  pragma GCC diagnostic ignored "-Wunused-value"

#  define echoDebug(...)               (void)sizeof(void)
#  define echoDebugNoHeader(...)       (void)sizeof(void)
#  define echoDebugLevel(...)          (void)sizeof(void)
#  define echoDebugNoHeaderLevel(...)  (void)sizeof(void)

#  pragma GCC diagnostic warning "-Wunused-value"
#else

int echoDebug(const lbc * format, ...);
int echoDebugNoHeader(const lbc * format, ...);
int echoDebugLevel(const lbc * debugContextName, int debugLevel, const lbc * format, ...);
int echoDebugNoHeaderLevel(int debugLevel, const lbc * format, ...);

#endif

#endif