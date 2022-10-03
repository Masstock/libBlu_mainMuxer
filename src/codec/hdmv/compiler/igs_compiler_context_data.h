

#ifndef __LIBBLU_MUXER__CODECS__IGS_COMPILER__CONTEXT_DATA_H__
#define __LIBBLU_MUXER__CODECS__IGS_COMPILER__CONTEXT_DATA_H__

#include "igs_compiler_data.h"
#include "../common/hdmv_pictures_io.h"
#include "../../../ini/iniData.h"

#define GET_COMP(ctx, id)                         \
  ((ctx)->data.compositions[id])

#define CUR_COMP(ctx)                             \
  GET_COMP(ctx, (ctx)->data.nbCompo)

typedef struct {
  IniFileContextPtr conf;
  HdmvPictureLibraries imgLibs;
  XmlCtxPtr xmlCtx;
  HdmvSegmentsInventoryPtr inv;

  lbc initialWorkingDir[PATH_BUFSIZE];
  lbc * workingDir;
  lbc * xmlFilename;

  IgsCompilerData data;
  IgsCompilerSegment * seg;
} IgsCompilerContext, *IgsCompilerContextPtr;

#endif