/** \~english
 * \file util.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief Muxer common utilities main header.
 */

#ifndef __LIBBLU_MUXER__UTIL_H__
#define __LIBBLU_MUXER__UTIL_H__

#include "util/common.h"
#include "util/macros.h"
#include "util/errorCodes.h"
#include "util/messages.h"
#include "util/crcLookupTables.h"
#include "util/hashTables.h"
#include "util/circularBuffer.h"
#include "util/bitStreamHandling.h"
#include "util/textFilesHandling.h"
#include "util/libraries.h"

#define PROG_INFOS  "libBlu Muxer Exp. 0.5"
#define PROG_CONF_FILENAME  lbc_str("settings.ini")

#define MAX_NB_STREAMS                                                       32

#define PAT_DELAY  (50 * 27000)
#define PMT_DELAY  (50 * 27000)
#define SIT_DELAY  (500 * 27000)
#define PCR_DELAY  (50  * 27000)

/* UI Parameters :                                                           */
#define PROGRESSION_BAR_LATENCY 200

/** \~english
 * \brief MPEG-TS main clock frequency (27MHz).
 *
 * Number of clock ticks per second.
 */
#define MAIN_CLOCK_27MHZ  ((uint64_t) 27000000)

/** \~english
 * \brief #MAIN_CLOCK_27MHZ clock to hours number.
 *
 * MAIN_CLOCK_HOURS(MAIN_CLOCK_27MHZ * 60 * 60) == 1.
 */
#define MAIN_CLOCK_HOURS(clk)                                                 \
  ((clk) / (MAIN_CLOCK_27MHZ * 3600) % 60)

/** \~english
 * \brief #MAIN_CLOCK_27MHZ clock to minutes number.
 *
 * MAIN_CLOCK_MINUTES(MAIN_CLOCK_27MHZ * 60) == 1.
 */
#define MAIN_CLOCK_MINUTES(clk)                                               \
  ((clk) / (MAIN_CLOCK_27MHZ * 60) % 60)

/** \~english
 * \brief #MAIN_CLOCK_27MHZ clock to seconds number.
 *
 * MAIN_CLOCK_SECONDS(MAIN_CLOCK_27MHZ) == 1.
 */
#define MAIN_CLOCK_SECONDS(clk)                                               \
  ((clk) / MAIN_CLOCK_27MHZ % 60)

/** \~english
 * \brief #MAIN_CLOCK_27MHZ clock to milliseconds number.
 *
 * MAIN_CLOCK_MILISECONDS(MAIN_CLOCK_27MHZ / 10) == 100.
 */
#define MAIN_CLOCK_MILISECONDS(clk)                                           \
  ((clk) / (MAIN_CLOCK_27MHZ / 1000) % 1000)

/** \~english
 * \brief MPEG-TS sub clock frequency (90kHz).
 */
#define SUB_CLOCK_90KHZ  (MAIN_CLOCK_27MHZ / 300)

/** \~english
 * \brief Rounds supplied 27MHz clock value to the highest 90kHz sub-clock
 * common tick.
 */
#define ROUND_90KHZ_CLOCK(x)                                  ((x) / 300 * 300)

/** \~english
 * \brief MPEG-TS packet header synchronization byte.
 */
#define TP_SYNC_BYTE  0x47

/** \~english
 * \brief MPEG-TS packet length in bytes.
 */
#define TP_SIZE  188

/** \~english
 * \brief MPEG-TS packet TS header length in bytes.
 */
#define TP_HEADER_SIZE  4

#define TP_PAYLOAD_SIZE  (TP_SIZE - TP_HEADER_SIZE)

/** \~english
 * \brief BDAV TP_extra_header length in bytes.
 */
#define TP_EXTRA_HEADER_SIZE  4

/* in b/s : */
#define BDAV_VIDEO_MAX_BITRATE                                         40000000
#define BDAV_VIDEO_MAX_BITRATE_SEC_V                                    7600000
#define BDAV_VIDEO_MAX_BITRATE_DVD_OUT                                 15000000
#define BDAV_UHD_VIDEO_MAX_BITRATE                                    100000000

/* In bytes : */
#define BDAV_VIDEO_MAX_BUFFER_SIZE                                    240000000
#define BDAV_VIDEO_MAX_BUFFER_SIZE_DVD_OUT                            120000000

typedef enum {
  TYPE_ES,

  TYPE_PAT,
  TYPE_PMT,
  TYPE_SIT,
  TYPE_PCR,
  TYPE_NULL,

  TYPE_ERROR = 0xFF
} StreamType;

const lbc * streamTypeStr(
  StreamType typ
);

static inline int isEsStreamType(
  StreamType typ
)
{
  return typ == TYPE_ES;
}

typedef struct {
  uint16_t primaryVideo;
  uint16_t rightView;
#if 0 /* Not implemented */
  uint16_t depthMap;
#endif
  uint16_t hdrView;

  uint16_t primaryAudio;
  uint16_t pg;
  uint16_t ig;
  uint16_t secAudio;
  uint16_t secVideo;
  uint16_t txtSubtitle;
} nbStreams;

#if 0

typedef struct {
  uint8_t subSampleRate;
  uint8_t bsid;
  bool bitrateMode;
  uint8_t bitrateCode;
  uint8_t surroundCode;
  uint8_t bsmod;
  uint8_t numChannels;
  bool fullSVC;
} Ac3Param;

typedef struct {
  uint32_t cpbSize;  /**< CpbSize[cpb_cnt_minus1] */
  uint32_t bitrate;  /**< BitRate[0] */
} H264Param;

typedef union {
  void * sharedPtr;
  Ac3Param * ac3Param;
  H264Param * h264Param;
} CodecSpecInfos;

typedef enum {
  FMT_SPEC_INFOS_NONE,
  FMT_SPEC_INFOS_AC3,
  FMT_SPEC_INFOS_H264
} CodecSpecInfosType;

#endif

#if 0
typedef struct {
  unsigned int rx; /* leaking Rate from transport buffer (TB -> B/MB) */

  /* if Video type: */
  unsigned int mbs; /* Multiplexing Buffer Size (in bytes) */
  unsigned int rbx; /* leaking Rate from multiplexing Buffer (MB -> EB) */
  unsigned int ebs; /* Elementary stream Buffer Size (in bytes) */

  /* other stream types: */
  unsigned int bs; /* decoder Buffer Size (in bytes) */

  void * tStdVerifier;
} BufferingParameters;
#endif

#if 0

typedef struct {
  LibbluStreamCodingType streamCodingType;  /**< Stream coding type (codec).       */
  bool secStream;                     /**< Stream is a secondary stream.     */

  union {
    struct {
      uint8_t videoFormat;            /**< Video stream format value.        */
      HdmvFrameRateCode frameRate;        /**< Video stream frame-rate value.    */
      bool stillPicture;              /**< Video stream is a still picture.  */
      bool hdrExtStream;              /**< Video stream is a dependant HDR
        extension stream of an independant video stream.                     */

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

  CodecSpecInfos codecSpecInfos;      /**< Codec specific parameters.        */
} StreamInfos;

#endif

#if 0
/** \~english
 * \brief Generated PES frame structure.
 *
 */
typedef struct {
  bool extensionFrame;     /**< Is an extension frame (used to set current
    PES frame as a nasted extension sub-stream and use correct timing
    parameters).                                                             */
  bool dtsPresent;         /**< DTS (Decoding TimeStamp) field presence
    triggering.                                                              */
  bool extensionDataPresent;

  uint64_t pts;            /**< PES packet PTS (Presentation Time Stamp) in
    27MHz clock.                                                             */
  uint64_t dts;            /**< PES packet DTS (Decoding Time Stamp) in
    27MHz clock (only use if dtsPresent == true).                            */

  EsmsPesPacketExtData extensionData;

  uint8_t * data;          /**< PES packet data bytes array.                 */
  size_t length;           /**< Length of PES packet in bytes.               */
  size_t dataOffset;       /**< PES packet current reading offset. This
    value is incremented each time PES frame is splitted in a TS packet.     */
  size_t allocatedLength;  /**< PES packet data field allocated length in
    bytes.                                                                   */
} PesPacket;

/** \~english
 * \brief Multiplexed ES stream structure.
 *
 * Elementary Stream multiplexing structure.
 * Also called PES stream in Rec. ITU-T H.222.
 */
typedef struct {
  StreamType type;    /**< Type of the stream.                               */
  uint16_t pid;       /**< Stream PID.                                       */

  uint32_t packetNb;  /**< Current number of supplied TS packets (used for
    continuity_counter).                                                     */

  uint64_t refPts;     /**< Zero timestamp reference of stream.               */
  uint64_t startPts;  /**< Stream starting timestamp (time value is obtain
    by doing startPts - refPts).                                              */
  uint64_t endPts;    /**< Stream final timestamp (only used to display
    progression).                                                            */

  StreamInfos streamParam;  /**< Stream content parameters.                  */

  /* Stream generation script related parameters (ESMS): */
  BitstreamReaderPtr streamScriptFile;  /**< Input stream generation
    script file.                                                             */
  EsmsDataBlocks dataSections;     /**< Script file sections.           */

  char * sourceFileNames[ESMS_MAX_ALLOWED_DIR];  /**< Stream generation input
    filenames.                                                               */
  BitstreamReaderPtr streamFile[ESMS_MAX_ALLOWED_DIR];  /**< Stream generation
    script attached input files handlers.                                    */
  unsigned nbStreamFiles;  /**< Stream generation script attached input
    files number. */

  bool endOfScript;                        /**< Set to true when end of
    script has been reached, meaning no more PES frames remain in input
    script.                                                                  */
  EsmsPesPacketNodePtr pesFrameScriptQueue;  /**< Script generated PES frames
    building commands FIFO.                                                  */
  EsmsPesPacketNodePtr lastAddedNode;        /**< pesFrameScriptQueue FIFO
    top node.                                                                */

  bool remainingData;          /**< Set to true when no more data to mux
    remain for this stream. This condition is true for script-generated
    stream when endOfScript is set to true and pesFrameScriptQueue
    FIFO is empty.                                                           */
  PesPacket currentPesPacket;  /**< Current generated PES frame.             */
} Stream, *StreamPtr;

/** \~english
 * \brief Returns stream current PES frame remaining bytes.
 *
 * \param StreamPtr StreamPtr Input stream.
 * \return size_t Remaining bytes.
 */
#define REMAINING_PES_DATA(StreamPtr)                                         \
  (                                                                           \
    (StreamPtr)->currentPesPacket.length -                                    \
    (StreamPtr)->currentPesPacket.dataOffset                                  \
  )

typedef struct {
  StreamType type;
  uint16_t pid;

  uint32_t packetNb;

#if 0
  uint64_t ptTs;
  uint64_t ptTsDelay;
  uint32_t tsDuration;
#endif
  bool firstFullTableSupplied;

  size_t length;
  size_t dataOffset;
  uint8_t * data;
} SystemStream, *SystemStreamPtr;

/** \~english
 * \brief Returns system stream current data remaining bytes.
 *
 * \param StreamPtr StreamPtr Input stream.
 * \return size_t Remaining bytes.
 */
#define REMAINING_SYS_DATA(SystemStreamPtr)                                   \
  (                                                                           \
    (SystemStreamPtr)->length -                                               \
    (SystemStreamPtr)->dataOffset                                             \
  )
#endif

/* int choosePID(StreamPtr stream); */

/* uint8_t getHdmvVideoFormat(
  int screenHorizontalDefinition,
  int screenVerticalDefinition,
  bool isInterlaced
); */

void printFileParsingProgressionBar(BitstreamReaderPtr bitStream);

/** \~english
 * \brief str_time() clock representation formatting mode.
 *
 * Given maximum sizes in char include terminating null-character.
 */
typedef enum {
  STRT_H_M_S_MS  /**< HH:MM:SS.mmm format, use at most 13 charcters.         */
} str_time_format;

#define STRT_H_M_S_MS_LEN  13

/** \~english
 * \brief Copies into buf a string representation of the time from given
 * clock timestamp in #MAIN_CLOCK_27MHZ ticks.
 *
 * \param buf Destination pointer to the destination array where the
 * resulting C string is copied.
 * \param maxsize Maximum number of characters to be copied to buf,
 * including the terminating null-character.
 * \param formatmode Result string time representation format mode.
 * \param clockvalue Clock value to be represented, exprimed in
 * #MAIN_CLOCK_27MHZ ticks.
 *
 * The output string format (and maximum size) is defined by given format
 * argument.
 *
 * \note If given maxsize value is shorter than the specified #str_time_format
 * one for the given mode, the output is truncated (and may not countain
 * terminating null-character)
 */
void str_time(
  lbc * buf,
  size_t maxsize,
  str_time_format formatmode,
  uint64_t clockvalue
);

#endif