

#ifndef __LIBBLU_MUXER__CODECS__IGS_COMPILER__CONTEXT_DATA_H__
#define __LIBBLU_MUXER__CODECS__IGS_COMPILER__CONTEXT_DATA_H__

#include "igs_compiler_data.h"
#include "../common/hdmv_timecodes.h"
#include "../common/hdmv_bitmap_io.h"
#include "../../../ini/iniData.h"

#define GET_COMP(ctx, id)                         \
  (&(ctx)->data.compositions[id])

#define CUR_COMP(ctx)                             \
  GET_COMP(ctx, (ctx)->data.nb_compositions-1)

typedef struct {
  IniFileContextPtr conf;
  HdmvPictureLibraries img_io_libs;
  XmlCtx xml_ctx;
  HdmvSegmentsInventory inv;
  HdmvTimecodes * timecodes;

  lbc initial_working_dir[PATH_BUFSIZE];
  lbc * cur_working_dir;
  lbc * xml_filename;

  IgsCompilerData data;
  IgsCompilerSegment * seg;
} IgsCompilerContext;

static inline int addTimecodeIgsCompilerContext(
  IgsCompilerContext * ctx,
  uint64_t value
)
{
  return addHdmvTimecodes(
    ctx->timecodes,
    value
  );
}

#endif