
#ifndef __LIBBLU_MUXER__INI__INI_DATA_H__
#define __LIBBLU_MUXER__INI__INI_DATA_H__

#if defined(DISABLE_INI)

typedef struct {
  void * empty;
} IniFileContext;

static inline lbc * lookupIniFile(
  const IniFileContext ctx,
  const char * expr
)
{
  (void) ctx;
  (void) expr;

  return NULL;
}

static inline void cleanIniFileContext(
  IniFileContext ctx
)
{
  (void) ctx;
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
  unsigned src_nb_lines;
} IniFileContext;

static inline void cleanIniFileContext(
  IniFileContext ctx
)
{
  destroyIniFileNode(ctx.tree);

  for (unsigned i = ctx.src_nb_lines; 0 < i; i--)
    free(ctx.src[i-1]);
  free(ctx.src);
}

#define INI_DEFAULT_NB_SOURCE_CODE_LINES 32
#define INI_DEFAULT_LEN_SOURCE_CODE_LINE 512

int loadSourceIniFileContext(
  IniFileContext * dst,
  FILE * inputFile
);

#endif
#endif