#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "tsPackets.h"

int writeTpExtraHeader(
  BitstreamWriterPtr output,
  uint64_t pcr
)
{
  uint8_t extraHeader[TP_EXTRA_HEADER_SIZE];
  size_t offset = 0;

  uint32_t ats;

  static const uint8_t copyPermInd = 0x0;

  ats = pcr & 0x3FFFFFFF;

  /* [u2 copyPermissionIndicator] [u30 arrivalTimeStamp] */
  WB_ARRAY(extraHeader, offset, (copyPermInd << 6) | (ats >> 24));
  WB_ARRAY(extraHeader, offset, ats >> 16);
  WB_ARRAY(extraHeader, offset, ats >>  8);
  WB_ARRAY(extraHeader, offset, ats);

  return writeBytes(
    output,
    extraHeader,
    TP_EXTRA_HEADER_SIZE
  );
}

static void prepareESTransportPacketMainHeader(
  TPHeaderParameters * dst,
  LibbluStreamPtr stream,
  bool pcrInjectionRequirement
)
{
  size_t pesPacketRemainingData;
  bool payloadPresence, payloadStart, adaptationFieldPresence;

  pesPacketRemainingData = remainingPesDataLibbluES(stream->es);
  assert(0 < pesPacketRemainingData);

  payloadPresence = (0 < pesPacketRemainingData);
  payloadStart = (payloadPresence && isPayloadUnitStartLibbluES(stream->es));

  adaptationFieldPresence =
    pesPacketRemainingData < (TP_SIZE - TP_HEADER_SIZE)
    || pcrInjectionRequirement
  ;

  dst->transportErrorIndicator = false;
  dst->payloadUnitStartIndicator = payloadStart;
  dst->transportPriority = false;

  dst->pid = stream->pid;
  dst->transportScramblingControl = 0x00;
  dst->adaptationFieldControl =
    (adaptationFieldPresence << 1)
    | payloadPresence
  ;
  dst->continuityCounter = stream->packetNb & 0xF;
}

static void prepareSysTransportPacketMainHeader(
  TPHeaderParameters * dst,
  LibbluStreamPtr stream,
  bool pcrInjectionRequirement
)
{
  size_t tableRemainingData;
  bool payloadPresence, payloadStart, adaptationFieldPresence;

  tableRemainingData = remainingTableDataLibbluSystemStream(stream->sys);

  payloadPresence = (0 < tableRemainingData);
  payloadStart = (
    payloadPresence
    && isPayloadUnitStartLibbluSystemStream(stream->sys)
  );

  adaptationFieldPresence =
    tableRemainingData < (TP_SIZE - TP_HEADER_SIZE)
    || pcrInjectionRequirement
  ;

  dst->transportErrorIndicator = false;
  dst->payloadUnitStartIndicator = payloadStart;
  dst->transportPriority = false;

  dst->pid = stream->pid;
  dst->transportScramblingControl = 0x00;
  dst->adaptationFieldControl =
    (adaptationFieldPresence << 1)
    | payloadPresence
  ;
  dst->continuityCounter =
    (stream->sys.useContinuityCounter) ?
      stream->packetNb & 0xF
    :
      0
  ;
}

static void prepareAdaptationField(
  AdaptationFieldParameters * dst,
  LibbluStreamPtr stream,
  bool pcrInjectionRequirement,
  uint64_t pcrValue
)
{
  bool pcrPresence;
  size_t remainingPayload, adaptationFieldSize;

  pcrPresence = ((stream->type == TYPE_PCR) || pcrInjectionRequirement);

  dst->writeOnlyLength = false;

  dst->discontinuityIndicator = false;
  dst->randomAccessIndicator = false;
  dst->elementaryStreamPriorityIndicator = false;
  dst->pcrFlag = pcrPresence;
  dst->opcrFlag = false;
  dst->splicingPointFlag = false;
  dst->transportPrivateDataFlag = false;
  dst->adaptationFieldExtensionFlag = false;

  if (dst->pcrFlag)
    dst->pcr = pcrValue;

  if (isESLibbluStream(stream))
    remainingPayload = remainingPesDataLibbluES(stream->es);
  else
    remainingPayload = remainingTableDataLibbluSystemStream(stream->sys);

  dst->stuffingBytesLen = 0;
  adaptationFieldSize = computeRequiredSizeAdaptationField(*dst);
  if (!adaptationFieldSize) {
    switch (remainingPayload) {
      case TP_SIZE - TP_HEADER_SIZE:
        break;

      case TP_SIZE - TP_HEADER_SIZE - 1:
        dst->writeOnlyLength = true;
        break;

      default:
        dst->stuffingBytesLen = TP_SIZE - TP_HEADER_SIZE - remainingPayload - 2;
    }
  }
  else {
    dst->stuffingBytesLen = TP_SIZE - TP_HEADER_SIZE - adaptationFieldSize - remainingPayload;
  }
}

void prepareTPHeader(
  TPHeaderParameters * dst,
  LibbluStreamPtr stream,
  bool pcrInjectionRequirement,
  uint64_t pcrValue
)
{
  if (isESLibbluStream(stream))
    prepareESTransportPacketMainHeader(dst, stream, pcrInjectionRequirement);
  else
    prepareSysTransportPacketMainHeader(dst, stream, pcrInjectionRequirement);

  if (dst->adaptationFieldControl & 0x2)
    prepareAdaptationField(
      &dst->adaptationField,
      stream,
      pcrInjectionRequirement,
      pcrValue
    );
}

static size_t insertAdaptationField(
  uint8_t * tp,
  size_t offset,
  AdaptationFieldParameters param
)
{
  size_t i, adaptationFieldLengthOff;

  /* [u8 adaptation_field_length] */
  adaptationFieldLengthOff = offset;
  WB_ARRAY(tp, offset, 0x00);

  if (!param.writeOnlyLength) {
    uint8_t flagsByte;

    /**
     * [b1 discontinuity_indicator]
     * [b1 random_access_indicator]
     * [b1 elementary_stream_priority_indicator]
     * [b1 PCR_flag]
     * [b1 OPCR_flag]
     * [b1 splicing_point_flag]
     * [b1 transport_private_data_flag]
     * [b1 adaptation_field_extension_flag]
     */
    flagsByte =
      ((param.discontinuityIndicator             ) << 7)
      | ((param.randomAccessIndicator            ) << 6)
      | ((param.elementaryStreamPriorityIndicator) << 5)
      | ((param.pcrFlag                          ) << 4)
      | ((param.opcrFlag                         ) << 3)
      | ((param.splicingPointFlag                ) << 2)
      | ((param.transportPrivateDataFlag         ) << 1)
      | param.adaptationFieldExtensionFlag
    ;
    WB_ARRAY(tp, offset, flagsByte);

    if (param.pcrFlag) {
      uint64_t programClockReferenceBase;
      uint16_t programClockReferenceExt;

      programClockReferenceBase = param.pcr / 300;
      programClockReferenceExt  = param.pcr % 300;

      /**
       * [u33 program_clock_reference_base]
       * [v6 reserved]
       * [u9 program_clock_reference_extension]
       */
      WB_ARRAY(tp, offset, programClockReferenceBase >> 25);
      WB_ARRAY(tp, offset, programClockReferenceBase >> 17);
      WB_ARRAY(tp, offset, programClockReferenceBase >>  9);
      WB_ARRAY(tp, offset, programClockReferenceBase >>  1);
      WB_ARRAY(
        tp, offset,
        (programClockReferenceBase <<  7)
        | ((programClockReferenceExt >> 8) & 0x1)
        | 0x7E
      );
      WB_ARRAY(tp, offset, programClockReferenceExt);
    }

    if (param.opcrFlag) {
      uint64_t originalProgramClockReferenceBase;
      uint16_t originalProgramClockReferenceExt;

      originalProgramClockReferenceBase = param.opcr / 300;
      originalProgramClockReferenceExt  = param.opcr % 300;

      /**
       * [u33 original_program_clock_reference_base]
       * [v6 reserved]
       * [u9 original_program_clock_reference_extension]
       */
      WB_ARRAY(tp, offset, originalProgramClockReferenceBase >> 25);
      WB_ARRAY(tp, offset, originalProgramClockReferenceBase >> 17);
      WB_ARRAY(tp, offset, originalProgramClockReferenceBase >>  9);
      WB_ARRAY(tp, offset, originalProgramClockReferenceBase >>  1);
      WB_ARRAY(
        tp, offset,
        (originalProgramClockReferenceBase <<  7)
        | ((originalProgramClockReferenceExt >> 8) & 0x1)
        | 0x7E
      );
      WB_ARRAY(tp, offset, originalProgramClockReferenceExt);
    }

    if (param.splicingPointFlag) {
      /* [u8 splice_countdown] */
      WB_ARRAY(tp, offset, param.spliceCountdown);
    }

    if (param.transportPrivateDataFlag) {
      uint8_t i;

      /* [u8 transport_private_data_length] */
      WB_ARRAY(tp, offset, param.transportPrivateDataLength);

      for (i = 0; i < param.transportPrivateDataLength; i++) {
        /* [v8 private_data_byte] */
        WB_ARRAY(tp, offset, param.transportPrivateData[i]);
      }
    }

    if (param.adaptationFieldExtensionFlag) {
      size_t adaptationFieldExtensionLengthOff;

      /* [u8 adaptation_field_extension_length] */
      adaptationFieldExtensionLengthOff = offset;
      WB_ARRAY(tp, offset, 0x00);

      /**
       * [b1 ltw_flag]
       * [b1 piecewise_rate_flag]
       * [b1 seamless_splice_flag]
       * // [b1 af_descriptor_not_present_flag]
       * [v6 reserved]
       */
      WB_ARRAY(
        tp, offset,
        (param.ext.ltwFlag              << 7)
        | (param.ext.piecewiseRateFlag  << 6)
        | (param.ext.seamlessSpliceFlag << 5)
        | 0x1F
      );

      if (param.ext.ltwFlag) {
        /* [b1 ltw_valid_flag] [u15 ltw_offset] */
        WB_ARRAY(
          tp, offset,
          (param.ext.ltwValidFlag  << 7)
          | ((param.ext.ltwOffset >> 8) & 0x7F)
        );
        WB_ARRAY(tp, offset, param.ext.ltwOffset);
      }

      if (param.ext.piecewiseRateFlag) {
        /* [v2 reserved] [u22 piecewise_rate] */
        WB_ARRAY(tp, offset, (param.ext.piecewiseRate >> 16) | 0xC0);
        WB_ARRAY(tp, offset,  param.ext.piecewiseRate >>  8);
        WB_ARRAY(tp, offset,  param.ext.piecewiseRate);
      }

      if (param.ext.seamlessSpliceFlag) {
        /**
         * [u4 Splice_type]
         * [u3 DTS_next_AU[32-30]]
         * [v1 marker_bit]
         * [u15 DTS_next_AU[29-15]]
         * [v1 marker_bit]
         * [u15 DTS_next_AU[14-0]]
         * [v1 marker_bit]
         */
        WB_ARRAY(
          tp, offset,
          (param.ext.spliceType << 4)
          | ((param.ext.dtsNextAU >> 29) & 0xE0)
          | 0x1
        );
        WB_ARRAY(tp, offset,  param.ext.dtsNextAU >> 22);
        WB_ARRAY(tp, offset, (param.ext.dtsNextAU >> 14) | 0x1);
        WB_ARRAY(tp, offset,  param.ext.dtsNextAU >>  7);
        WB_ARRAY(tp, offset, (param.ext.dtsNextAU <<  1) | 0x1);
      }

      /* TODO: if (!af_descriptor_not_present_flag) */

      /* [u8*N reserved] // NOT USED */

      /* Set adaptation field extension length : */
      SB_ARRAY(
        tp,
        adaptationFieldExtensionLengthOff,
        offset - adaptationFieldExtensionLengthOff - 1
      );
    }

    for (i = 0; i < param.stuffingBytesLen; i++) {
      /* [v8 stuffing_byte] // 0xFF */
      WB_ARRAY(tp, offset, 0xFF);
    }
  }

  /* Set adaptation field length : */
  SB_ARRAY(
    tp,
    adaptationFieldLengthOff,
    offset - adaptationFieldLengthOff - 1
  );

  return offset;
}

static size_t insertPacketHeader(
  uint8_t * tp,
  size_t offset,
  TPHeaderParameters param
)
{
  /* [v8 syncByte] */
  WB_ARRAY(tp, offset, TP_SYNC_BYTE);

  /*
    [b1 transportErrorIndicator]
    [b1 payloadUnitStartIndicator]
    [b1 transportPriority]
    [u13 pid]
  */
  WB_ARRAY(
    tp, offset,
    (param.transportErrorIndicator     << 7)
    | (param.payloadUnitStartIndicator << 6)
    | (param.transportPriority         << 5)
    | ((param.pid >> 8) & 0x1F)
  );
  WB_ARRAY(
    tp, offset,
    param.pid
  );

  /**
   * [v2 transportScramblingControl]
   * [v2 adaptationFieldControl]
   * [u4 continuityCounter]
   */
  WB_ARRAY(
    tp, offset,
    (param.transportScramblingControl  << 6)
    | (param.adaptationFieldControl    << 4)
    | param.continuityCounter
  );

  if (adaptationFieldPresenceTPHeader(param))
    return insertAdaptationField(tp, offset, param.adaptationField);
  return offset;
}

static size_t insertPayload(
  uint8_t * tp,
  size_t offset,
  LibbluStreamPtr stream,
  size_t payloadSize
)
{
  if (isESLibbluStream(stream)) {
#if 1
    LibbluESPesPacketData * pesPacket = &stream->es.curPesPacket.data;

    assert(payloadSize <= pesPacket->dataUsedSize - pesPacket->dataOffset);

    WA_ARRAY(tp, offset, pesPacket->data + pesPacket->dataOffset, payloadSize);
    pesPacket->dataOffset += payloadSize;
#else
  LIBBLU_ERROR_EXIT("Missing code " __FILE__ ", line %d\n", __LINE__);
#endif
  }
  else {
    LibbluSystemStream * sys = &stream->sys;

    assert(payloadSize <= sys->dataLength - sys->dataOffset);

    WA_ARRAY(tp, offset, sys->data + sys->dataOffset, payloadSize);
    sys->dataOffset += payloadSize;

    /* Reset the table writing offset if its end is reached
    (and mark as fully supplied at least one time) */
    if (!remainingTableDataLibbluSystemStream(stream->sys))
      resetTableWritingOffLibbluSystemStream(sys);
  }

  return offset;
}

int writeTransportPacket(
  BitstreamWriterPtr output,
  LibbluStreamPtr stream,
  TPHeaderParameters header,
  size_t * headerSize,
  size_t * payloadSize
)
{
  uint8_t tp[TP_SIZE];
  size_t hdrSize, pldSize;

  if (header.adaptationFieldControl == 0x00)
    LIBBLU_ERROR_RETURN(
      "Unable to write transport packet, "
      "reserved value 'adaptation_field_control' == 0x00.\n"
    );

  /* transport_packet header */
  hdrSize = insertPacketHeader(tp, 0, header);
  assert(hdrSize <= TP_SIZE);
  assert(hdrSize == computeSizeTPHeader(header));

  /* transport_packet data_byte payload */
  pldSize = TP_SIZE - hdrSize;
  assert(payloadPresenceTPHeader(header) ^ !pldSize);
  if (0 < pldSize) {
    if (!payloadPresenceTPHeader(header))
      LIBBLU_ERROR_RETURN(
        "Unexpected presence of payload in transport packet (%zu bytes).\n",
        pldSize
      );

    insertPayload(tp, hdrSize, stream, pldSize);
  } else {
    if (!isESLibbluStream(stream))
      markAsSuppliedLibbluSystemStream(&stream->sys);
  }

  /* Write generated transport packet */
  if (writeBytes(output, tp, TP_SIZE) < 0)
    return -1;

  if (NULL != headerSize)
    *headerSize = hdrSize;
  if (NULL != payloadSize)
    *payloadSize = pldSize;

  stream->packetNb++;
  return 0;
}