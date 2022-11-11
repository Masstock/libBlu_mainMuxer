

#ifndef __LIBBLU_MUXER__CODECS__IGS_COMPILER__CONTEXT_DATA_H__
#define __LIBBLU_MUXER__CODECS__IGS_COMPILER__CONTEXT_DATA_H__

#include "igs_compiler_data.h"
#include "../common/hdmv_timecodes.h"
#include "../common/hdmv_pictures_io.h"
#include "../../../ini/iniData.h"

#define GET_COMP(ctx, id)                         \
  ((ctx)->data.compositions[id])

#define CUR_COMP(ctx)                             \
  GET_COMP(ctx, (ctx)->data.nbCompo-1)

typedef struct {
  IniFileContextPtr conf;
  HdmvPictureLibraries imgLibs;
  XmlCtxPtr xmlCtx;
  HdmvSegmentsInventoryPtr inv;
  HdmvTimecodes * timecodes;

  lbc initialWorkingDir[PATH_BUFSIZE];
  lbc * workingDir;
  lbc * xmlFilename;

  IgsCompilerData data;
  IgsCompilerSegment * seg;
} IgsCompilerContext, *IgsCompilerContextPtr;

static inline int addTimecodeIgsCompilerContext(
  IgsCompilerContextPtr ctx,
  uint64_t value
)
{
  return addHdmvTimecodes(
    ctx->timecodes,
    value
  );
}

#endif