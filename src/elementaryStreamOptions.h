

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
  bool force_script_generation;  /**< Forced script building. */
  bool dvd_media;
  bool secondary;         /**< Set to true if ES must be muxed as a
    secondary stream.                                                        */
  bool disable_fixes;

  struct {
    bool extract_core;
    lbc *pbr_filepath;
  };  /**< Audio codecs related options.                                     */

  struct {
    bool second_pass;  /**< Is the second parsing attempt. */
    bool half_PicOrderCnt;  /**< Divide by two the computed PicOrderCnt of
      each picture. This option is automatically triggered.                  */
    int64_t initDecPicNbCntShift;  /**< Initial decoding timestamp shifting
      in pictures units (1 / frame-rate). This option is automatically
      adjusted.                                                              */

    HdmvFrameRateCode fps_mod;
    LibbluAspectRatioMod ar_mod;
    uint8_t level_mod;
    bool force_rebuild_sei;
    bool discard_sei;

    bool disable_HRD_verifier;
    lbc *HRD_CPB_stats_log_fp;
    lbc *HRD_DPB_stats_log_fp;
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
  const lbc *expr,
  HdmvFrameRateCode *val
);

int parseArChange(
  const lbc *expr,
  LibbluAspectRatioMod *val
);

int parseLevelChange(
  const lbc *expr,
  uint8_t *val
);

#endif