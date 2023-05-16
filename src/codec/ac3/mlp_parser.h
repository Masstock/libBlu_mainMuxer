

#ifndef __LIBBLU_MUXER__CODECS__AC3__MLP_PARSER_H__
#define __LIBBLU_MUXER__CODECS__AC3__MLP_PARSER_H__

#include "ac3_data.h"
#include "ac3_error.h"

/** \~english
 * \brief Parse MLP sync header.
 *
 * \param bs Source bitstream.
 * \param sh Destination sync header parameters structure.
 * \param cnv Check nibble value pointer.
 * \return int Upon success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int parseMlpSyncHeader(
  BitstreamReaderPtr bs,
  MlpSyncHeaderParameters * sh,
  uint8_t * cnv
);

/** \~english
 * \brief Parse one MLP substream directory entry.
 *
 * \param bs Source bitstream.
 * \param entry Destination entry parameters structure.
 * \param cnv Check nibble value pointer.
 * \return int Upon success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int parseMlpSubstreamDirectoryEntry(
  BitstreamReaderPtr bs,
  MlpSubstreamDirectoryEntry * entry,
  uint8_t * cnv
);

/** \~english
 * \brief Decode and check one MLP substream segment.
 *
 * \param input Source bitstream.
 * \param substream Destination substream parameters structure.
 * \param entry Parsed substream associated directory entry.
 * \param ss_idx Index of the substream in the access unit.
 * \return int Upon success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int decodeMlpSubstreamSegment(
  BitstreamReaderPtr input,
  MlpSubstreamParameters * substream,
  const MlpSubstreamDirectoryEntry * entry,
  unsigned ss_idx
);

/** \~english
 * \brief Decode and check MLP EXTRA DATA.
 *
 * \param input Source bitstream.
 * \param au_remaining_length Number of remaining 16-bits words in access unit.
 * \return int Upon success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int decodeMlpExtraData(
  BitstreamReaderPtr input,
  unsigned au_remaining_length
);

#endif