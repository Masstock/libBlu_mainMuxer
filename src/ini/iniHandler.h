
#ifndef __LIBBLU_MUXER__INI__INI_HANDLER_H__
#define __LIBBLU_MUXER__INI__INI_HANDLER_H__

#include "iniData.h"

int parseIniFile(
  IniFileContext *dst,
  const lbc *filepath
);

lbc * lookupIniFile(
  const IniFileContext ctx,
  const char *expr
);

#endif