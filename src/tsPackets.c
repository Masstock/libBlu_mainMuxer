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

  static const uint8_t copyPermInd = 0x0;

  uint32_t ats = pcr & 0x3FFFFFFF;

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

static void _prepareESTransportPacketMainHeader(
  TPHeaderParameters * dst,
  const LibbluStreamPtr stream,
  bool pcrInjectReq
)
{
  size_t remDataSize = remainingPesDataLibbluES(stream->es);

  assert(0 < remDataSize);

  bool payloadPresence = true; // (0 < remDataSize)
  bool payloadStart = (payloadPresence && isPayloadUnitStartLibbluES(stream->es));
  bool adaptFieldPres = (remDataSize < TP_PAYLOAD_SIZE || pcrInjectReq);

  *dst = (TPHeaderParameters) {
    .transportErrorIndicator = false,
    .payloadUnitStartIndicator = payloadStart,
    .transportPriority = false,
    .pid = stream->pid,
    .transportScramblingControl = 0x00,
    .adaptationFieldControl = (adaptFieldPres << 1) | payloadPresence,
    .continuityCounter = stream->packetNb & 0xF
  };
}

/** \~english
 * \brief Return the number of bytes remaining in the current system table.
 *
 * \param stream System Stream handle.
 * \return size_t Number of remaining bytes in the system packet current table.
 */
static size_t _remainingTableDataLibbluSystemStream(
  const LibbluStreamPtr stream
)
{
  return stream->sys.dataLength - stream->sys.dataOffset;
}

static void _prepareSysTransportPacketMainHeader(
  TPHeaderParameters * dst,
  const LibbluStreamPtr stream,
  bool pcrInjectReq
)
{
  size_t remDataSize = _remainingTableDataLibbluSystemStream(stream);

  bool payloadPresence = (0 < remDataSize);
  bool payloadStart = (payloadPresence && 0x0 == stream->sys.dataOffset);
  bool adaptFieldPres = (remDataSize < TP_PAYLOAD_SIZE || pcrInjectReq);
  bool useContCounter = stream->sys.useContinuityCounter;

  *dst = (TPHeaderParameters) {
    .transportErrorIndicator = false,
    .payloadUnitStartIndicator = payloadStart,
    .transportPriority = false,
    .pid = stream->pid,
    .transportScramblingControl = 0x00,
    .adaptationFieldControl = (adaptFieldPres << 1) | payloadPresence,
    .continuityCounter = (useContCounter) ? stream->packetNb & 0xF : 0
  };
}

static void _prepareAdaptationField(
  AdaptationFieldParameters * dst,
  const LibbluStreamPtr stream,
  bool pcrInjectionRequirement,
  uint64_t pcrValue
)
{
  AdaptationFieldParameters param = {
    .pcrFlag = ((stream->type == TYPE_PCR) || pcrInjectionRequirement),
    .pcr = pcrValue
  };

  size_t remainingPayload;
  if (isESLibbluStream(stream))
    remainingPayload = remainingPesDataLibbluES(stream->es);
  else
    remainingPayload = _remainingTableDataLibbluSystemStream(stream);
  assert(remainingPayload <= TP_PAYLOAD_SIZE);

  size_t adaptationFieldSize = computeRequiredSizeAdaptationField(param);

  if (!adaptationFieldSize) {
    /* No adaptation field required */
    switch (remainingPayload) {
      case TP_PAYLOAD_SIZE: // Payload size match exactly TS packet one.
        break;

      case TP_PAYLOAD_SIZE - 1: // Only one stuffing byte required.
        param.writeOnlyLength = true;
        break;

      default: // Add stuffing bytes to pad
        param.stuffingBytesLen = TP_PAYLOAD_SIZE - remainingPayload - 2;
    }
  }
  else {
    param.stuffingBytesLen = TP_PAYLOAD_SIZE - adaptationFieldSize - remainingPayload;
  }

  *dst = param;
}

void prepareTPHeader(
  TPHeaderParameters * dst,
  const LibbluStreamPtr stream,
  bool pcrInjectionRequirement,
  uint64_t pcrValue
)
{
  if (isESLibbluStream(stream))
    _prepareESTransportPacketMainHeader(dst, stream, pcrInjectionRequirement);
  else
    _prepareSysTransportPacketMainHeader(dst, stream, pcrInjectionRequirement);

  if (dst->adaptationFieldControl & 0x2) {
    _prepareAdaptationField(
      &dst->adaptationField,
      stream,
      pcrInjectionRequirement,
      pcrValue
    );
  }
}

static size_t _insertAdaptationField(
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

static size_t _insertPacketHeader(
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
    return _insertAdaptationField(tp, offset, param.adaptationField);
  return offset;
}

static size_t _insertPayload(
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
    if (!_remainingTableDataLibbluSystemStream(stream)) {
      stream->sys.dataOffset = 0;
      stream->sys.firstFullTableSupplied = true;
    }
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

  if (header.adaptationFieldControl == 0x00)
    LIBBLU_ERROR_RETURN(
      "Unable to write transport packet, "
      "reserved value 'adaptation_field_control' == 0x00.\n"
    );

  /* transport_packet header */
  size_t hdrSize = _insertPacketHeader(tp, 0, header);

  assert(hdrSize <= TP_SIZE);
  assert(hdrSize == computeSizeTPHeader(header));

  /* transport_packet data_byte payload */
  size_t pldSize = TP_SIZE - hdrSize;

  assert(payloadPresenceTPHeader(header) ^ !pldSize);

  if (0 < pldSize) {
    if (!payloadPresenceTPHeader(header))
      LIBBLU_ERROR_RETURN(
        "Unexpected presence of payload in transport packet (%zu bytes).\n",
        pldSize
      );

    _insertPayload(tp, hdrSize, stream, pldSize);
  } else {
    if (!isESLibbluStream(stream)) {
      stream->sys.firstFullTableSupplied = true;
    }
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