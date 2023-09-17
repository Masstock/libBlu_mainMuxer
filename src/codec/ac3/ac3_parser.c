#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "ac3_parser.h"


static int _parseAc3CoreSyncFrame(
  Ac3Context * ctx
)
{

  LIBBLU_AC3_DEBUG(
    "0x%08" PRIX64 " === AC3 Core sync frame ===\n",
    ctx->cur_au.start_offset
  );

  /* syncinfo() */
  Ac3SyncInfoParameters syncinfo;
  if (parseAc3SyncInfo(ctx->bs, &syncinfo) < 0)
    return -1;

  if (ctx->core.nb_frames) {
    // TODO
  }
  else {
    if (checkAc3SyncInfoCompliance(&syncinfo) < 0)
      return -1;
    ctx->core.syncinfo = syncinfo;
  }

  if (fillAc3BitReaderAc3Context(ctx, &syncinfo) < 0)
    return -1;

  /* bsi() */
  Ac3BitStreamInfoParameters bsi;
  if (parseAc3BitStreamInfo(&ctx->br, &bsi) < 0)
    return -1;

  if (ctx->core.nb_frames) {
    // TODO
  }
  else {
    if (checkAc3BitStreamInfoCompliance(&bsi) < 0)
      return -1;
    ctx->core.bsi = bsi;
  }

  skipLibbluBitReader(&ctx->br, remainingBitsLibbluBitReader(&ctx->br));

  if (completeFrameAc3Context(ctx) < 0)
    return -1;

  ctx->core.pts += ((uint64_t) MAIN_CLOCK_27MHZ * AC3_SAMPLES_PER_FRAME) / 48000;
  ctx->core.nb_frames++;
  return 0;
}


static int _parseEac3SyncFrame(
  Ac3Context * ctx
)
{
  LIBBLU_AC3_DEBUG(
    "0x%08" PRIX64 " === EAC-3 Extension sync frame ===\n",
    ctx->cur_au.start_offset
  );

  if (fillEac3BitReaderAc3Context(ctx) < 0)
    return -1;

  /* syncinfo() */
  if (parseEac3SyncInfo(&ctx->br) < 0)
    return -1;

  /* bsi() */
  Eac3BitStreamInfoParameters bsi = {0};
  if (parseEac3BitStreamInfo(&ctx->br, &bsi) < 0)
    return -1;

  if (0 < ctx->eac3.nb_frames) {
    if (memcmp(&ctx->eac3.bsi, &bsi, sizeof(Eac3BitStreamInfoParameters))) {
      if (checkChangeEac3BitStreamInfoCompliance(&ctx->eac3.bsi, &bsi) < 0)
        return -1;
      memcpy(&ctx->eac3.bsi, &bsi, sizeof(Eac3BitStreamInfoParameters));
    }
  }
  else {
    if (checkEac3BitStreamInfoCompliance(&bsi) < 0)
      return -1;
    memcpy(&ctx->eac3.bsi, &bsi, sizeof(Eac3BitStreamInfoParameters));
  }

  skipLibbluBitReader(&ctx->br, remainingBitsLibbluBitReader(&ctx->br));
  // if (_parseRemainingEac3Frame(bs, bsi, start_off) < 0)
  //   return -1;

  if (completeFrameAc3Context(ctx) < 0)
    return -1;

  ctx->eac3.pts += ((uint64_t) MAIN_CLOCK_27MHZ * AC3_SAMPLES_PER_FRAME) / 48000;
  ctx->eac3.nb_frames++;
  return 0;
}


static int _parseMlpSyncFrame(
  Ac3Context * ctx
)
{
  MlpParsingContext * mlp = &ctx->mlp;

  LIBBLU_AC3_DEBUG(
    "0x%08" PRIX64 " === MLP/TrueHD Access Unit ===\n",
    ctx->cur_au.start_offset
  );

  if (mlp->terminator_reached) {
    LIBBLU_MLP_WARNING("Unexpected Access Unit, previous one indicated end-of-stream.\n");

    MlpMajorSyncInfoParameters * msi = &mlp->mlp_sync_header.major_sync_info;
    for (unsigned i = 0; i < msi->substreams; i++) {
      mlp->substreams[i].restart_header_seen = false;
      mlp->substreams[i].terminator_reached  = false;
    }
  }

  /* mlp_sync_header */
  MlpSyncHeaderParameters mlp_sync_header;
  LIBBLU_MLP_DEBUG_PARSING_HDR(" MLP Sync, mlp_sync_header\n");
  uint8_t ms_parity;
  if (parseMlpMinorSyncHeader(ctx->bs, &mlp_sync_header, &ms_parity) < 0)
    return -1;

  if (fillMlpBitReaderAc3Context(ctx, &mlp_sync_header) < 0)
    return -1;

  /* Check if current frame contain Major Sync */
  uint32_t next_dword;
  if (getLibbluBitReader(&ctx->br, &next_dword, 32) < 0)
    return -1;
  mlp_sync_header.major_sync = ((next_dword >> 8) == MLP_SYNCWORD_PREFIX);

  if (mlp_sync_header.major_sync) {
    if (parseMlpMajorSyncInfo(&ctx->br, &mlp_sync_header.major_sync_info) < 0)
      return -1;
  }

  if (mlp->nb_frames && !mlp->terminator_reached) {
    // TODO: Constant check

    updateMlpSyncHeaderParametersParameters(
      &mlp->mlp_sync_header,
      mlp_sync_header
    );
  }
  else {
    mlp->terminator_reached = false;

    if (checkMlpSyncHeader(&mlp_sync_header, &mlp->info, true) < 0)
      return -1;

    // TODO: Check compliance (only if !ctx.extractCore)

    if (MLP_MAX_NB_SS < mlp_sync_header.major_sync_info.substreams) {
      LIBBLU_ERROR_RETURN("Unsupported number of substreams.\n"); // TODO
    }
    ctx->mlp.mlp_sync_header = mlp_sync_header;
  }

  LIBBLU_MLP_DEBUG_PARSING_HDR(
    "  Substream directory, substream_directory\n"
  );

  // substream_directory
  uint8_t ssdir_offset = offsetLibbluBitReader(&ctx->br);
  if (decodeMlpSubstreamDirectory(&ctx->br, mlp) < 0)
    return -1;

  size_t ssdir_size = offsetLibbluBitReader(&ctx->br) - ssdir_offset;
  uint8_t ssdir_parity = computeParityLibbluBitReader(
    &ctx->br, ssdir_offset, ssdir_size
  );

  uint8_t parity = ms_parity ^ ssdir_parity;
  parity = (parity ^ (parity >> 4)) & 0xF;
  if (0xF != parity)
    LIBBLU_MLP_ERROR_RETURN("Unexpected sync header checksum (check_nibble).\n");

  LIBBLU_MLP_DEBUG_PARSING_SS(
    "  Substream data, start\n"
  );

  // substream_data
  if (decodeMlpSubstreamSegments(&ctx->br, mlp) < 0)
    return -1;

  /* extra_start */
  if (remainingBitsLibbluBitReader(&ctx->br)) {
    if (decodeMlpExtraData(&ctx->br) < 0)
      return -1;
  }
  else {
    LIBBLU_MLP_DEBUG_PARSING_SS(
      "  No EXTRA_DATA.\n"
    );
  }

  if (completeFrameAc3Context(ctx) < 0)
    return -1;

  mlp->pts += ((uint64_t) MAIN_CLOCK_27MHZ) / TRUE_HD_UNITS_PER_SEC;
  mlp->nb_frames++;
  return 0;
}


static int _parseNextFrame(
  Ac3Context * ctx
)
{
  switch (initNextFrameAc3Context(ctx)) {
    case AC3_CORE:
      return _parseAc3CoreSyncFrame(ctx);
    case AC3_EAC3:
      return _parseEac3SyncFrame(ctx);
    case AC3_TRUEHD:
      return _parseMlpSyncFrame(ctx);
  }

  LIBBLU_AC3_DEBUG_UTIL("Unable to parse next frame.\n");
  return -1;
}


int analyzeAc3(
  LibbluESParsingSettings * settings
)
{
  Ac3Context ctx;

  if (initAc3Context(&ctx, settings) < 0)
    return -1;

  while (!isEofAc3Context(&ctx)) {
    printFileParsingProgressionBar(ctx.bs);

    if (_parseNextFrame(&ctx) < 0)
      return -1;
  }

  if (completeAc3Context(&ctx) < 0)
    return -1;

  return 0;
}
