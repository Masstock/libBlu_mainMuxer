/** \~english
 * \file dts_frame.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief DTS audio bitstreams frame building handling module.
 */

#ifndef __LIBBLU_MUXER__CODECS__DTS__FRAME_H__
#define __LIBBLU_MUXER__CODECS__DTS__FRAME_H__

#include "../../util.h"
#include "../../esms/scriptCreation.h"

#include "dts_error.h"
#include "dts_patcher.h"
#include "dts_data.h"

typedef enum {
  DTS_FRM_INNER_CORE_SS,
  DTS_FRM_INNER_EXT_SS_HDR,
  DTS_FRM_INNER_EXT_SS_ASSET
} DtsAUInnerType;

typedef union {
  DcaExtSSHeaderParameters ext_ss_hdr;
  DcaXllFrameSFPosition ext_ss_asset;
} DtsAUInnerReplacementParam;

typedef struct {
  DtsAUInnerType type;

  int64_t startOffset;
  int64_t length;

  bool skip;
  bool replace;

  DtsAUInnerReplacementParam * param;
} DtsAUCell, *DtsAUCellPtr;

static inline void setPositionDtsAUCell(
  DtsAUCellPtr cell,
  uint64_t src_file_offset,
  uint32_t size
)
{
  cell->startOffset = src_file_offset;
  cell->length = size;
}

typedef struct {
  DtsAUCell * contentCells;
  unsigned nbUsedContentCells;
  unsigned nbAllocatedContentCells;

  bool initializedCell;

  DtsAUInnerReplacementParam * replacementParams;
  unsigned nbUsedReplacementParam;
  unsigned nbAllocatedReplacementParam;
} DtsAUFrame, *DtsAUFramePtr;

static inline void cleanDtsAUFrame(
  DtsAUFrame frm
)
{
  free(frm.contentCells);
  free(frm.replacementParams);
}

DtsAUCellPtr initDtsAUCell(
  DtsAUFramePtr frm,
  DtsAUInnerType type
);

static inline DtsAUCellPtr recoverCurDtsAUCell(
  DtsAUFramePtr frm
)
{
  if (!frm->initializedCell)
    LIBBLU_DTS_ERROR_NRETURN("AU cell never initialized.\n");
  return &frm->contentCells[frm->nbUsedContentCells];
}

int replaceCurDtsAUCell(
  DtsAUFramePtr frm,
  DtsAUInnerReplacementParam param
);

int discardCurDtsAUCell(
  DtsAUFramePtr frm
);

int addCurCellToDtsAUFrame(
  DtsAUFramePtr frm
);

typedef enum {
  DTS_AU_EMPTY    = -1,
  DTS_AU_CORE_SS  =  0,
  DTS_AU_EXT_SS   =  1
} DtsAUContentType;

DtsAUContentType identifyContentTypeDtsAUFrame(
  DtsAUFramePtr frm
);

int processCompleteFrameDtsAUFrame(
  BitstreamWriterPtr output,
  EsmsFileHeaderPtr script,
  DtsAUFramePtr frm,
  unsigned srcFileScriptIdx,
  uint64_t framePts
);

void discardWholeCurDtsAUFrame(
  DtsAUFramePtr frm
);

#endif