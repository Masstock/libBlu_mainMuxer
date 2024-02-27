#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <assert.h>

#include "pgs_parser.h"

int analyzePgs(
  LibbluESParsingSettings * settings
)
{
  if (settings->options.hdmv.ass_input) {
#if !defined(DISABLE_PGS_ASS_GENERATOR)
    return processPgsGenerator(settings->esFilepath);
#else
    LIBBLU_ERROR_RETURN("PGS from ASS generator is not available in this program build !\n");
#endif
  }

  HdmvContext * ctx = createHdmvContext(settings, NULL, HDMV_STREAM_TYPE_PGS, false);
  if (NULL == ctx)
    return -1;

  while (!isEofHdmvContext(ctx)) {
    /* Progress bar : */
    printFileParsingProgressionBar(inputHdmvContext(ctx));

    if (parseHdmvSegment(ctx) < 0)
      goto free_return;
  }

  /* Process remaining segments: */
  if (completeCurDSHdmvContext(ctx) < 0)
    return -1;

  lbc_printf(
    lbc_str(" === Parsing finished with success. ===              \n")
  );

  /* Display infos : */
  lbc_printf(
    lbc_str("== Stream Infos =======================================================================\n")
  );
  lbc_printf(lbc_str("Codec: HDMV/PGS Subtitles format.\n"));
  lbc_printf(lbc_str("Number of Display Sets: %u.\n"), ctx->nb_DS);
  lbc_printf(lbc_str("Number of Epochs: %u.\n"), ctx->nb_epochs);
  lbc_printf(lbc_str("Total number of segments per type:\n"));
  printContentHdmvContext(ctx);
  lbc_printf(
    lbc_str("=======================================================================================\n")
  );

  if (closeHdmvContext(ctx) < 0)
    goto free_return;
  destroyHdmvContext(ctx);
  return 0;

free_return:
  destroyHdmvContext(ctx);
  return -1;
}