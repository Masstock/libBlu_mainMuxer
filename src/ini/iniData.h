
#ifndef __LIBBLU_MUXER__INI__INI_DATA_H__
#define __LIBBLU_MUXER__INI__INI_DATA_H__

#if defined(DISABLE_INI)
typedef void * IniFileContextPtr;
#define destroyIniFileContext (void) sizeof

static inline void * lookupIniFile(
  IniFileContextPtr ctx,
  const char * expr
)
{
  (void) ctx;
  (void) expr;

  return NULL;
}

#else

#include "../util.h"
#include "iniTree.h"

typedef struct {
  int first_line;
  int first_column;
  int last_line;
  int last_column;
} IniFileSrcLocation;

typedef struct {
  IniFileNodePtr tree;

  char ** src;
  size_t srcNbLines;
} IniFileContext, *IniFileContextPtr;

IniFileContextPtr createIniFileContext(
  void
);

static inline void destroyIniFileContext(
  IniFileContextPtr ctx
)
{
  size_t i;

  if (NULL == ctx)
    return;

  destroyIniFileNode(ctx->tree);

  for (i = ctx->srcNbLines; 0 < i; i--)
    free(ctx->src[i-1]);
  free(ctx->src);
  free(ctx);
}

#define INI_DEFAULT_NB_SOURCE_CODE_LINES 32
#define INI_DEFAULT_LEN_SOURCE_CODE_LINE 512

int loadSourceIniFileContext(
  IniFileContextPtr dst,
  FILE * inputFile
);

#endif
#endif