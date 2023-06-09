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
  HdmvContextPtr ctx;

  if (NULL == (ctx = createHdmvContext(settings, NULL, HDMV_STREAM_TYPE_PGS, false)))
    return -1;

  while (!isEofHdmvContext(ctx)) {
    /* Progress bar : */
    printFileParsingProgressionBar(inputHdmvContext(ctx));

    if (parseHdmvSegment(ctx) < 0)
      goto free_return;
  }

  /* Process remaining segments: */
  if (completeDisplaySetHdmvContext(ctx) < 0)
    return -1;

  lbc_printf(" === Parsing finished with success. ===              \n");

  /* Display infos : */
  lbc_printf("== Stream Infos =======================================================================\n");
  lbc_printf("Codec: HDMV/PGS Subtitles format.\n");
  lbc_printf("Number of Display Sets: %u.\n", ctx->nbDisplaySets);
  lbc_printf("Number of Epochs: %u.\n", ctx->nbEpochs);
  lbc_printf("Total number of segments per type:\n");
  printContentHdmvContext(ctx);
  lbc_printf("=======================================================================================\n");

  if (closeHdmvContext(ctx) < 0)
    goto free_return;
  destroyHdmvContext(ctx);
  return 0;

free_return:
  destroyHdmvContext(ctx);

  return -1;
}