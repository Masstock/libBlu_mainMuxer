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
    LibbluES es;  /**< Elementary Stream linked parameters.    */
    LibbluSystemStream sys;     /**< Systeam Stream linked parameters.       */
  };

  uint16_t pid;  /**< Stream associated PID.                                 */

  uint32_t packetNb;  /**< Pending number of emited TS packets counter. Used
    for continuity_counter field in transport packets.                       */
} LibbluStream, *LibbluStreamPtr;

static inline bool isESLibbluStream(
  const LibbluStreamPtr stream
)
{
  return isEsStreamType(stream->type);
}

static inline void setPIDLibbluStream(
  LibbluStreamPtr stream,
  uint16_t pid
)
{
  assert(NULL != stream);

  stream->pid = pid;
}

LibbluStreamPtr createElementaryLibbluStream(
  LibbluESSettings * settings
);

LibbluStreamPtr createPATSystemLibbluStream(
  PatParameters param,
  uint16_t pid
);

LibbluStreamPtr createPMTSystemLibbluStream(
  PmtParameters param,
  uint16_t pid
);

LibbluStreamPtr createPCRSystemLibbluStream(
  uint16_t pid
);

LibbluStreamPtr createSITSystemLibbluStream(
  SitParameters param,
  uint16_t pid
);

LibbluStreamPtr createNULLSystemLibbluStream(
  uint16_t pid
);

void destroyLibbluStream(
  LibbluStreamPtr stream
);

int requestESPIDLibbluStream(
  LibbluPIDValues * values,
  uint16_t * pid,
  LibbluStreamPtr stream
);

#endif