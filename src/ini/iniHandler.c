#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "iniHandler.h"

extern FILE * yyin;
extern int iniparse(IniFileContextPtr ctx);
extern int yylex_destroy (void);

int parseIniFile(
  IniFileContextPtr * dst,
  const lbc * filepath
)
{
  IniFileContextPtr ctx;
  FILE * fp;

  if (NULL == (fp = lbc_fopen(filepath, "r")))
    LIBBLU_ERROR_RETURN(
      "Unable to open INI file '%" PRI_LBCS "', %s (errno: %d).\n",
      filepath,
      strerror(errno),
      errno
    );

  if (NULL == (ctx = createIniFileContext()))
    goto free_return;

  if (loadSourceIniFileContext(ctx, fp) < 0)
    goto free_return;

  yyin = fp;
  if (0 != iniparse(ctx))
    LIBBLU_ERROR_FRETURN(
      "Unable to parse INI file '%" PRI_LBCS "'.\n",
      filepath
    );
  yylex_destroy();

#if 0
  printIniFileNode(ctx->tree);
  printf("Found: '%s'.\n", lookupIniFileNode(ctx->tree, "LIBRARIES.LIBPNG"));
#endif

  if (NULL != dst)
    *dst = ctx;
  else
    destroyIniFileContext(ctx);

  fclose(fp);
  return 0;

free_return:
  destroyIniFileContext(ctx);
  fclose(fp);
  return -1;
}

lbc * lookupIniFile(
  const IniFileContextPtr ctx,
  const char * expr
)
{
  if (NULL == ctx)
    return NULL;
  return lookupIniFileNode(ctx->tree, expr);
}