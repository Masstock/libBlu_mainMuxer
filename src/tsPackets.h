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

int writeTpExtraHeader(
  BitstreamWriterPtr output,
  uint64_t pcr
);

typedef struct {
  bool ltwFlag;
  bool piecewiseRateFlag;
  bool seamlessSpliceFlag;
  /* bool afDescriptorNotPresent; */

  /* if (ltwFlag) */
  bool ltwValidFlag;
  uint16_t ltwOffset;

  /* if (piecewiseRateFlag) */
  uint32_t piecewiseRate;

  /* if (seamlessSpliceFlag) */
  uint8_t spliceType;
  uint64_t dtsNextAU;
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
static inline size_t computeSizeAdaptationFieldExtensionParameters(
  AdaptationFieldExtensionParameters param
)
{
  size_t size;

  size = 2;
  if (param.ltwFlag)
    size += 2;
  if (param.piecewiseRateFlag)
    size += 3;
  if (param.seamlessSpliceFlag)
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
  double byteStcDuration
)
{
  return (uint64_t) (pcr + TP_PCR_FIELD_OFF * byteStcDuration);
}

typedef struct {
  bool writeOnlyLength;

  bool discontinuityIndicator;
  bool randomAccessIndicator;
  bool elementaryStreamPriorityIndicator;
  bool pcrFlag;
  bool opcrFlag;
  bool splicingPointFlag;
  bool transportPrivateDataFlag;
  bool adaptationFieldExtensionFlag;

  /* if (pcrFlag) */
  uint64_t pcr;

  /* if (opcrFlag) */
  uint64_t opcr;

  /* if (splicingPointFlag) */
  uint8_t spliceCountdown;

  /* if (transportPrivateDataFlag) */
  uint8_t transportPrivateDataLength;
  uint8_t * transportPrivateData;

  /* if (adaptationFieldExtensionFlag) */
  AdaptationFieldExtensionParameters ext;

  size_t stuffingBytesLen;
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
 *  if (!param.writeOnlyLength) // 0 < adaptation_field_length
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
 *    - vn  : stuffing_byte // param.stuffingBytesLen
 */
static inline size_t computeSizeAdaptationField(
  AdaptationFieldParameters param
)
{
  size_t size;

  if (param.writeOnlyLength)
    return 1;

  size = 2;
  if (param.pcrFlag)
    size += 6;
  if (param.opcrFlag)
    size += 6;
  if (param.splicingPointFlag)
    size++;
  if (param.transportPrivateDataFlag)
    size += 1 + param.transportPrivateDataLength;
  if (param.adaptationFieldExtensionFlag)
    size += computeSizeAdaptationFieldExtensionParameters(param.ext);
  return size + param.stuffingBytesLen;
}

static inline size_t computeRequiredSizeAdaptationField(
  AdaptationFieldParameters param
)
{
  bool adaptationFieldUsed;

  adaptationFieldUsed = (
    param.discontinuityIndicator
    || param.randomAccessIndicator
    || param.elementaryStreamPriorityIndicator
    || param.pcrFlag
    || param.opcrFlag
    || param.splicingPointFlag
    || param.transportPrivateDataFlag
    || param.adaptationFieldExtensionFlag
  );

  if (adaptationFieldUsed)
    return computeSizeAdaptationField(param);
  return 0;
}

typedef struct {
  bool transportErrorIndicator;
  bool payloadUnitStartIndicator;
  bool transportPriority;

  uint16_t pid;

  uint8_t transportScramblingControl:2;
  uint8_t adaptationFieldControl:2;
  uint8_t continuityCounter:4;

  AdaptationFieldParameters adaptationField;
} TPHeaderParameters;

static inline bool adaptationFieldPresenceTPHeader(
  TPHeaderParameters header
)
{
  return header.adaptationFieldControl & 0x2;
}

static inline bool payloadPresenceTPHeader(
  TPHeaderParameters header
)
{
  return header.adaptationFieldControl & 0x1;
}

static inline size_t computeSizeTPHeader(
  TPHeaderParameters header
)
{
  if (adaptationFieldPresenceTPHeader(header))
    return TP_HEADER_SIZE + computeSizeAdaptationField(header.adaptationField);
  return TP_HEADER_SIZE;
}

void prepareTPHeader(
  TPHeaderParameters * dst,
  LibbluStreamPtr stream,
  bool pcrInjectionRequirement,
  uint64_t pcrValue
);

int writeTransportPacket(
  BitstreamWriterPtr output,
  LibbluStreamPtr stream,
  TPHeaderParameters header,
  size_t * headerSize,
  size_t * payloadSize
);

#endif