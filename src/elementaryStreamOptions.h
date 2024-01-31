

#ifndef __LIBBLU_MUXER__ELEMENTARY_STREAM_OPTIONS_H__
#define __LIBBLU_MUXER__ELEMENTARY_STREAM_OPTIONS_H__

/* #include "util.h" */
#include "elementaryStreamProperties.h"
#include "ini/iniData.h"
#include "codec/h264/h264_data.h"
#include "esms/scriptData.h"

typedef struct {
  H264AspectRatioIdcValue idc;
  unsigned x;
  unsigned y;
} LibbluAspectRatioMod;

static inline bool isUsedLibbluAspectRatioMod(
  LibbluAspectRatioMod param
)
{
  return param.x != 0;
}

typedef struct {
  IniFileContext conf_hdl;
  bool forcedScriptBuilding;  /**< Forced script building. */
  bool dvdMedia;
  bool secondaryStream;         /**< Set to true if ES must be muxed as a
    secondary stream.                                                        */
  bool disableFixes;

  struct {
    bool extractCore;
    lbc * pbrFilepath;
  };  /**< Audio codecs related options.                                     */

  struct {
    bool secondPass;  /**< Is the second parsing attempt. */
    bool halfPicOrderCnt;  /**< Divide by two the computed PicOrderCnt of
      each picture. This option is automatically triggered.                  */
    int64_t initDecPicNbCntShift;  /**< Initial decoding timestamp shifting
      in pictures units (1 / frame-rate). This option is automatically
      adjusted.                                                              */

    HdmvFrameRateCode fpsChange;
    LibbluAspectRatioMod arChange;
    uint8_t levelChange;
    bool forceRebuildSei;
    bool discardSei;

    bool disableHrdVerifier;
    lbc * hrdCpbStatsFilepath;
    lbc * hrdDpbStatsFilepath;
  };  /**< Video codecs related options.                                     */

  struct {
    int64_t initial_timestamp;
    bool force_retiming;
    bool ass_input;
  } hdmv;  /**< HDMV codecs related options. */
} LibbluESSettingsOptions;

uint64_t computeFlagsLibbluESSettingsOptions(
  LibbluESSettingsOptions options
);

int parseFpsChange(
  const lbc * expr,
  HdmvFrameRateCode * val
);

int parseArChange(
  const lbc * expr,
  LibbluAspectRatioMod * val
);

int parseLevelChange(
  const lbc * expr,
  uint8_t * val
);

#endif