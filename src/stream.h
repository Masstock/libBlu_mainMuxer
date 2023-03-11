/** \~english
 * \file stream.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief Elementary/System streams common handling module.
 */

#ifndef __LIBBLU_MUXER__STREAM_H__
#define __LIBBLU_MUXER__STREAM_H__

#include "elementaryStream.h"
#include "systemStream.h"

/** \~english
 * \brief Elementary/Stream common handling structure.
 *
 */
typedef struct {
  StreamType type;  /**< Type of the stream.                                 */

  union {
    LibbluES es;             /**< Elementary Stream linked parameters.       */
    LibbluSystemStream sys;  /**< Systeam Stream linked parameters.          */
  };

  uint16_t pid;  /**< Stream associated PID.                                 */

  uint32_t packetNb;  /**< Pending number of emited TS packets counter. Used
    for continuity_counter field in transport packets.                       */
} LibbluStream, *LibbluStreamPtr;

LibbluStreamPtr createLibbluStream(
  StreamType type,
  uint16_t pid
);

void destroyLibbluStream(
  LibbluStreamPtr stream
);

static inline bool isESLibbluStream(
  const LibbluStreamPtr stream
)
{
  return isEsStreamType(stream->type);
}

int requestESPIDLibbluStream(
  LibbluPIDValues * values,
  uint16_t * pid,
  LibbluStreamPtr stream
);

#endif