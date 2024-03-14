#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "iniHandler.h"

extern FILE *yyin;
extern int iniparse(IniFileContext *ctx);
extern int yylex_destroy (void);

int parseIniFile(
  IniFileContext *dst,
  const lbc *filepath
)
{
  FILE *fp = lbc_fopen(filepath, "r");
  if (NULL == fp)
    LIBBLU_ERROR_RETURN(
      "Unable to open INI file '%" PRI_LBCS "', %s (errno: %d).\n",
      filepath,
      strerror(errno),
      errno
    );

  IniFileContext ctx = {0};
  if (loadSourceIniFileContext(&ctx, fp) < 0)
    goto free_return;

  yyin = fp;
  if (0 != iniparse(&ctx))
    LIBBLU_ERROR_FRETURN(
      "Unable to parse INI file '%" PRI_LBCS "'.\n",
      filepath
    );

  yylex_destroy();
  fclose(fp);

  *dst = ctx;
  return 0;

free_return:
  cleanIniFileContext(ctx);
  fclose(fp);
  return -1;
}

lbc *lookupIniFile(
  const IniFileContext ctx,
  const char *expr
)
{
  return lookupIniFileNode(
    ctx.tree,
    expr
  );
}