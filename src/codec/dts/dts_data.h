/** \~english
 * \file dts_data.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief DTS audio bitstreams data structures module.
 */

#ifndef __LIBBLU_MUXER__CODECS__DTS__DATA_H__
#define __LIBBLU_MUXER__CODECS__DTS__DATA_H__

typedef struct {
  int64_t length;
  unsigned version;

  uint8_t refClockCode;

  unsigned timeStamp;

  uint8_t frameRateCode;
  bool dropFrame;

  bool extSubstreamPresent;
  bool coreSubstreamPresent;
  bool navigationTableIndicatorPresent;
  bool peakBitRateSmoothingPerformed;
  bool variableBitRate;

  unsigned nbAudioPresentations;
  unsigned nbExtensionSubstreams;

  /* Computed parameters */
  unsigned refClock;
  float frameRate;
} DtshdFileHeaderChunk;

typedef struct {
  int64_t length;
  char * string;
} DtshdFileInfo;

typedef struct {
  int64_t length;

  unsigned sampleRate;
  unsigned bitRateKbps;
  uint16_t channelMask;
  int64_t framePayloadLength;
} DtshdCoreSubStrmMeta;

typedef struct {
  int64_t length;

  unsigned avgBitRateKbps;
  unsigned peakBitRateKbps;
  unsigned peakBitRateSmoothingBufSizeKb;
  size_t framePayloadLength;
} DtshdExtSubStrmMeta;

typedef struct {
  int64_t length;

  unsigned presentationIndex;

  bool lbrComponentPresent;
  bool losslessComponentPresent;
  bool backwardCompatibleCoreLocation;
  bool backwardCompatibleCorePresent;

  unsigned maxSampleRate;
  unsigned nbFrames;
  unsigned maxNbSamplesPerFrame;
  unsigned maxNbSamplesOrigAudioPerFrame;

  uint16_t channelMask;

  unsigned codecDelay;
  unsigned nbSkippedFrames;

  struct {
    unsigned sampleRate;
    unsigned bitRateKbps;
    uint16_t channelMask;
  } extCore;
  bool extCorePresent;

  unsigned losslessLsbTrim;
} DtshdAudioPresPropHeaderMeta;

typedef struct {
  int64_t length;
  unsigned index;
  char * string;
} DtshdAudioPresText;

typedef struct {
  int64_t length;
} DtshdNavMeta;

typedef struct {
  int64_t endOffset;
} DtshdStreamData;

typedef struct {
  int64_t length;

  unsigned clockFreq;
  uint8_t frameRateCode;
  float frameRate;
  bool dropFrame;
} DtshdTimecode;

typedef struct {
  int64_t length;
  char * string;
} DtshdBuildVer;

typedef struct {
  int64_t length;
  uint8_t * frame;
} DtshdBlackout;

typedef enum {
  DTS_SYNCWORD_CORE       = 0x7FFE8001,
  DTS_SYNCWORD_SUBSTREAM  = 0x64582025,
  DTS_SYNCWORD_XLL        = 0x41A29547
} DcaSyncword;

/** \~english
 * \brief Maximal supported Encoder Software Revision code.
 */
#define DTS_MAX_SYNTAX_VERNUM 0x7

typedef struct {
  bool terminationFrame;           /* FTYPE  */

  uint8_t samplesPerBlock;         /* SHORT  */
  uint8_t blocksPerChannel;        /* NBLKS  */
  int64_t frameLength;             /* FSIZE  */

  bool crcPresent;                 /* CPF    */
  uint16_t crcValue;               /* HCRC   */

  uint8_t audioChannelArrangement; /* AMODE  */

  uint8_t audioFreqCode;           /* SFREQ  */

  uint8_t sourcePcmResCode;        /* PCMR   */

  uint8_t bitRateCode;             /* RATE   */

  bool embeddedDynRange;           /* DYNF   */
  bool embeddedTimestamp;          /* TIMEF  */
  bool auxData;                    /* AUXF   */
  bool hdcd;                       /* HDCD   */
  bool extAudio;                   /* EXT_AUDIO      */
  bool syncWordInsertionLevel;     /* ASPF   */
  uint8_t lfePresent;              /* LFF    */
  bool predHist;                   /* HFLAG  */
  bool multiRtInterpolatorSwtch;   /* FILTS  */
  bool frontSum;                   /* SUMF   */
  bool surroundSum;                /* SUMS   */

  uint8_t extAudioId;              /* EXT_AUDIO_ID   */

  uint8_t syntaxCode;              /* VERNUM */
  uint8_t copyHistory;             /* CHIST  */
  uint8_t dialNormCode;            /* DIALNORM / DNG */

  /* Computed parameters : */
  unsigned nbChannels;
  unsigned audioFreq;
  unsigned bitDepth;
  bool isEs;
  unsigned bitrate;
  int dialNorm;
  int64_t size; /**< Header size in bytes (sync word comprised). */
} DcaCoreSSFrameHeaderParameters;

typedef struct {
  DcaCoreSSFrameHeaderParameters header;
  int64_t payloadSize;
} DcaCoreSSFrameParameters;

typedef struct {
  int64_t size;

  bool syncWordPresent;
  unsigned peakBitRateSmoothingBufSizeCode;
  size_t peakBitRateSmoothingBufSize;
  unsigned initialXllDecodingDelayInFrames;
  size_t nbBytesOffXllSync;

  uint8_t steamId;
} DcaAudioAssetSSXllParameters;

#define DCA_EXT_SS_DISABLE_MIX_META_SUPPORT false
#define DCA_EXT_SS_ENABLE_DRC_2 true
#define DCA_EXT_SS_DISABLE_CRC false
#define DCA_EXT_SS_STRICT_NOT_DIRECT_SPEAKERS_FEED_REJECT false

#define DTS_EXT_SS_LANGUAGE_DESC_SIZE  3

#define DTS_EXT_SS_MAX_STRING_TEXT_MAX_LEN  1024

#define DTS_EXT_SS_MAX_CHANNELS_NB  25

#define DTS_EXT_SS_MAX_SPEAKERS_SETS_NB  16

#define DTS_EXT_SS_MAX_OUT_MIX_CONFIGS_NB  4
#define DTS_EXT_SS_MAX_NB_REMAP_SETS  7
#define DTS_EXT_SS_MAX_DOWNMIXES_NB  3

#define DTS_EXT_SS_MAX_AUDIO_PRES_NB  8
#define DTS_EXT_SS_MAX_EXT_SUBSTREAMS_NB  4

/** \~english
 * \brief Define the maximal supported Ext SS Reserved fields size in bytes.
 *
 * If field size exceed reservedFieldLength * 8 + paddingBits bits,
 * the reserved field data isn't saved.
 */
#define DCA_EXT_SS_MAX_SUPP_RES_FIELD_SIZE 16

#define DCA_EXT_SS_IS_SUPP_RES_FIELD_SIZES(ReservedSize, ByteAlignSize)       \
  (                                                                           \
    (8 * (ReservedSize) + (ByteAlignSize))                                    \
    <= 8 * DCA_EXT_SS_MAX_SUPP_RES_FIELD_SIZE                                 \
  )

typedef struct {
  bool assetTypeDescriptorPresent;
  uint8_t assetTypeDescriptor;

  bool languageDescriptorPresent;
  uint8_t languageDescriptor[
    DTS_EXT_SS_LANGUAGE_DESC_SIZE + 1
  ];

  bool infoTextPresent;
  uint8_t infoText[DTS_EXT_SS_MAX_STRING_TEXT_MAX_LEN + 1];
  int64_t infoTextLength;

  unsigned bitDepth;
  uint8_t maxSampleRateCode;
  unsigned nbChannels;

  bool directSpeakersFeed;
  bool embeddedStereoDownmix;
  bool embeddedSurround6chDownmix;
  uint8_t representationType; /* Only if directSpeakersFeed == 0b0 */

  bool speakersMaskEnabled;
  uint16_t speakersMask;

  unsigned nbOfSpeakersRemapSets;
  uint16_t stdSpeakersLayoutMask[DTS_EXT_SS_MAX_NB_REMAP_SETS];
  unsigned nbChsInRemapSet[DTS_EXT_SS_MAX_NB_REMAP_SETS];
  unsigned nbChRequiredByRemapSet[DTS_EXT_SS_MAX_NB_REMAP_SETS];
  /* decodedChannelsLinkedToSetSpeakerLayoutMask : */
  uint16_t decChLnkdToSetSpkrMask[DTS_EXT_SS_MAX_NB_REMAP_SETS][DTS_EXT_SS_MAX_CHANNELS_NB];
  uint8_t nbRemapCoeffCodes[DTS_EXT_SS_MAX_NB_REMAP_SETS][DTS_EXT_SS_MAX_CHANNELS_NB];
  uint8_t outputSpkrRemapCoeffCodes[DTS_EXT_SS_MAX_NB_REMAP_SETS][DTS_EXT_SS_MAX_CHANNELS_NB][DTS_EXT_SS_MAX_SPEAKERS_SETS_NB];

  /* Computed parameters */
  unsigned maxSampleRate;
} DcaAudioAssetDescriptorStaticFieldsParameters;

/* Value used if 'bDRCMetadatRev2Present' is false: */
#define DEFAULT_DRC_VERSION_VALUE -1

typedef struct {
  bool drcEnabled;       /**< bDRCCoefPresent                                */
  struct {
    uint8_t drcCode;     /**< nuDRCCode                                      */
    uint8_t drc2ChCode;  /**< nuDRC2ChDmixCode                               */
  } drcParameters;       /**< DRC parameters present if (bDRCCoefPresent)    */

  bool dialNormEnabled;  /**< bDialNormPresent                               */
  uint8_t dialNormCode;  /**< nuDialNormCode                                 */

  bool mixMetadataPresent;  /**< bMixMetadataPresent                         */
  struct {
#if !DCA_EXT_SS_DISABLE_MIX_META_SUPPORT
    bool useExternalMix;            /**< bExternalMixFlag                    */
    uint8_t postMixGainCode;        /**< nuPostMixGainAdjCode                */
    uint8_t drcMixerControlCode;    /**< nuControlMixerDRC                   */
    union {
      uint8_t limitDRCPriorMix;       /**< nuLimit4EmbeddedDRC               */
      uint8_t customMixDRCCoeffCode;  /**< nuCustomDRCCode                   */
    };

    bool perMainAudioChSepScal;     /**< bEnblPerChMainAudioScale            */
    uint8_t scalingAudioParam[
      DTS_EXT_SS_MAX_OUT_MIX_CONFIGS_NB
    ][DTS_EXT_SS_MAX_CHANNELS_NB];             /**< nuMainAudioScaleCode, if
      'perMainAudioChSepScal == 0b0', the scaling code shared between all
      channels of one mix config is stored as for the channel of index 0,
      'scalingAudioParam[configId][0]'.                                      */

    unsigned nbDownMixes;
    unsigned nbChPerDownMix[DTS_EXT_SS_MAX_DOWNMIXES_NB];
    uint16_t mixOutputMappingMask[DTS_EXT_SS_MAX_OUT_MIX_CONFIGS_NB][DTS_EXT_SS_MAX_DOWNMIXES_NB][DTS_EXT_SS_MAX_CHANNELS_NB];
    unsigned mixOutputMappingNbCoeff[DTS_EXT_SS_MAX_OUT_MIX_CONFIGS_NB][DTS_EXT_SS_MAX_DOWNMIXES_NB][DTS_EXT_SS_MAX_CHANNELS_NB];
    uint8_t mixOutputCoefficients[DTS_EXT_SS_MAX_OUT_MIX_CONFIGS_NB][DTS_EXT_SS_MAX_DOWNMIXES_NB][DTS_EXT_SS_MAX_DOWNMIXES_NB][DTS_EXT_SS_MAX_SPEAKERS_SETS_NB];
#endif
  } mixMetadata;

} DcaAudioAssetDescriptorDynamicMetadataParameters;

typedef enum {
  DCA_EXT_SS_CODING_MODE_DTS_HD_COMPONENTS             = 0x0,
  DCA_EXT_SS_CODING_MODE_DTS_HD_LOSSLESS_WITHOUT_CORE  = 0x1,
  DCA_EXT_SS_CODING_MODE_DTS_HD_LOW_BITRATE            = 0x2,
  DCA_EXT_SS_CODING_MODE_AUXILIARY_CODING              = 0x3
} DcaAudioAssetCodingMode;

typedef enum {
  DCA_EXT_SS_COD_COMP_CORE_DCA         = (1 <<  0),
  DCA_EXT_SS_COD_COMP_CORE_EXT_XXCH    = (1 <<  1),
  DCA_EXT_SS_COD_COMP_CORE_EXT_X96     = (1 <<  2),
  DCA_EXT_SS_COD_COMP_CORE_EXT_XCH     = (1 <<  3),
  DCA_EXT_SS_COD_COMP_EXTSUB_CORE_DCA  = (1 <<  4),
  DCA_EXT_SS_COD_COMP_EXTSUB_XBR       = (1 <<  5),
  DCA_EXT_SS_COD_COMP_EXTSUB_XXCH      = (1 <<  6),
  DCA_EXT_SS_COD_COMP_EXTSUB_X96       = (1 <<  7),
  DCA_EXT_SS_COD_COMP_EXTSUB_LBR       = (1 <<  8),
  DCA_EXT_SS_COD_COMP_EXTSUB_XLL       = (1 <<  9),
  DCA_EXT_SS_COD_COMP_RESERVED_1       = (1 << 10),
  DCA_EXT_SS_COD_COMP_RESERVED_2       = (1 << 11)
} DcaAudioAssetCodingComponentMask;

typedef enum {
  DCA_EXT_SS_DRC_REV_2_VERSION_1 = 1
} DcaAudioDrcMetadataRev2Version;

typedef struct {
  DcaAudioAssetCodingMode codingMode;
  uint16_t codingComponentsUsedMask;

  union {
    struct {
      struct {
        int64_t size;
        bool syncWordPresent;
        unsigned syncDistanceInFramesCode;
        unsigned syncDistanceInFrames;
      } extSSCore;

      struct {
        int64_t size;
      } extSSXbr;

      struct {
        int64_t size;
      } extSSXxch;

      struct {
        int64_t size;
      } extSSX96;

      struct {
        int64_t size;
        bool syncWordPresent;
        unsigned syncDistanceInFramesCode;
        unsigned syncDistanceInFrames;
      } extSSLbr;

      DcaAudioAssetSSXllParameters extSSXll;

      uint16_t reservedExtension1_data;
      uint16_t reservedExtension2_data;
    } codingComponents;

    struct {
      int64_t size;
      uint8_t auxCodecId;
      bool syncWordPresent;
      unsigned syncDistanceInFramesCode;
      unsigned syncDistanceInFrames;
    } auxilaryCoding;
  };

  struct {
    bool oneTrackToOneChannelMix;
    bool perChannelMainAudioScaleCode;
    uint8_t mainAudioScaleCodes[DTS_EXT_SS_MAX_OUT_MIX_CONFIGS_NB][DTS_EXT_SS_MAX_CHANNELS_NB];
  } mixMetadata;
  bool mixMetadataFieldsPresent;

  bool decodeInSecondaryDecoder;

  bool extraDataPresent;
  bool drcRev2Present;
  struct {
    unsigned version;
  } drcRev2;
} DcaAudioAssetDescriptorDecoderNavDataParameters;

typedef struct {
  int64_t descriptorLength;

  unsigned assetIndex;

  DcaAudioAssetDescriptorStaticFieldsParameters staticFields;
  bool staticFieldsPresent; /* Copy from ExtSS Header */
  DcaAudioAssetDescriptorDynamicMetadataParameters dynMetadata;
  DcaAudioAssetDescriptorDecoderNavDataParameters decNavData;

  int64_t reservedFieldLength;
  int64_t paddingBits;
  uint8_t reservedFieldData[
    DCA_EXT_SS_MAX_SUPP_RES_FIELD_SIZE
  ];
} DcaAudioAssetDescriptorParameters;

typedef struct {
  uint8_t adjustmentLevel;
  unsigned nbMixOutputConfigs;

  uint16_t mixOutputChannelsMask[DTS_EXT_SS_MAX_OUT_MIX_CONFIGS_NB];
  unsigned nbMixOutputChannels[DTS_EXT_SS_MAX_OUT_MIX_CONFIGS_NB];
} DcaExtSSHeaderMixMetadataParameters;

typedef struct {
  uint8_t referenceClockCode;

  uint8_t frameDurationCode;
  unsigned frameDurationCodeValue;

  bool timeStampPresent;
  uint32_t timeStampValue;
  uint8_t timeStampLsb;

  unsigned nbAudioPresentations;
  unsigned nbAudioAssets;

  uint8_t activeExtSSMask[DTS_EXT_SS_MAX_AUDIO_PRES_NB];
  uint8_t activeAssetMask[DTS_EXT_SS_MAX_AUDIO_PRES_NB][DTS_EXT_SS_MAX_EXT_SUBSTREAMS_NB];

  bool mixMetadataEnabled;
  DcaExtSSHeaderMixMetadataParameters mixMetadata;

  /* Computed parameters */
  unsigned referenceClockFreq;
  float frameDuration;
  uint64_t timeStamp;
} DcaExtSSHeaderStaticFieldsParameters;

#define DCA_EXT_SS_CRC_LENGTH 16
#define DCA_EXT_SS_CRC_POLY 0x11021
#define DCA_EXT_SS_CRC_MODULO 0x10000
#define DCA_EXT_SS_CRC_INITIAL_V 0xFFFF

#define DCA_EXT_SS_CRC_PARAM()                                                \
  (CrcParam) {.table = dtsExtSSCrcTable, .mask = 0xFFFF, .length = 16}

#define DCA_EXT_SS_MAX_NB_INDEXES 4
#define DCA_EXT_SS_MAX_NB_AUDIO_ASSETS 8

typedef struct {
  uint8_t userDefinedBits;
  uint8_t extSSIdx;

  bool longHeaderSizeFlag;

  int64_t extensionSubstreamHeaderLength;
  int64_t extensionSubstreamFrameLength;

  bool staticFieldsPresent; /* Mandatory in most cases */
  DcaExtSSHeaderStaticFieldsParameters staticFields;

  int64_t audioAssetsLengths[DCA_EXT_SS_MAX_NB_AUDIO_ASSETS];
  DcaAudioAssetDescriptorParameters audioAssets[DCA_EXT_SS_MAX_NB_AUDIO_ASSETS];

  bool audioAssetBckwdCompCorePresent[DCA_EXT_SS_MAX_NB_AUDIO_ASSETS];
  struct {
    uint8_t extSSIndex;
    uint8_t assetIndex;
  } audioAssetBckwdCompCore[DCA_EXT_SS_MAX_NB_AUDIO_ASSETS];

  int64_t reservedFieldLength;  /**< Reserved */
  int64_t paddingBits;
  uint8_t reservedFieldData[
    DCA_EXT_SS_MAX_SUPP_RES_FIELD_SIZE
  ];  /**< Reserved field data content array, only in use if field size does
    not exceed its size. See #DCA_EXT_SS_MAX_SUPP_RES_FIELD_SIZE.            */

  uint16_t crcValue;
} DcaExtSSHeaderParameters;

typedef struct {
  DcaExtSSHeaderParameters header;
} DcaExtSSFrameParameters;

#define DTS_XLL_MAX_SUPPORTED_OFILE_POS 8

typedef struct {
  int64_t off;
  size_t len;
} DcaXllFrameSFPositionIndex;

/** \~english
 * \brief DTS XLL frame Source File position.
 *
 */
typedef struct {
  DcaXllFrameSFPositionIndex sourceOffsets[
    DTS_XLL_MAX_SUPPORTED_OFILE_POS
  ];
  int nbSourceOffsets;
} DcaXllFrameSFPosition;

#define INIT_DTS_XLL_FRAME_SF_POS()                                           \
  (                                                                           \
    (DcaXllFrameSFPosition) {                                                 \
      .nbSourceOffsets = 0                                                    \
    }                                                                         \
  )

#define IS_EMPTY_XLL_FRAME_SF_POS(frmPos)                                     \
  ((frmPos).nbSourceOffsets == 0)

/** \~english
 * \brief Register given source file offset and length to given
 * #DcaXllFrameSFPosition structure.
 *
 * If too many source offsets are already been defined, the given error
 * instruction is executed.
 *
 * \param #DcaXllFrameSFPosition Destination frame source file position
 * recorder.
 * \param offset off_t Original file XLL frame piece start offset.
 * \param length size_t XLL frame piece length.
 * \param errinstr Error instruction executed if too many indexes have been
 * used, E.g. 'LIBBLU_ERROR_RETURN("Error")'.
 */
#define ADD_DTS_XLL_FRAME_SF_POS(dst, offset, length, errinstr)               \
  {                                                                           \
    if (DTS_XLL_MAX_SUPPORTED_OFILE_POS <= (dst).nbSourceOffsets)             \
      errinstr;                                                               \
    (dst).sourceOffsets[(dst).nbSourceOffsets].off = (offset);                \
    (dst).sourceOffsets[(dst).nbSourceOffsets].len = (length);                \
    (dst).nbSourceOffsets++;                                                  \
  }

typedef struct {
  unsigned number;     /**< Used for stats, number of the PBR frame in
    bitstream.                                                               */
  size_t size;         /**< Frame size in bytes.                             */

  DcaXllFrameSFPosition pos;

  unsigned pbrDelay;   /**< Frame decoding delay according to audio asset
    parameters. If this value is greater than zero, data is accumulated in
    PBR buffer prior to decoding.                                            */
} DcaXllPbrFrame, *DcaXllPbrFramePtr;

/** \~english
 * \brief DTS XLL PBR smoothing buffer maximum size in bytes.\n
 *
 * Value is equal to 240 kBytes in binary representation (245 760 bytes).
 */
#define DTS_XLL_MAX_PBR_BUFFER_SIZE (240 << 10)

/** \~english
 * \brief Max supported DTS-HDMA nVersion number.
 */
#define DTS_XLL_MAX_VERSION 1

/** \~english
 * \brief
 *
 */
#define DTS_XLL_HEADER_MIN_SIZE 12

#define DTS_XLL_MAX_CHSETS_NB 3

#define DTS_XLL_MAX_SEGMENTS_NB 1024

/** \~english
 * \brief Up to 48 kHz.
 *
 */
#define DTS_XLL_MAX_SAMPLES_PER_SEGMENT_UT_48KHZ 256

/** \~english
 * \brief More than 48 kHz.
 *
 */
#define DTS_XLL_MAX_SAMPLES_PER_SEGMENT_MT_48KHZ 512

/** \~english
 * \brief
 *
 * At 512 samples/seg, up to 128 segments.
 * At 256 samples/seg, up to 256 segments.
 */
#define DTS_XLL_MAX_SAMPLES_NB 65536

typedef struct {
  unsigned version;

  size_t headerSize;
  size_t frameSize;
  unsigned frameSizeFieldLength;
  unsigned nbChSetsPerFrame;
  unsigned nbSegmentsInFrameCode;
  unsigned nbSamplesPerSegmentCode;
  unsigned nbBitsSegmentSizeField;
  uint8_t crc16Pres;
  bool scalableLSBs;
  unsigned nbBitsChMaskField;
  unsigned fixedLsbWidth;

  size_t reservedFieldSize;
  unsigned paddingBits;

  uint16_t headerCrc;

  /* Computed parameters : */
  unsigned nbSegmentsInFrame;
  unsigned nbSamplesPerSegment;
} DtsXllCommonHeader;

#define DTS_XLL_MAX_CH_NB DTS_EXT_SS_MAX_CHANNELS_NB

#define DTS_XLL_MAX_DOWMIX_COEFF_NB                                           \
  (                                                                           \
    (DTS_XLL_MAX_CH_NB + 1)                                                   \
    * (DTS_XLL_MAX_CH_NB * (DTS_XLL_MAX_CHSETS_NB - 1))                       \
  )

typedef struct {
  size_t chSetHeaderSize;
  unsigned nbChannels;
  uint32_t residualChType;
  unsigned bitRes;
  unsigned bitWidth;
  uint8_t origSamplFreqIdx;
  uint8_t samplFreqInterplFactrMod;

  uint8_t replSetMemberId;
  struct {
    bool activeReplSet;
  };  /**< Only used if replSetMemberId == true.                             */

  /* Common fields: */
  bool isPrimaryChSet;
  bool downMixCoeffEmbedded;
  bool partOfHierarchy;

  union {
    struct {
      bool downMixPerformed;
      uint8_t downMixType;
      uint16_t downMixCoeff[DTS_XLL_MAX_DOWMIX_COEFF_NB];

      bool chMaskEnabled;
      union {
        uint64_t chMask;
        struct {
          uint16_t radius;
          uint16_t theta;
          uint16_t phi;
        } sphericalChCoords[DTS_XLL_MAX_CH_NB];
      };
    };  /**< Only if bOne2OneMapChannels2Speakers == true.                   */

    struct {
      bool mapCoeffPresent;  /**< Not supported (TODO).                      */
    };  /**< Otherwise (bOne2OneMapChannels2Speakers == false).              */
  };

  unsigned nbFreqBands;

  /* Computed parameters : */
  unsigned samplingFreq;
} DtsXllChannelSetSubHeader;

typedef struct {
  DtsXllCommonHeader comHdr;
  DtsXllChannelSetSubHeader subHdr[DTS_XLL_MAX_CHSETS_NB];

  DcaXllFrameSFPosition originalPosition;
} DtsXllFrameParameters;

#endif