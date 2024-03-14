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
  uint64_t atc
)
{
  uint8_t tp_extra_header[TP_EXTRA_HEADER_SIZE];
  uint8_t offset = 0;

  static const uint8_t copy_perm_idc = 0x0;
  uint32_t ats = atc & 0x3FFFFFFF;

  /* [u2 copy_permission_indicator] [u30 arrival_timestamp] */
  WB_ARRAY(tp_extra_header, offset, (copy_perm_idc << 6) | (ats >> 24));
  WB_ARRAY(tp_extra_header, offset, ats >> 16);
  WB_ARRAY(tp_extra_header, offset, ats >>  8);
  WB_ARRAY(tp_extra_header, offset, ats);

  return writeBytes(
    output,
    tp_extra_header,
    TP_EXTRA_HEADER_SIZE
  );
}

static TPHeaderParameters _prepareESTransportPacketMainHeader(
  const LibbluStreamPtr stream,
  bool pcr_inj_req
)
{
  size_t rem_data = remainingPesDataLibbluES(&stream->es);

  assert(0 < rem_data);
  assert(TYPE_ES == stream->type);

  const LibbluESProperties *es_prop = &stream->es.prop;
  bool transport_priority = (
    (
      es_prop->coding_type == STREAM_CODING_TYPE_AC3
      || es_prop->coding_type == STREAM_CODING_TYPE_DTS
      || es_prop->coding_type == STREAM_CODING_TYPE_TRUEHD
      || es_prop->coding_type == STREAM_CODING_TYPE_EAC3
      || es_prop->coding_type == STREAM_CODING_TYPE_HDHR
      || es_prop->coding_type == STREAM_CODING_TYPE_HDMA
    ) && !stream->es.current_pes_packet.extension_frame
  );

  bool pl_presence = true; // (0 < rem_data)
  bool is_pl_start = (pl_presence && isPayloadUnitStartLibbluES(stream->es));
  bool adapt_fd_pres = (rem_data < TP_PAYLOAD_SIZE || pcr_inj_req);

  return (TPHeaderParameters) {
    .payload_unit_start_indicator = is_pl_start,
    .transport_priority = transport_priority,
    .pid = stream->pid,
    .adaptation_field_control = (adapt_fd_pres << 1) | pl_presence,
    .continuity_counter = stream->nb_transport_packets & 0xF
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
  return stream->sys.table_data_length - stream->sys.table_data_offset;
}

static TPHeaderParameters _prepareSysTransportPacketMainHeader(
  const LibbluStreamPtr stream,
  bool pcr_inj_req
)
{
  size_t rem_size = _remainingTableDataLibbluSystemStream(stream);

  bool pl_pres = (0 < rem_size);
  bool is_pl_start = (pl_pres && 0x0 == stream->sys.table_data_offset);
  bool adapt_field_pres = (rem_size < TP_PAYLOAD_SIZE || pcr_inj_req);

  return (TPHeaderParameters) {
    .payload_unit_start_indicator = is_pl_start && (TYPE_NULL != stream->type),
    .pid = stream->pid,
    .adaptation_field_control = (adapt_field_pres << 1) | pl_pres,
    .continuity_counter = (pl_pres) ? stream->nb_transport_packets & 0xF : 0
  };
}

static AdaptationFieldParameters _prepareAdaptationField(
  const LibbluStreamPtr stream,
  bool pcr_injection_req,
  uint64_t pcr_value
)
{
  AdaptationFieldParameters param = {
    .PCR_flag = ((stream->type == TYPE_PCR) || pcr_injection_req),
    .pcr = pcr_value
  };

  size_t rem_payload;
  if (isESLibbluStream(stream))
    rem_payload = remainingPesDataLibbluES(&stream->es);
  else
    rem_payload = _remainingTableDataLibbluSystemStream(stream);
  assert(rem_payload <= TP_PAYLOAD_SIZE);

  uint8_t adapt_fd_size = computeRequiredSizeAdaptationField(&param);

  if (!adapt_fd_size) {
    /* No adaptation field required */
    switch (rem_payload) {
    case TP_PAYLOAD_SIZE: // Payload size match exactly TS packet one.
      break;

    case TP_PAYLOAD_SIZE - 1: // Only one stuffing byte required.
      param.write_only_length = true;
      break;

    default: // Add stuffing bytes to pad
      param.nb_stuffing_bytes = TP_PAYLOAD_SIZE - rem_payload - 2;
    }
  }
  else {
    param.nb_stuffing_bytes = TP_PAYLOAD_SIZE - adapt_fd_size - rem_payload;
  }

  return param;
}

TPHeaderParameters prepareTPHeader(
  const LibbluStreamPtr stream,
  bool pcr_injection_req,
  uint64_t pcr_value
)
{
  TPHeaderParameters tp_header;

  if (isESLibbluStream(stream))
    tp_header = _prepareESTransportPacketMainHeader(stream, pcr_injection_req);
  else
    tp_header = _prepareSysTransportPacketMainHeader(stream, pcr_injection_req);

  if (tp_header.adaptation_field_control & 0x2) {
    tp_header.adaptation_field = _prepareAdaptationField(
      stream,
      pcr_injection_req,
      pcr_value
    );
  }

  return tp_header;
}

static uint8_t _insertAdaptationField(
  uint8_t *tp,
  uint8_t offset,
  const AdaptationFieldParameters *param
)
{
  /* [u8 adaptation_field_length] */
  uint8_t adapt_fd_len_off = offset;
  WB_ARRAY(tp, offset, 0x00);

  if (!param->write_only_length) {
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
    uint8_t flags_byte =
      ((param->discontinuity_indicator               ) << 7)
      | ((param->random_access_indicator             ) << 6)
      | ((param->elementary_stream_priority_indicator) << 5)
      | ((param->PCR_flag                            ) << 4)
      | ((param->OPCR_flag                           ) << 3)
      | ((param->splicing_point_flag                 ) << 2)
      | ((param->transport_private_data_flag         ) << 1)
      | param->adaptation_field_extension_flag
    ;
    WB_ARRAY(tp, offset, flags_byte);

    if (param->PCR_flag) {
      uint64_t programClockReferenceBase;
      uint16_t programClockReferenceExt;

      programClockReferenceBase = param->pcr / 300;
      programClockReferenceExt  = param->pcr % 300;

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

    if (param->OPCR_flag) {
      uint64_t originalProgramClockReferenceBase;
      uint16_t originalProgramClockReferenceExt;

      originalProgramClockReferenceBase = param->opcr / 300;
      originalProgramClockReferenceExt  = param->opcr % 300;

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

    if (param->splicing_point_flag) {
      /* [u8 splice_countdown] */
      WB_ARRAY(tp, offset, param->splice_countdown);
    }

    if (param->transport_private_data_flag) {
      uint8_t i;

      /* [u8 transport_private_data_length] */
      WB_ARRAY(tp, offset, param->transport_private_data_length);

      for (i = 0; i < param->transport_private_data_length; i++) {
        /* [v8 private_data_byte] */
        WB_ARRAY(tp, offset, param->private_data[i]);
      }
    }

    if (param->adaptation_field_extension_flag) {
      const AdaptationFieldExtensionParameters *ext =
        &param->adaptation_field_extension
      ;

      /* [u8 adaptation_field_extension_length] */
      uint8_t adapt_fd_ext_len_off = offset;
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
        (ext->ltw_flag              << 7)
        | (ext->piecewise_rate_flag  << 6)
        | (ext->seamless_splice_flag << 5)
        | 0x1F
      );

      if (ext->ltw_flag) {
        /* [b1 ltw_valid_flag] [u15 ltw_offset] */
        WB_ARRAY(
          tp, offset,
          (ext->ltw_valid_flag  << 7)
          | ((ext->ltw_offset >> 8) & 0x7F)
        );
        WB_ARRAY(tp, offset, ext->ltw_offset);
      }

      if (ext->piecewise_rate_flag) {
        /* [v2 reserved] [u22 piecewise_rate] */
        WB_ARRAY(tp, offset, (ext->piecewise_rate >> 16) | 0xC0);
        WB_ARRAY(tp, offset,  ext->piecewise_rate >>  8);
        WB_ARRAY(tp, offset,  ext->piecewise_rate);
      }

      if (ext->seamless_splice_flag) {
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
          (ext->splice_type << 4)
          | ((ext->DTS_next_AU >> 29) & 0xE0)
          | 0x1
        );
        WB_ARRAY(tp, offset,  ext->DTS_next_AU >> 22);
        WB_ARRAY(tp, offset, (ext->DTS_next_AU >> 14) | 0x1);
        WB_ARRAY(tp, offset,  ext->DTS_next_AU >>  7);
        WB_ARRAY(tp, offset, (ext->DTS_next_AU <<  1) | 0x1);
      }

      /* TODO: if (!af_descriptor_not_present_flag) */

      /* [u8*N reserved] // NOT USED */

      /* Set adaptation field extension length : */
      SB_ARRAY(
        tp,
        adapt_fd_ext_len_off,
        offset - adapt_fd_ext_len_off - 1
      );
    }

    for (uint8_t i = 0; i < param->nb_stuffing_bytes; i++) {
      /* [v8 stuffing_byte] // 0xFF */
      WB_ARRAY(tp, offset, 0xFF);
    }
  }

  /* Set adaptation field length : */
  SB_ARRAY(
    tp,
    adapt_fd_len_off,
    offset - adapt_fd_len_off - 1
  );

  return offset;
}

static uint8_t _insertPacketHeader(
  uint8_t *tp,
  uint8_t offset,
  const TPHeaderParameters *param
)
{
  /* [v8 sync_byte] // 0x47 */
  WB_ARRAY(tp, offset, TP_SYNC_BYTE);

  /*
    [b1 transport_error_indicator]
    [b1 payload_unit_start_indicator]
    [b1 transport_priority]
    [u13 PID]
  */
  WB_ARRAY(
    tp, offset,
    (param->transport_error_indicator      << 7)
    | (param->payload_unit_start_indicator << 6)
    | (param->transport_priority           << 5)
    | ((param->pid >> 8) & 0x1F)
  );
  WB_ARRAY(
    tp, offset,
    param->pid
  );

  /**
   * [v2 transport_scrambling_control]
   * [v2 adaptation_field_control]
   * [u4 continuity_counter]
   */
  WB_ARRAY(
    tp, offset,
    (param->transport_scrambling_control  << 6)
    | (param->adaptation_field_control    << 4)
    | param->continuity_counter
  );

  if (adaptationFieldPresenceTPHeader(param))
    return _insertAdaptationField(tp, offset, &param->adaptation_field);
  return offset;
}

static uint8_t _insertPayload(
  uint8_t *tp,
  uint8_t offset,
  LibbluStreamPtr stream,
  uint8_t pl_size
)
{
  if (isESLibbluStream(stream)) {
    LibbluESPesPacketData *pes_packet = &stream->es.current_pes_packet.data;

    assert(pl_size <= pes_packet->size - pes_packet->offset);

    WA_ARRAY(tp, offset, &pes_packet->data[pes_packet->offset], pl_size);
    pes_packet->offset += pl_size;
  }
  else {
    LibbluSystemStream *sys = &stream->sys;

    assert(pl_size <= sys->table_data_length - sys->table_data_offset);

    WA_ARRAY(tp, offset, &sys->table_data[sys->table_data_offset], pl_size);
    sys->table_data_offset += pl_size;

    /* Reset the table writing offset if its end is reached
    (and mark as fully supplied at least one time) */
    if (!_remainingTableDataLibbluSystemStream(stream)) {
      stream->sys.table_data_offset = 0;
      stream->sys.first_full_table_supplied = true;
    }
  }

  return offset;
}

int writeTransportPacket(
  BitstreamWriterPtr output,
  LibbluStreamPtr stream,
  const TPHeaderParameters *tp_header,
  uint8_t *tp_header_size_ret,
  uint8_t *tp_payload_size_ret
)
{
  uint8_t tp_data[TP_SIZE];

  if (tp_header->adaptation_field_control == 0x00)
    LIBBLU_ERROR_RETURN(
      "Unable to write transport packet, "
      "reserved value 'adaptation_field_control' == 0x00.\n"
    );

  /* transport_packet header */
  uint8_t header_size = _insertPacketHeader(tp_data, 0, tp_header);

  assert(header_size <= TP_SIZE);
  assert(header_size == computeSizeTPHeader(tp_header));

  /* transport_packet data_byte payload */
  uint8_t payload_size = TP_SIZE - header_size;

  assert(payloadPresenceTPHeader(tp_header) ^ !payload_size);

  if (0 < payload_size) {
    if (!payloadPresenceTPHeader(tp_header))
      LIBBLU_ERROR_RETURN(
        "Unexpected presence of payload in transport packet (%zu bytes).\n",
        payload_size
      );

    _insertPayload(tp_data, header_size, stream, payload_size);
  } else {
    if (!isESLibbluStream(stream)) {
      stream->sys.first_full_table_supplied = true;
    }
  }

  /* Write generated transport packet */
  if (writeBytes(output, tp_data, TP_SIZE) < 0)
    return -1;

  if (NULL != tp_header_size_ret)
    *tp_header_size_ret = header_size;
  if (NULL != tp_payload_size_ret)
    *tp_payload_size_ret = payload_size;

  stream->nb_transport_packets++;
  return 0;
}