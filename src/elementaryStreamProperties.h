/** \~english
 * \file elementaryStreamProperties.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief
 *
 * \todo Rewrite checkPrimVideoConfiguration and checkSecVideoConfiguration.
 */

#ifndef __LIBBLU_MUXER__ELEMENTARY_STREAM_PROPERTIES_H__
#define __LIBBLU_MUXER__ELEMENTARY_STREAM_PROPERTIES_H__

#include "streamCodingType.h"
#include "util.h"

/** \~english
 * \brief #LibbluES AC-3 audio specific properties.
 *
 * Elementary Streams properties specific for AC-3 and derived formats.
 */
typedef struct {
  uint8_t sample_rate_code;  /**< Sample Rate code.                          */
  uint8_t bsid;              /**< Bit Stream Identification.                 */
  uint8_t bit_rate_code;     /**< Bit rate code.                             */
  uint8_t surround_mode;     /**< Surround mode.                             */
  uint8_t bsmod;             /**< Bit Stream Mode.                           */
  uint8_t num_channels;      /**< Number of channels.                        */
  uint8_t full_svc;          /**< Full service.                              */
} LibbluESAc3SpecProperties;

/** \~english
 * \brief #LibbluES H.264 video specific properties.
 *
 * Elementary Streams properties specific for H.264 formats.
 */
typedef struct {
  uint8_t constraint_flags;

  uint32_t CpbSize;  /**< CpbSize[cpb_cnt_minus1] value from SPS VUI.        */
  uint32_t BitRate;  /**< BitRate[0] value from SPS VUI.                     */
} LibbluESH264SpecProperties;

/** \~english
 * \brief Elementary Stream format specific properties common union.
 *
 */
typedef union {
  void *shared_ptr;                  /**< Shared pointer for manipulations. */
  LibbluESAc3SpecProperties *ac3;    /**< AC-3 audio and
    derived.                                                                 */
  LibbluESH264SpecProperties *h264;  /**< H.264 video.                      */
} LibbluESFmtProp;

/** \~english
 * \brief Elementary Stream format specific properties type.
 *
 */
typedef enum {
  FMT_SPEC_INFOS_NONE,  /**< No format specific properties.                  */
  FMT_SPEC_INFOS_AC3,   /**< Dolby audio formats specific properties.        */
  FMT_SPEC_INFOS_H264,  /**< H.264 video format specific properties.         */
} LibbluESFmtSpecPropType;

static inline size_t sizeofLibbluESFmtSpecPropType(
  const LibbluESFmtSpecPropType type
)
{
  static const size_t type_alloc_sizes[] = {
    0,
    sizeof(LibbluESAc3SpecProperties),
    sizeof(LibbluESH264SpecProperties)
  };

  if (sizeof(type_alloc_sizes) <= type)
    return 0;
  return type_alloc_sizes[type];
}

int initLibbluESFmtSpecProp(
  LibbluESFmtProp *dst,
  LibbluESFmtSpecPropType type
);

typedef enum {
  HDMV_VIDEO_FORMAT_480I   = 0x1,
  HDMV_VIDEO_FORMAT_576I   = 0x2,
  HDMV_VIDEO_FORMAT_480P   = 0x3,
  HDMV_VIDEO_FORMAT_1080I  = 0x4,
  HDMV_VIDEO_FORMAT_720P   = 0x5,
  HDMV_VIDEO_FORMAT_1080P  = 0x6,
  HDMV_VIDEO_FORMAT_576P   = 0x7,
  HDMV_VIDEO_FORMAT_2160P  = 0x8
} HdmvVideoFormat;

static inline const char *HdmvVideoFormatStr(
  HdmvVideoFormat video_format
)
{
  static const char *strings[] = {
    "reserved",
    "480i",
    "576i",
    "480p",
    "1080i",
    "720p",
    "1080p",
    "576p",
    "2160p"
  };

  if (video_format < ARRAY_SIZE(strings))
    return strings[video_format];
  return "reserved";
}

HdmvVideoFormat getHdmvVideoFormat(
  unsigned width,
  unsigned height,
  bool is_interlaced
);

/** \~english
 * \brief HDMV frame-rate codes.
 */
typedef enum {
  FRAME_RATE_CODE_UNSPC  = 0x00,  /**< Special value 'unspecified'           */

  FRAME_RATE_CODE_23976  = 0x01,  /**< 23.976 (24000/1001)                   */
  FRAME_RATE_CODE_24     = 0x02,  /**< 24                                    */
  FRAME_RATE_CODE_25     = 0x03,  /**< 25                                    */
  FRAME_RATE_CODE_29970  = 0x04,  /**< 29.970 (30000/1001)                   */
  FRAME_RATE_CODE_50     = 0x06,  /**< 50                                    */
  FRAME_RATE_CODE_59940  = 0x07   /**< 59.940 (60000/1001)                   */
} HdmvFrameRateCode;

static inline const char *HdmvFrameRateCodeStr(
  HdmvFrameRateCode frame_rate_code
)
{
  static const char *strings[] = {
    "unspecified",
    "23.976 (24000/1001)",
    "24",
    "25",
    "29.970 (30000/1001)",
    "50",
    "59.940 (60000/1001)"
  };

  if (frame_rate_code < ARRAY_SIZE(strings))
    return strings[frame_rate_code];
  return "reserved";
}

/** \~english
 * \brief Convert a frame-rate value to its HDMV code.
 *
 * \param frame_rate Floating point frame-rate value.
 * \return HdmvFrameRateCode If value is part of HDMV allowed frame-rate values,
 * assigned code is returned. Otherwise, a negative value is returned.
 */
HdmvFrameRateCode getHdmvFrameRateCode(
  float frame_rate
);

/** \~english
 * \brief Return the frame-rate value represented by given HDMV code as a
 * double precision floating point.
 *
 * \param code HDMV frame-rate code to convert.
 * \return float If valid, double precision floating point frame-rate value
 * is returned. Otherwise, a negative value is returned.
 */
static inline double frameRateCodeToDouble(
  HdmvFrameRateCode code
)
{
  static const double frame_rate_values[] = {
    -1.0, /* < Reserved */
    24000.0 / 1001.0,
    24.0,
    25.0,
    30000.0 / 1001.0,
    -1.0, /* < Reserved */
    50.0,
    50000.0 / 1001.0
  };

  if (code < ARRAY_SIZE(frame_rate_values))
    return frame_rate_values[code];
  return -1.0;
}

/** \~english
 * \brief Return the frame-rate value represented by given HDMV code.
 *
 * \param code HDMV frame-rate code to convert.
 * \return float If valid, floating point frame-rate value is returned.
 * Otherwise, a negative value is returned.
 */
static inline float frameRateCodeToFloat(
  HdmvFrameRateCode code
)
{
  return (float) frameRateCodeToDouble(code);
}

static inline int64_t frameDur27MHzHdmvFrameRateCode(
  HdmvFrameRateCode code
)
{
  static const int64_t frame_duration_values[] = {
    0,                                   // reserved
    1001ll * MAIN_CLOCK_27MHZ / 24000ll, // 23.976
    1000ll * MAIN_CLOCK_27MHZ / 24000ll, // 24
    1000ll * MAIN_CLOCK_27MHZ / 25000ll, // 25
    1001ll * MAIN_CLOCK_27MHZ / 30000ll, // 29.970
    0,                                   // reserved
    1000ll * MAIN_CLOCK_27MHZ / 50000ll, // 50
    1001ll * MAIN_CLOCK_27MHZ / 60000ll  // 59.940
  };

  if (code < ARRAY_SIZE(frame_duration_values))
    return frame_duration_values[code];
  return 0;
}

static inline int64_t frameDur90kHzHdmvFrameRateCode(
  HdmvFrameRateCode code
)
{
  static const int64_t frame_duration_values[] = {
    0,                                  // reserved
    1001ll * SUB_CLOCK_90KHZ / 24000ll, // 23.976
    1000ll * SUB_CLOCK_90KHZ / 24000ll, // 24
    1000ll * SUB_CLOCK_90KHZ / 25000ll, // 25
    1001ll * SUB_CLOCK_90KHZ / 30000ll, // 29.970
    0,                                  // reserved
    1000ll * SUB_CLOCK_90KHZ / 50000ll, // 50
    1001ll * SUB_CLOCK_90KHZ / 60000ll  // 59.940
  };

  if (code < ARRAY_SIZE(frame_duration_values))
    return frame_duration_values[code];
  return 0;
}

typedef enum {
  AUDIO_FORMAT_MONO                  = 0x1,
  AUDIO_FORMAT_STEREO                = 0x3,
  AUDIO_FORMAT_MULTI_CHANNEL         = 0x6,
  AUDIO_FORMAT_STEREO_MULTI_CHANNEL  = 0xC
} AudioFormatCode;

static inline const char *AudioFormatCodeStr(
  const AudioFormatCode code
)
{
  switch (code) {
  case AUDIO_FORMAT_MONO:
    return "Mono";
  case AUDIO_FORMAT_STEREO:
    return "Stereo";
  case AUDIO_FORMAT_MULTI_CHANNEL:
    return "Multi-channel";
  case AUDIO_FORMAT_STEREO_MULTI_CHANNEL:
    return "Stereo core + Multi-channel extension";
  }
  return "Unknown";
}

typedef enum {
  SAMPLE_RATE_CODE_48000   = 0x1,
  SAMPLE_RATE_CODE_96000   = 0x4,
  SAMPLE_RATE_CODE_192000  = 0x5
} SampleRateCode;

static inline unsigned valueSampleRateCode(
  const SampleRateCode code
)
{
  switch (code) {
  case SAMPLE_RATE_CODE_48000:
    return 48000;
  case SAMPLE_RATE_CODE_96000:
    return 96000;
  case SAMPLE_RATE_CODE_192000:
    return 19200;
  }
  return 0;
}

static inline double sampleRateCodeToDouble(
  const SampleRateCode code
)
{
  switch (code) {
  case SAMPLE_RATE_CODE_48000:
    return 48000.0;
  case SAMPLE_RATE_CODE_96000:
    return 96000.0;
  case SAMPLE_RATE_CODE_192000:
    return 19200.0;
  }
  return -1.0;
}

typedef enum {
  BIT_DEPTH_16_BITS  = 0x1,
  BIT_DEPTH_20_BITS  = 0x2,
  BIT_DEPTH_24_BITS  = 0x3
} BitDepthCode;

static inline unsigned valueBitDepthCode(
  const BitDepthCode code
)
{
  switch (code) {
  case BIT_DEPTH_16_BITS:
    return 16u;
  case BIT_DEPTH_20_BITS:
    return 20u;
  case BIT_DEPTH_24_BITS:
    return 24u;
  }
  return 0u;
}

typedef enum {
  CHK_VIDEO_CONF_RET_ILL_VIDEO_SIZE  = -1,  /**< Illegal video size.         */
  CHK_VIDEO_CONF_RET_ILL_FRAME_RATE  = -2,  /**< Illegal frame-rate for
    video size.                                                              */
  CHK_VIDEO_CONF_RET_ILL_DISP_MODE   = -3,  /**< Illegal progressive/
    interlacing for this video configuration.                                */

  CHK_VIDEO_CONF_RET_OK              = 0    /**< OK.                         */
} CheckVideoConfigurationRet;

CheckVideoConfigurationRet checkPrimVideoConfiguration(
  unsigned width,
  unsigned height,
  HdmvFrameRateCode frame_rate,
  bool interlaced
);

CheckVideoConfigurationRet checkSecVideoConfiguration(
  unsigned width,
  unsigned height,
  HdmvFrameRateCode frame_rate,
  bool interlaced
);

/** \~english
 * \brief Elementary Stream properties.
 *
 */
typedef struct {
  LibbluESType type;
  LibbluStreamCodingType coding_type;  /**< Effective stream coding type.    */

  union {
    struct {
      HdmvVideoFormat video_format;   /**< Video stream format value.        */
      HdmvFrameRateCode frame_rate;   /**< Video stream frame-rate value.    */
      bool still_picture;             /**< Video stream is a still picture.  */

      uint8_t profile_IDC;            /**< Video stream profile indice.      */
      uint8_t level_IDC;              /**< Video stream level indice.        */
    };  /**< If type == VIDEO.                                               */

    struct {
      AudioFormatCode audio_format;   /**< Audio stream format value.        */
      SampleRateCode sample_rate;     /**< Audio stream sample-rate value.   */
      BitDepthCode bit_depth;         /**< Audio stream bit-depth value.     */
    };  /**< If type == AUDIO.                                               */
  };

  double bitrate;                     /**< Stream nominal max bitrate in bits
    per second.                                                              */
  double nb_pes_per_sec;              /**< Constant number of PES frames per
    second.  */
  double nb_ped_sec_per_sec;          /**< Constant number of secondary PES
    frames per second used when secStream == true.                           */
  bool br_based_on_duration;          /**< When true, nb_pes_per_sec and
    nb_ped_sec_per_sec fields are used to define how many PES frames are
    present per second. Otherwise, the number of PES frames is estimated
    according to current PES frame size compared to bitrate.                 */
} LibbluESProperties;

#endif