

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
  IniFileContext conf_hdl;
  HdmvPictureLibraries img_io_libs;
  XmlCtxPtr xml_ctx;
  HdmvTimecodes * timecodes;

  lbc * init_working_dir;
  lbc * cur_working_dir;
  lbc * xml_filename;

  IgsCompilerData data;
  IgsCompilerSegment * seg;
} IgsCompilerContext;

#endif