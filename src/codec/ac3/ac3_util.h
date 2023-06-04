

#ifndef __LIBBLU_MUXER__CODECS__AC3__UTIL_H__
#define __LIBBLU_MUXER__CODECS__AC3__UTIL_H__

#include "../../util.h"
#include "../../esms/scriptCreation.h"
#include "../common/esParsingSettings.h"

#include "ac3_error.h"
#include "eac3_util.h"
#include "mlp_util.h"

typedef struct {
  Ac3SyncInfoParameters syncinfo;
  Ac3BitStreamInfoParameters bsi;

  unsigned nb_frames;
  uint64_t pts;
} Ac3CoreContext;

typedef enum {
  AC3_CORE,
  AC3_EAC3,
  AC3_TRUEHD
} Ac3ContentType;

typedef struct {
  Ac3ContentType type;

  int64_t start_offset;
  size_t size;
} Ac3AccessUnit;

typedef struct {
  unsigned src_file_idx;
  BitstreamReaderPtr bs;
  BitstreamWriterPtr script_bs;
  EsmsFileHeaderPtr script;
  const lbc * script_fp;

  LibbluBitReader br;
  uint8_t * buffer;
  size_t buffer_size;

  Ac3CoreContext core;
  Eac3ExtensionContext eac3;
  MlpParsingContext mlp;

  bool skip_ext;
  bool contains_mlp;
  // bool isSecondaryStream;
  // bool skipExtensionSubstreams;
  // unsigned skippedFramesControl;
  // bool skipCurrentPeriod;

  Ac3AccessUnit cur_au;
} Ac3Context;

int initAc3Context(
  Ac3Context * ctx,
  LibbluESParsingSettings * settings
);

Ac3ContentType initNextFrameAc3Context(
  Ac3Context * ctx
);

/** \~english
 * \brief
 *
 * Shall be called after #parseAc3SyncInfo().
 *
 * \param ctx
 * \param si
 * \return int
 */
int fillAc3BitReaderAc3Context(
  Ac3Context * ctx,
  const Ac3SyncInfoParameters * si
);

/** \~english
 * \brief
 *
 * Shall be called at sync word first byte offset (not inclusive).
 *
 * \param ctx
 * \return int
 */
int fillEac3BitReaderAc3Context(
  Ac3Context * ctx
);

/** \~english
 * \brief
 *
 * Shall be called after #parseMlpMinorSyncHeader().
 *
 * \param ctx
 * \param sh
 * \return int
 */
int fillMlpBitReaderAc3Context(
  Ac3Context * ctx,
  const MlpSyncHeaderParameters * sh
);

int completeFrameAc3Context(
  Ac3Context * ctx
);

static inline bool isEofAc3Context(
  const Ac3Context * ctx
)
{
  return isEof(ctx->bs);
}

int completeAc3Context(
  Ac3Context * ctx
);

void cleanAc3Context(
  Ac3Context * ctx
);

#endif