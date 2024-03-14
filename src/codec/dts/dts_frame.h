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

static inline const char *DtsAUInnerTypeStr(
  DtsAUInnerType type
)
{
  static const char *strings[] = {
    "Core SS",
    "Ext SS Header",
    "Ext SS Asset"
  };

  if (type < ARRAY_SIZE(strings))
    return strings[type];
  return "Unknown";
}

typedef union {
  DcaExtSSHeaderParameters ext_ss_hdr;
  DcaXllFrameSFPosition ext_ss_asset;
} DtsAUInnerReplacementParam;

typedef enum {
  DTS_AU_KEEP,
  DTS_AU_SKIP,
  DTS_AU_REPLACE
} DtsAUCellTreatment;

static inline const char *DtsAUCellTreatmentStr(
  DtsAUCellTreatment treatment
)
{
  static const char *strings[] = {
    "keep",
    "skip",
    "replace"
  };

  if (treatment < ARRAY_SIZE(strings))
    return strings[treatment];
  return "Unknown";
}

typedef struct {
  DtsAUInnerType type;

  int64_t offset;
  uint32_t size;

  DtsAUCellTreatment treatment;
  DtsAUInnerReplacementParam *param;
} DtsAUCell, *DtsAUCellPtr;

typedef struct {
  DtsAUCell *contentCells;
  unsigned nbUsedContentCells;
  unsigned nbAllocatedContentCells;

  bool initializedCell;

  DtsAUInnerReplacementParam *replacementParams; // TODO: Simplify this.
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

uint32_t getSizeDtsAUFrame(
  const DtsAUFramePtr frm
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
  EsmsHandlerPtr script,
  DtsAUFramePtr frm,
  unsigned srcFileScriptIdx,
  uint64_t framePts
);

void discardWholeCurDtsAUFrame(
  DtsAUFramePtr frm
);

#endif