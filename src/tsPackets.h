/** \~english
 * \file tsPackets.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief Transport Packets handling module.
 */

#ifndef __LIBBLU_MUXER_TS_PACKETS_H__
#define __LIBBLU_MUXER_TS_PACKETS_H__

#include "util.h"
#include "stream.h"

/** \~english
 * \brief Writes tp_extra_header structure.
 *
 * \param output Output bistream.
 * \param atc Transport packet Arrival Time Clock timestamp.
 * \return int
 */
int writeTpExtraHeader(
  BitstreamWriterPtr output,
  uint64_t atc
);

typedef struct {
  bool ltw_flag;
  bool piecewise_rate_flag;
  bool seamless_splice_flag;
  // bool af_descriptor_not_present_flag;

  /* if (ltw_flag) */
  bool ltw_valid_flag;
  uint16_t ltw_offset;

  /* if (piecewise_rate_flag) */
  uint32_t piecewise_rate;

  /* if (seamless_splice_flag) */
  uint8_t splice_type;
  uint64_t DTS_next_AU;
} AdaptationFieldExtensionParameters;

/** \~english
 * \brief
 *
 * Composed of:
 *  - u8  : adaptation_field_extension_length
 *  - b1  : ltw_flag
 *  - b1  : piecewise_rate_flag
 *  - b1  : seamless_splice_flag
 *  - b1  : af_descriptor_not_present_flag
 *  - v4  : reserved
 *  if (ltw_flag)
 *    - b1  : ltw_valid_flag
 *    - u15 : ltw_offset
 *  if (piecewise_rate_flag)
 *    - v2  : reserved
 *    - u22 : piecewise_rate
 *  if (seamless_splice_flag)
 *    - u4  : Splice_type
 *    - u3  : DTS_next_AU[32..30]
 *    - v1  : marker_bit
 *    - u15 : DTS_next_AU[29..15]
 *    - v1  : marker_bit
 *    - u15 : DTS_next_AU[14..0]
 *    - v1  : marker_bit
 *   if (af_descriptor_not_present_flag)
 *     - vn  : af_descriptor()s
 *   else
 *     - vn  : reserved
 */
static inline uint8_t computeSizeAdaptationFieldExtensionParameters(
  const AdaptationFieldExtensionParameters *param
)
{
  uint8_t size = 2;
  if (param->ltw_flag)
    size += 2;
  if (param->piecewise_rate_flag)
    size += 3;
  if (param->seamless_splice_flag)
    size += 5;
  return size;
}

/** \~english
 * \brief Offset of the 'program_clock_reference_base' field from the transport
 * packet first byte.
 *
 */
#define TP_PCR_FIELD_OFF  0x0A

static inline uint64_t computePcrFieldValue(
  double pcr,
  double byte_STC_dur
)
{
  return (uint64_t) (pcr + TP_PCR_FIELD_OFF *byte_STC_dur);
}

typedef struct {
  bool write_only_length;

  bool discontinuity_indicator;
  bool random_access_indicator;
  bool elementary_stream_priority_indicator;
  bool PCR_flag;
  bool OPCR_flag;
  bool splicing_point_flag;
  bool transport_private_data_flag;
  bool adaptation_field_extension_flag;

  /* if (PCR_flag) */
  uint64_t pcr;

  /* if (OPCR_flag) */
  uint64_t opcr;

  /* if (splicing_point_flag) */
  uint8_t splice_countdown;

  /* if (transport_private_data_flag) */
  uint8_t transport_private_data_length;
  uint8_t *private_data;

  /* if (adaptation_field_extension_flag) */
  AdaptationFieldExtensionParameters adaptation_field_extension;

  uint8_t nb_stuffing_bytes;
} AdaptationFieldParameters;

/** \~english
 * \brief Return in bytes the size of the transport stream adaptation field
 * data according to supplied parameters.
 *
 * \param param Adaptation field settings.
 * \return size_t The size of the adaptation field in bytes.
 *
 * Composed of:
 *  - u8  : adaptation_field_length
 *  if (!param->write_only_length) // 0 < adaptation_field_length
 *    - b1  : discontinuity_indicator
 *    - b1  : section_syntax_indicator
 *    - b1  : elementary_stream_priority_indicator
 *    - b1  : PCR_flag
 *    - b1  : OPCR_flag
 *    - b1  : splicing_point_flag
 *    - b1  : transport_private_data_flag
 *    - b1  : adaptation_field_extension_flag
 *    if (PCR_flag)
 *      - u33 : program_clock_reference_base
 *      - v6  : reserved
 *      - u9  : program_clock_reference_extension
 *    if (OPCR_flag)
 *      - u33 : original_program_clock_reference_base
 *      - v6  : reserved
 *      - u9  : original_program_clock_reference_extension
 *    if (splicing_point_flag)
 *      - u8  : splice_countdown
 *    if (transport_private_data_flag)
 *      - u8  : transport_private_data_length
 *      - v<8*transport_private_data_length> : private_data_byte(s)
 *    if (adaptation_field_extension_flag)
 *      - vn  : adaptation_field_extension()
 *        // computeSizeAdaptationFieldExtensionParameters()
 *    - vn  : stuffing_byte // param->nb_stuffing_bytes
 */
static inline uint8_t computeSizeAdaptationField(
  const AdaptationFieldParameters *param
)
{
  if (param->write_only_length)
    return 1;

  uint8_t size = 2;
  if (param->PCR_flag)
    size += 6;
  if (param->OPCR_flag)
    size += 6;
  if (param->splicing_point_flag)
    size++;
  if (param->transport_private_data_flag)
    size += 1 + param->transport_private_data_length;
  if (param->adaptation_field_extension_flag)
    size += computeSizeAdaptationFieldExtensionParameters(
      &param->adaptation_field_extension
    );
  return size + param->nb_stuffing_bytes;
}

static inline uint8_t computeRequiredSizeAdaptationField(
  const AdaptationFieldParameters *param
)
{
  bool adapt_field_used = (
    param->discontinuity_indicator
    || param->random_access_indicator
    || param->elementary_stream_priority_indicator
    || param->PCR_flag
    || param->OPCR_flag
    || param->splicing_point_flag
    || param->transport_private_data_flag
    || param->adaptation_field_extension_flag
  );

  if (adapt_field_used)
    return computeSizeAdaptationField(param);
  return 0;
}

typedef struct {
  bool transport_error_indicator;
  bool payload_unit_start_indicator;
  bool transport_priority;

  uint16_t pid;

  uint8_t transport_scrambling_control;
  uint8_t adaptation_field_control;
  uint8_t continuity_counter;

  AdaptationFieldParameters adaptation_field;
} TPHeaderParameters;

static inline bool adaptationFieldPresenceTPHeader(
  const TPHeaderParameters *header
)
{
  return header->adaptation_field_control & 0x2;
}

static inline bool payloadPresenceTPHeader(
  const TPHeaderParameters *header
)
{
  return header->adaptation_field_control & 0x1;
}

static inline uint8_t computeSizeTPHeader(
  const TPHeaderParameters *header
)
{
  if (!adaptationFieldPresenceTPHeader(header))
    return TP_HEADER_SIZE;
  return TP_HEADER_SIZE + computeSizeAdaptationField(
    &header->adaptation_field
  );
}

TPHeaderParameters prepareTPHeader(
  const LibbluStreamPtr stream,
  bool pcr_injection_req,
  uint64_t pcr_value
);

int writeTransportPacket(
  BitstreamWriterPtr output,
  LibbluStreamPtr stream,
  const TPHeaderParameters *tp_header,
  uint8_t *tp_header_size_ret,
  uint8_t *tp_payload_size_ret
);

#endif