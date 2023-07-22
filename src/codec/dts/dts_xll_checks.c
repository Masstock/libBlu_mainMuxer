#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include "dts_xll_checks.h"

int checkDtsXllCommonHeader(
  const DtsXllCommonHeader param
)
{

  LIBBLU_DTS_DEBUG_XLL(
    "    SYNCXLL: 0x%08" PRIX32 ".\n",
    DCA_SYNCXLL
  );

  LIBBLU_DTS_DEBUG_XLL(
    "    Version number (nVersion): %u (0x%x).\n",
    param.version, param.version - 1
  );

  LIBBLU_DTS_DEBUG_XLL(
    "    XLL frame Common Header length (nHeaderSize): "
    "%zu byte(s) (0x%zx).\n",
    param.headerSize, param.headerSize - 1
  );
  if (param.headerSize < DTS_XLL_HEADER_MIN_SIZE)
    LIBBLU_DTS_ERROR_RETURN(
      "Too short DTS XLL Common Header size, "
      "expect at least %zu bytes, got %zu.\n",
      DTS_XLL_HEADER_MIN_SIZE,
      param.headerSize
    );

  LIBBLU_DTS_DEBUG_XLL(
    "    Size of field nBytesFrameLL (nBits4FrameFsize): "
    "%zu bits (0x%zx).\n",
    param.frameSizeFieldLength, param.frameSizeFieldLength - 1
  );

  LIBBLU_DTS_DEBUG_XLL(
    "    Number of bytes in LossLess Frame (nLLFrameSize): "
    "%zu bytes (0x%zx).\n",
    param.frameSize, param.frameSize - 1
  );

  if (DTS_XLL_MAX_PBR_BUFFER_SIZE < param.frameSize)
    LIBBLU_DTS_ERROR_RETURN(
      "Unexpected DTS XLL frame size "
      "(exceed maximum possible size: %zu < %zu bytes).\n",
      DTS_XLL_MAX_PBR_BUFFER_SIZE,
      param.frameSize
    );

  LIBBLU_DTS_DEBUG_XLL(
    "    Number of Channels Sets per Frame (nNumChSetsInFrame): "
    "%u set(s) (0x%X).\n",
    param.nbChSetsPerFrame,
    param.nbChSetsPerFrame - 1
  );

  if (DTS_XLL_MAX_CHSETS_NB < param.nbChSetsPerFrame)
    LIBBLU_DTS_ERROR_RETURN(
      "Too many Channel Sets in DTS XLL frame (%u < %u).\n",
      DTS_XLL_MAX_CHSETS_NB,
      param.nbChSetsPerFrame
    );

  LIBBLU_DTS_DEBUG_XLL(
    "    Number of Segments per Frame (nSegmentsInFrame): "
    "%u segment(s) (0x%X).\n",
    param.nbSegmentsInFrame,
    param.nbSegmentsInFrameCode
  );

  if (DTS_XLL_MAX_SEGMENTS_NB < param.nbSegmentsInFrame)
    LIBBLU_DTS_ERROR_RETURN(
      "Too many Segments in DTS XLL frame (%u < %u).\n",
      DTS_XLL_MAX_SEGMENTS_NB,
      param.nbSegmentsInFrame
    );

  LIBBLU_DTS_DEBUG_XLL(
    "    Number of Samples in a Segment (nSmplInSeg): %u sample(s) (0x%X).\n",
    param.nbSamplesPerSegment,
    param.nbSamplesPerSegmentCode
  );
  LIBBLU_DTS_DEBUG_XLL(
    "     -> NOTE: This value is applied for each frequency band "
    "of the first channel set.\n"
  );

  if (!param.nbSamplesPerSegment)
    LIBBLU_DTS_ERROR_RETURN(
      "Unexpected zero number of Samples in DTS XLL frame.\n"
    );
  if (DTS_XLL_MAX_SAMPLES_PER_SEGMENT_MT_48KHZ < param.nbSamplesPerSegment)
    LIBBLU_DTS_ERROR_RETURN(
      "Too high number of Samples in DTS XLL frame (%u < %u).\n",
      DTS_XLL_MAX_SAMPLES_PER_SEGMENT_MT_48KHZ,
      param.nbSamplesPerSegment
    );

  /* TODO: nSmplInSeg shall be checked for <= 48KHz. */

  if (
    DTS_XLL_MAX_SAMPLES_NB
    < param.nbSegmentsInFrame * param.nbSamplesPerSegment
  )
    LIBBLU_DTS_ERROR_RETURN(
      "Too many Samples in DTS XLL frame (product of number of samples per "
      "segment and number of segments %u exceed %u samples).\n",
      param.nbSegmentsInFrame * param.nbSamplesPerSegment,
      DTS_XLL_MAX_SAMPLES_NB
    );

  LIBBLU_DTS_DEBUG_XLL(
    "    Length of the size field in NAVI table (nBits4SSize): "
    "%u bit(s) (0x%X).\n",
    param.nbBitsSegmentSizeField,
    param.nbBitsSegmentSizeField - 1
  );

  LIBBLU_DTS_DEBUG_XLL(
    "    Frequency bands CRC16 checksum presence code (nBandDataCRCEn): "
    "0x%X.\n",
    param.crc16Pres
  );
  LIBBLU_DTS_DEBUG_XLL(
    "     => %s.\n",
    dtsXllCommonHeaderCrc16PresenceCodeStr(param.crc16Pres)
  );

  LIBBLU_DTS_DEBUG_XLL(
    "    Frequency band 0 MSB/LSB blocks spliting (bScalableLSBs): "
    "%s (0b%x).\n",
    BOOL_INFO(param.scalableLSBs),
    param.scalableLSBs
  );

  LIBBLU_DTS_DEBUG_XLL(
    "    Length of the Channels position Mask field (nBits4ChMask): "
    "%u bit(s) (0x%X).\n",
    param.nbBitsChMaskField,
    param.nbBitsChMaskField - 1
  );

  if (param.scalableLSBs) {
    LIBBLU_DTS_DEBUG_XLL(
      "    Size of the frequency bands' LSB part code (nuFixedLSBWidth): "
      "0x%X.\n",
      param.fixedLsbWidth
    );
    if (0 < param.fixedLsbWidth)
      LIBBLU_DTS_DEBUG_XLL(
        "     => nuFixedLSBWidth == 0, size of LSB part is variable.\n"
      );
    else
      LIBBLU_DTS_DEBUG_XLL(
        "     => nuFixedLSBWidth > 0, size of LSB part is fixed, "
        "equal to %u bit(s).\n",
        param.fixedLsbWidth
      );
  }

  LIBBLU_DTS_DEBUG_XLL(
    "    Reserved data field length (Reserved): %zu byte(s).\n",
    param.reservedFieldSize
  );

  LIBBLU_DTS_DEBUG_XLL(
    "    Byte boundary padding field (ByteAlign): %u bit(s).\n",
    param.ZeroPadForFsize_size
  );

  LIBBLU_DTS_DEBUG_XLL(
    "    Common Header CRC16 checksum protection (nCRC16Header): "
    "0x%04" PRIX16 ".\n",
    param.headerCrc
  );
#if !DCA_EXT_SS_DISABLE_CRC
  LIBBLU_DTS_DEBUG_XLL(
    "     -> Verified.\n"
  );
#else
  LIBBLU_DTS_DEBUG_XLL(
    "     -> Unckecked.\n"
  );
#endif

  return 0;
}

int checkDtsXllChannelSetSubHeader(
  const DtsXllChannelSetSubHeader param
)
{
  unsigned i;

  LIBBLU_DTS_DEBUG_XLL(
    "    Channel Set Sub-Header length (nChSetHeaderSize): "
    "%zu byte(s) (0x%zx).\n",
    param.chSetHeaderSize,
    param.chSetHeaderSize - 1
  );

  LIBBLU_DTS_DEBUG_XLL(
    "    Number of Channels in Set (nChSetLLChannel): %u (0x%x).\n",
    param.nbChannels,
    param.nbChannels - 1
  );

  LIBBLU_DTS_DEBUG_XLL(
    "    Channels Residual coding Type (nResidualChEncode): 0x%" PRIu32 ".\n",
    param.residualChType
  );

  for (i = 0; i < param.nbChannels; i++)
    LIBBLU_DTS_DEBUG_XLL(
      "     => Channel %u: %s (0b%x).\n",
      i,
      (param.residualChType & (1 << i)) ?
        "Coded residual"
      :
        "Substractive residual",
      (param.residualChType & (1 << i)) ?
        1
      :
        0
    );

  return 0;
}