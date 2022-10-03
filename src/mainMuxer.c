#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <assert.h>

#include "mainMuxer.h"

static void printProgressBar(
  double progression
)
{
  double percentage;
  int barSize;

  static double lastPercentage = -1;

  static const unsigned targetedDecima = 1;
  static const double pow10[10] = {
    1,
    10,
    100,
    1000,
    10000,
    100000,
    1000000,
    10000000,
    100000000,
    1000000000
  };

  assert(targetedDecima <= 5);

  percentage = floor(
    progression * pow10[targetedDecima+2]
  ) / pow10[targetedDecima];

  if (percentage <= lastPercentage)
    return;
  lastPercentage = percentage;

  percentage = MIN(percentage, 100);

  barSize = floor(percentage / 5);

  lbc_printf(
    "Multiplexing... [%.*s%.*s] %*.*f%%\r",
    barSize,      "====================",
    20 - barSize, "                    ",
    3, targetedDecima, percentage
  );
  fflush(stdout);
}

static void printBytes(
  size_t bytes
)
{
  double mantissa;
  size_t nbDiv;

#if defined(ARCH_WIN32) /* Windows uses MiB, not MB. */
  static const char * divSuf[] = {"B", "KiB", "MiB", "GiB", "TiB"};
#else
  static const char * divSuf[] = {"B", "KB", "MB", "GB", "TB"};
#endif

  mantissa = (double) bytes;
  for (nbDiv = 0; mantissa > 1000.0 && nbDiv < ARRAY_SIZE(divSuf); nbDiv++) {
#if defined(ARCH_WIN32)
    mantissa /= 1024;
#else
    mantissa /= 1000;
#endif
  }

  lbc_printf("%.1f %s", mantissa, divSuf[nbDiv]);
}

int mainMux(
  LibbluMuxingSettings settings
)
{
  BitstreamWriterPtr output = NULL;

  LibbluMuxingContextPtr ctx;

  unsigned nbSystemPackets, i;
  bool debugMode;

  LIBBLU_DEBUG_COM("Verbose output activated.\n");
  debugMode = isDebugEnabled() /* || true */;

  if (NULL == (ctx = createLibbluMuxingContext(settings)))
    goto free_return;

  if (NULL == (output = createBitstreamWriter(settings.outputTsFilename, WRITE_BUFFER_LEN)))
    goto free_return;

  /* Mux packets while remain data */
  while (dataRemainingLibbluMuxingContext(ctx)) {
    if (muxNextPacketLibbluMuxingContext(ctx, output) < 0)
      goto free_return;

    if (!debugMode)
      printProgressBar(ctx->progress);
  }

  /* Padding aligned unit (= 32 TS p.) with NULL packets : */
  if (padAlignedUnitLibbluMuxingContext(ctx, output) < 0)
    goto free_return;

  lbc_printf("Multiplexing... [====================] 100%% Finished !\n\n");

  closeBitstreamWriter(output);

  lbc_printf("== Multiplexing summary ===============================================================\n");
  lbc_printf("Muxed: %u packets (", ctx->nbTsPacketsMuxed);
  printBytes(ctx->nbBytesWritten);
  lbc_printf(", %zu bytes).\n", ctx->nbBytesWritten);

  nbSystemPackets = ctx->nbTsPacketsMuxed;
  for (i = 0; i < nbESLibbluMuxingContext(ctx); i++) {
    lbc_printf(
      " - 0x%04x : %d packets (%.1f%%);\n",
      ctx->elementaryStreams[i]->pid,
      ctx->elementaryStreams[i]->packetNb,
      ((float) ctx->elementaryStreams[i]->packetNb / ctx->nbTsPacketsMuxed) * 100
    );
    nbSystemPackets -= ctx->elementaryStreams[i]->packetNb;
  }
  lbc_printf(" - System packets : %d packets (%.1f%%).\n", nbSystemPackets, ((float) nbSystemPackets / ctx->nbTsPacketsMuxed) * 100);

  lbc_printf("=======================================================================================\n");
  destroyLibbluMuxingContext(ctx);

  return 0;

free_return:
  closeBitstreamWriter(output);
  destroyLibbluMuxingContext(ctx);
  return -1;
}