#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>

#include "messages.h"

static bool debugMode = DEFAULT_DEBUG_STATE;
static int debugVerboseLevel = DEFAULT_VERBOSE_LEVEL;

void enableDebug(void)
{
  debugMode = true;
}

void defineDebugVerboseLevel(int level)
{
  debugVerboseLevel = level;
}

bool isDebugEnabled(void)
{
  return debugMode;
}

int getDebugVerboseLevel(void)
{
  return debugVerboseLevel;
}

void echoErrorCore(const lbc * format, va_list args)
{
  lbc_fprintf(stderr, "[ERROR] ");
  lbc_vfprintf(stderr, format, args);
}

void echoErrorNoHeaderCore(const lbc * format, va_list args)
{
  lbc_vfprintf(stderr, format, args);
}

void echoError(const lbc * format, ...)
{
  va_list args;

  va_start(args, format);
  echoErrorCore(format, args);
  va_end(args);
}

void echoErrorNoHeader(const lbc * format, ...)
{
  va_list args;

  va_start(args, format);
  echoErrorNoHeaderCore(format, args);
  va_end(args);
}

void echoWarning(const lbc * format, ...)
{
  va_list arg;

  va_start(arg, format);
  lbc_fprintf(stderr, "[WARNING] ");
  lbc_vfprintf(stderr, format, arg);
  va_end(arg);
}

void echoInfo(const lbc * format, ...)
{
  va_list arg;

  va_start(arg, format);
  lbc_fprintf(stdout, "[INFO] ");
  lbc_vfprintf(stdout, format, arg);
  va_end(arg);
}

void echoMsgCore(const lbc * msgName, const lbc * format, va_list args)
{
  lbc_fprintf(stdout, "[%s] ", msgName);
  lbc_vfprintf(stdout, format, args);
}

void echoMsgNoHeaderCore(const lbc * format, va_list args)
{
  lbc_vfprintf(stdout, format, args);
}

#if !defined(NDEBUGMSG)

int echoDebugCore(const lbc * debugName, const lbc * format, va_list args)
{
  int ret;

  ret = lbc_fprintf(stdout, "[DEBUG%s] ", debugName);
  ret += lbc_vfprintf(stdout, format, args);
  return ret;
}

int echoDebugNoHeaderCore(const lbc * format, va_list args)
{
  return lbc_vfprintf(stdout, format, args);
}

int echoDebug(const lbc * format, ...)
{
  va_list args;
  int ret;

  if (!debugMode || debugVerboseLevel < MAX_VERBOSE_LEVEL)
    return EOF;

  va_start(args, format);
  ret = echoDebugCore(lbc_str(""), format, args);
  va_end(args);

  return ret;
}

int echoDebugNoHeader(const lbc * format, ...)
{
  va_list args;
  int ret;

  if (!debugMode || debugVerboseLevel < MAX_VERBOSE_LEVEL)
    return EOF;

  va_start(args, format);
  ret = echoDebugNoHeaderCore(format, args);
  va_end(args);

  return ret;
}

int echoDebugLevel(const lbc * debugContextName, int debugLevel, const lbc * format, ...)
{
  va_list args;
  int ret;

  if (!debugMode || debugVerboseLevel < debugLevel)
    return EOF;

  va_start(args, format);
  ret = echoDebugCore(debugContextName, format, args);
  va_end(args);

  return ret;
}

int echoDebugNoHeaderLevel(int debugLevel, const lbc * format, ...)
{
  va_list args;
  int ret;

  if (!debugMode || debugVerboseLevel < debugLevel)
    return EOF;

  va_start(args, format);
  ret = echoDebugNoHeaderCore(format, args);
  va_end(args);

  return ret;
}

#endif
