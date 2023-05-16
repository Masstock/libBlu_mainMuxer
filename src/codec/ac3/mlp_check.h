

#ifndef __LIBBLU_MUXER__CODECS__AC3__MLP_CHECK_H__
#define __LIBBLU_MUXER__CODECS__AC3__MLP_CHECK_H__

#include "ac3_data.h"
#include "ac3_error.h"

/** \~english
 * \brief Check MLP sync header parameters.
 *
 * \param sync Sync parameters.
 * \param info Bitstream informations destination structure.
 * \param firstAU Is the MLP access unit the first in the bitstream.
 * \return int Upon success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int checkMlpSyncHeader(
  const MlpSyncHeaderParameters * sync,
  MlpInformations * info,
  bool firstAU
);

/** \~english
 * \brief Check MLP substream directory section parameters and compute size
 * of each substream.
 *
 * \param directory Substream directory.
 * \param msi Major sync parameters.
 * \param au_ss_length Total size of the substreams in the access unit in
 * 16-bits word unit. This value is equal to the size of the whole access unit
 * minus the size of the MLP sync (including substream_directory section).
 * \param is_major_sync Is the MLP access unit the first in the bitstream.
 * \return int Upon success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int checkAndComputeSSSizesMlpSubstreamDirectory(
  MlpSubstreamDirectoryEntry * directory,
  const MlpMajorSyncInfoParameters * msi,
  unsigned au_ss_length,
  bool is_major_sync
);

#endif