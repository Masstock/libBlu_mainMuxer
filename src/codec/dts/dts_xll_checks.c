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
    param.nVersion, param.nVersion - 1
  );

  LIBBLU_DTS_DEBUG_XLL(
    "    XLL frame Common Header length (nHeaderSize): "
    "% byte(s) (0x%02" PRIX8 ").\n",
    param.nHeaderSize,
    param.nHeaderSize - 1u
  );

  LIBBLU_DTS_DEBUG_XLL(
    "    Size of field nBytesFrameLL (nBits4FrameFsize): "
    "%" PRIu8 " bits (0x%02" PRIX8 ").\n",
    param.nBits4FrameFsize,
    param.nBits4FrameFsize - 1
  );

  LIBBLU_DTS_DEBUG_XLL(
    "    Number of bytes in LossLess Frame (nLLFrameSize): "
    "%" PRIu32 " bytes (0x%" PRIX32 ").\n",
    param.nLLFrameSize,
    param.nLLFrameSize - 1u
  );

  if (DTS_XLL_MAX_PBR_BUFFER_SIZE < param.nLLFrameSize)
    LIBBLU_DTS_ERROR_RETURN(
      "Invalid DTS XLL frame size, "
      "exceed maximum possible size: %" PRIu32 " < %" PRIu32 " bytes.\n",
      DTS_XLL_MAX_PBR_BUFFER_SIZE,
      param.nLLFrameSize
    );
  if (0 == param.nLLFrameSize)
    LIBBLU_DTS_ERROR_RETURN(
      "Invalid DTS XLL frame size, "
      "overflow.\n"
    );

  LIBBLU_DTS_DEBUG_XLL(
    "    Number of Channels Sets per Frame (nNumChSetsInFrame): "
    "%u set(s) (0x%X).\n",
    param.nNumChSetsInFrame,
    param.nNumChSetsInFrame - 1
  );

  if (DTS_XLL_MAX_CHSETS_NB < param.nNumChSetsInFrame)
    LIBBLU_DTS_ERROR_RETURN(
      "Too many Channel Sets in DTS XLL frame (%u < %u).\n",
      DTS_XLL_MAX_CHSETS_NB,
      param.nNumChSetsInFrame
    );

  LIBBLU_DTS_DEBUG_XLL(
    "    Number of Segments per Frame (nSegmentsInFrame): "
    "%u segment(s) (0x%X).\n",
    param.nSegmentsInFrame,
    param.nSegmentsInFrame_code
  );

  if (DTS_XLL_MAX_SEGMENTS_NB < param.nSegmentsInFrame)
    LIBBLU_DTS_ERROR_RETURN(
      "Too many Segments in DTS XLL frame (%u < %u).\n",
      DTS_XLL_MAX_SEGMENTS_NB,
      param.nSegmentsInFrame
    );

  LIBBLU_DTS_DEBUG_XLL(
    "    Number of Samples in a Segment (nSmplInSeg): %u sample(s) (0x%X).\n",
    param.nSmplInSeg,
    param.nSmplInSeg_code
  );
  LIBBLU_DTS_DEBUG_XLL(
    "     -> NOTE: This value is applied for each frequency band "
    "of the first channel set.\n"
  );

  if (DTS_XLL_MAX_SAMPLES_PER_SEGMENT_MT_48KHZ < param.nSmplInSeg)
    LIBBLU_DTS_ERROR_RETURN(
      "Too high number of Samples in DTS XLL frame (%u < %u).\n",
      DTS_XLL_MAX_SAMPLES_PER_SEGMENT_MT_48KHZ,
      param.nSmplInSeg
    );

  /* TODO: nSmplInSeg shall be checked for <= 48KHz. */

  if (DTS_XLL_MAX_SAMPLES_NB < param.nSegmentsInFrame * param.nSmplInSeg)
    LIBBLU_DTS_ERROR_RETURN(
      "Too many Samples in DTS XLL frame (product of number of samples per "
      "segment and number of segments %u exceed %u samples).\n",
      param.nSegmentsInFrame * param.nSmplInSeg,
      DTS_XLL_MAX_SAMPLES_NB
    );

  LIBBLU_DTS_DEBUG_XLL(
    "    Length of the size field in NAVI table (nBits4SSize): "
    "%" PRIu8 " bit(s) (0x%X).\n",
    param.nBits4SSize,
    param.nBits4SSize - 1
  );

  LIBBLU_DTS_DEBUG_XLL(
    "    Frequency bands CRC16 checksum presence code "
    "(nBandDataCRCEn): %s (0x%" PRIX8 ").\n",
    dtsXllCommonHeaderCrc16PresenceCodeStr(param.nBandDataCRCEn),
    param.nBandDataCRCEn
  );

  LIBBLU_DTS_DEBUG_XLL(
    "    Frequency band 0 MSB/LSB blocks spliting (bScalableLSBs): "
    "%s (0b%x).\n",
    BOOL_INFO(param.bScalableLSBs),
    param.bScalableLSBs
  );

  LIBBLU_DTS_DEBUG_XLL(
    "    Length of the Channels position Mask field (nBits4ChMask): "
    "%" PRIu8 " bit(s) (0x%" PRIX8 ").\n",
    param.nBits4ChMask,
    param.nBits4ChMask - 1
  );

  if (param.bScalableLSBs) {
    LIBBLU_DTS_DEBUG_XLL(
      "    Size of the frequency bands' LSB part code "
      "(nuFixedLSBWidth): 0x%" PRIu8 ".\n",
      param.nuFixedLSBWidth
    );

    if (0 < param.nuFixedLSBWidth)
      LIBBLU_DTS_DEBUG_XLL(
        "     => nuFixedLSBWidth == 0, size of LSB part is variable.\n"
      );
    else
      LIBBLU_DTS_DEBUG_XLL(
        "     => nuFixedLSBWidth > 0, size of LSB part is fixed, "
        "equal to %u bit(s).\n",
        param.nuFixedLSBWidth
      );
  }

  LIBBLU_DTS_DEBUG_XLL(
    "    Reserved/Byte alignment field (Reserved): %" PRIu32 " byte(s).\n",
    param.Reserved_size
  );

  LIBBLU_DTS_DEBUG_XLL(
    "    Common Header CRC16 checksum protection (nCRC16Header): "
    "0x%04" PRIX16 ".\n",
    param.nCRC16Header
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
    "%" PRIu16 " byte(s) (0x%06" PRIX16 ").\n",
    param.nChSetHeaderSize,
    param.nChSetHeaderSize - 1
  );

  LIBBLU_DTS_DEBUG_XLL(
    "    Number of Channels in Set (nChSetLLChannel): "
    "%" PRIu8 " (0x%" PRIX8 ").\n",
    param.nChSetLLChannel,
    param.nChSetLLChannel - 1
  );

  LIBBLU_DTS_DEBUG_XLL(
    "    Channels Residual coding Type (nResidualChEncode): "
    "0x%" PRIu16 ".\n",
    param.nResidualChEncode
  );

  for (i = 0; i < param.nChSetLLChannel; i++) {
    bool standalone_residual = (param.nResidualChEncode >> i) & 0x1;

    LIBBLU_DTS_DEBUG_XLL(
      "     => Channel %u: %s (0b%x).\n",
      i,
      standalone_residual ? "Original" : "Residual",
      standalone_residual
    );
  }

  return 0;
}