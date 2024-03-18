#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <assert.h>

#include "mainMuxer.h"

static void _printProgressBar(
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
    lbc_str("Multiplexing... [%.*s%.*s] %*.*f%%\r"),
    barSize,      "====================",
    20 - barSize, "                    ",
    3, targetedDecima, percentage
  );
  fflush(stdout);
}

static void printBytes(
  size_t bytes
)
{;
  static const char *div_suffix[] = {
#if defined(ARCH_WIN32) /* Windows uses MiB, not MB. */
    "B", "KiB", "MiB", "GiB", "TiB"
#else
    "B", "KB", "MB", "GB", "TB"
#endif
  };

  double mantissa = (double) bytes;
  size_t nb_div = 0;
  for (; mantissa > 1000.0 && nb_div < ARRAY_SIZE(div_suffix); nb_div++) {
#if defined(ARCH_WIN32)
    mantissa /= 1024;
#else
    mantissa /= 1000;
#endif
  }

  lbc_printf(lbc_str("%.1f %s"), mantissa, div_suffix[nb_div]);
}

int mainMux(
  const LibbluMuxingSettings *settings
)
{
  LIBBLU_DEBUG_COM("Verbose output activated.\n");
  bool is_debug_mode = isDebugEnabledLibbbluStatus() /* || true */;

  LibbluMuxingContext ctx;
  if (initLibbluMuxingContext(&ctx, settings))
    return -1;

  BitstreamWriterPtr output = createBitstreamWriterDefBuf(settings->output_ts_filename);
  if (NULL == output)
    goto free_return;

  /* Mux packets while remain data */
  while (dataRemainingLibbluMuxingContext(ctx)) {
    if (muxNextPacketLibbluMuxingContext(&ctx, output) < 0)
      goto free_return;

    if (!is_debug_mode)
      _printProgressBar(ctx.progress);
  }

  /* Padding aligned unit (= 32 TS p.) with NULL packets : */
  if (padAlignedUnitLibbluMuxingContext(&ctx, output) < 0)
    goto free_return;

  fprintf(stderr, "STC_start = %" PRIu64 "\n", ctx.STC_start);
  fprintf(stderr, "STC_end   = %" PRIu64 "\n", ctx.STC_end);
  fprintf(stderr, "PTS_start = %" PRIu64 "\n", ctx.PTS_start);
  fprintf(stderr, "PTS_end   = %" PRIu64 "\n", ctx.PTS_end);
  fprintf(stderr, "PU_dur    = %" PRIu64 "\n", ctx.PU_dur);

  lbc_printf(lbc_str("Multiplexing... [====================] 100%% Finished !\n\n"));

  closeBitstreamWriter(output);

  lbc_printf(
    lbc_str("== Multiplexing summary ===============================================================\n")
  );
  lbc_printf(lbc_str("Muxed: %u packets ("), ctx.nb_tp_muxed);
  printBytes(ctx.nb_bytes_written);
  lbc_printf(lbc_str(", %zu bytes).\n"), ctx.nb_bytes_written);

  unsigned nb_sys_tp = ctx.nb_tp_muxed;
  for (unsigned i = 0; i < ctx.nb_elementary_streams; i++) {
    lbc_printf(
      lbc_str(" - 0x%04x : %d packets (%.1f%%);\n"),
      ctx.elementary_streams[i]->pid,
      ctx.elementary_streams[i]->nb_transport_packets,
      100.f * ctx.elementary_streams[i]->nb_transport_packets / ctx.nb_tp_muxed
    );
    nb_sys_tp -= ctx.elementary_streams[i]->nb_transport_packets;
  }
  lbc_printf(
    lbc_str(" - System packets : %d packets (%.1f%%).\n"),
    nb_sys_tp, 100.f * nb_sys_tp / ctx.nb_tp_muxed
  );

  lbc_printf(
    lbc_str("=======================================================================================\n")
  );
  cleanLibbluMuxingContext(ctx);

  return 0;

free_return:
  closeBitstreamWriter(output);
  cleanLibbluMuxingContext(ctx);
  return -1;
}