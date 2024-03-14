/** \~english
 * \file mainMuxer.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief Multiplexer main main module.
 */

#ifndef __LIBBLU_MUXER_MAINMUXER_H__
#define __LIBBLU_MUXER_MAINMUXER_H__

#include "util.h"
#include "muxingSettings.h"
#include "muxingContext.h"

/** \~english
 * \brief Multiplexer main function.
 *
 * \param settings Settings used for the mux. Supplied structure may be
 * modified by the function and shall not modified nor cleaned after
 * call (even if function return an error).
 * \return int
 */
int mainMux(
  const LibbluMuxingSettings *settings
);

#endif