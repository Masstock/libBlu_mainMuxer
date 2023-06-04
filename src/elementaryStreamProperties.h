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
  uint8_t constraintFlags;

  uint32_t cpbSize;  /**< CpbSize[cpb_cnt_minus1] value from SPS VUI.        */
  uint32_t bitrate;  /**< BitRate[0] value from SPS VUI.                     */
} LibbluESH264SpecProperties;

/** \~english
 * \brief Elementary Stream format specific properties common union.
 *
 */
typedef union {
  void * sharedPtr;                   /**< Shared pointer for manipulations. */
  LibbluESAc3SpecProperties * ac3;    /**< AC-3 audio and
    derived.                                                                 */
  LibbluESH264SpecProperties * h264;  /**< H.264 video.                      */
} LibbluESFmtSpecProp;

/** \~english
 * \brief Elementary Stream format specific properties type.
 *
 */
typedef enum {
  FMT_SPEC_INFOS_NONE,
  FMT_SPEC_INFOS_AC3,
  FMT_SPEC_INFOS_H264,

  NB_FMT_SPEC_PROP_TYPES
} LibbluESFmtSpecPropType;

static inline size_t sizeofLibbluESFmtSpecPropType(
  const LibbluESFmtSpecPropType type
)
{
  static const size_t typeAllocationSizes[] = {
    0,
    sizeof(LibbluESAc3SpecProperties),
    sizeof(LibbluESH264SpecProperties)
  };

  if (NB_FMT_SPEC_PROP_TYPES <= type)
    return 0;
  return typeAllocationSizes[type];
}

int initLibbluESFmtSpecProp(
  LibbluESFmtSpecProp * dst,
  LibbluESFmtSpecPropType type
);

typedef enum {
  HDMV_VIDEO_FORMAT_RES    = 0x0,
  HDMV_VIDEO_FORMAT_480I   = 0x1,
  HDMV_VIDEO_FORMAT_576I   = 0x2,
  HDMV_VIDEO_FORMAT_480P   = 0x3,
  HDMV_VIDEO_FORMAT_1080I  = 0x4,
  HDMV_VIDEO_FORMAT_720P   = 0x5,
  HDMV_VIDEO_FORMAT_1080P  = 0x6,
  HDMV_VIDEO_FORMAT_576P   = 0x7,
  HDMV_VIDEO_FORMAT_2160P  = 0x8
} HdmvVideoFormat;

HdmvVideoFormat getHdmvVideoFormat(
  unsigned width,
  unsigned height,
  bool isInterlaced
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

/** \~english
 * \brief Convert a frame-rate value to its HDMV code.
 *
 * \param frameRate Floating point frame-rate value.
 * \return HdmvFrameRateCode If value is part of HDMV allowed frame-rate values,
 * assigned code is returned. Otherwise, a negative value is returned.
 */
HdmvFrameRateCode getHdmvFrameRateCode(
  float frameRate
);

/** \~english
 * \brief Return the frame-rate value represented by given HDMV code as a
 * double precision floating point.
 *
 * \param code HDMV frame-rate code to convert.
 * \return float If valid, double precision floating point frame-rate value
 * is returned. Otherwise, a negative value is returned.
 */
double frameRateCodeToDouble(
  HdmvFrameRateCode code
);

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

typedef enum {
  SAMPLE_RATE_CODE_48000   = 0x01,
  SAMPLE_RATE_CODE_96000   = 0x04,
  SAMPLE_RATE_CODE_192000  = 0x05
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
  HdmvFrameRateCode frameRate,
  bool interlaced
);

CheckVideoConfigurationRet checkSecVideoConfiguration(
  unsigned width,
  unsigned height,
  HdmvFrameRateCode frameRate,
  bool interlaced
);

/** \~english
 * \brief Elementary Stream properties.
 *
 */
typedef struct {
  LibbluESType type;
  LibbluStreamCodingType codingType;        /**< Effective stream coding type.     */

  union {
    struct {
      HdmvVideoFormat videoFormat;    /**< Video stream format value.        */
      HdmvFrameRateCode frameRate;        /**< Video stream frame-rate value.    */
      bool stillPicture;              /**< Video stream is a still picture.  */

      uint8_t profileIDC;             /**< Video stream profile indice.      */
      uint8_t levelIDC;               /**< Video stream level indice.        */
    };  /**< If type == VIDEO.                                               */

    struct {
      uint8_t audioFormat;            /**< Audio stream format value.        */
      SampleRateCode sampleRate;      /**< Audio stream sample-rate value.   */
      uint8_t bitDepth;               /**< Audio stream bit-depth value.     */
    };  /**< If type == AUDIO.                                               */
  };

  double bitrate;                     /**< Stream nominal max bitrate in bits
    per second.                                                              */
  double pesNb;                       /**< Constant number of PES frames per
    second.  */
  double pesNbSec;                    /**< Constant number of secondary PES
    frames per second used when secStream == true.                           */
  bool bitRateBasedDurationAlternativeMode;  /**< If true, pesNb and pesNbSec
    fields are used to know how many PES frames are used per second.
    Otherwise, this value is defined according to current PES frame size
    compared to bitrate.                                                     */
} LibbluESProperties;

#endif