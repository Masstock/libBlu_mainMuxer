/** \~english
 * \file muxingContext.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief Muxing context module.
 *
 * \xrefitem references "References" "References list"
 *  [1] Siddaraju, Naveen & Rao, Kamisetty. (2012). Multiplexing the
 *      Elementary Streams of H.264 Video and MPEG4 HE AAC v2 Audio,
 *      De-multiplexing and Achieving Lip Synchronization. American Journal
 *      of Signal Processing. 2. 52-59. 10.5923/j.ajsp.20120203.03.
 */

#ifndef __LIBBLU_MUXER__MUXING_CONTEXT_H__
#define __LIBBLU_MUXER__MUXING_CONTEXT_H__

#include "util.h"
#include "muxingSettings.h"
#include "elementaryStream.h"
#include "systemStream.h"
#include "stream.h"
#include "streamHeap.h"
#include "packetIdentifier.h"
#include "codecsUtilities.h"
#include "tsPackets.h"
#include "tStdVerifier/bdavStd.h"

#define SHIFT_PACKETS_BEFORE_DTS true
#define USE_AVERAGE_PES_SIZE false

typedef struct {
  LibbluMuxingSettings settings;  /**< Context associated settings.          */

  LibbluPIDValues pidValues;
  LibbluStreamPtr elementaryStreams[LIBBLU_MAX_NB_STREAMS];
  LibbluESFormatUtilities elementaryStreamsUtilities[LIBBLU_MAX_NB_STREAMS];

  double currentPcr;           /**< Current Program Clock Reference value,
    in #MAIN_CLOCK_27MHZ ticks.                                              */
  uint64_t currentAt;          /**< Current muxing packets Arrival Timestamp,
    floor(#currentPcr) derivated.                                             */

  struct {
    bool carriedByES;

    uint16_t esCarryingPID;
    bool injectionRequired;  /**< PCR injection is required on next TS
    packet with settings specified PID. Only used if PCR is carried on ES
    TS packets                                                               */
  } pcrParam;

  /* Timing heaps: */
  StreamHeapPtr systemStreamsHeap;      /**< System streams timing heap.    */
  StreamHeapPtr elementaryStreamsHeap;  /**< ES timing heap.                */

  /* System packets: */
  PatParameters patParam;
  PmtParameters pmtParam;
  SitParameters sitParam;

  LibbluStreamPtr pat;
  LibbluStreamPtr pmt;
  LibbluStreamPtr sit;
  LibbluStreamPtr pcr;
  LibbluStreamPtr null;

  /* Used for computation parameters: */
  double bytePcrDuration;
  /* In MAIN_CLOCK_27MHZ ticks (= MAIN_CLOCK_27MHZ * 8 / MuxingSettings.muxRate). */
  double tpPcrDuration;
  /* TS packet duration, In MAIN_CLOCK_27MHZ ticks (= 188 * bytePcrDuration). */
  uint64_t tpPcrIncrementation;  /**< floor(tpPcrDuration) */
  uint64_t referentialPcr; /* PCR referential value in #MAIN_CLOCK_27MHZ ticks. */
  /* uint64_t initialPcr; */
  uint64_t stdBufDelay;  /**< Initial STD buffering delay.
    Initial muxing PCR value can be retrived with formula 'referentialPcr' - 'stdBufDelay' */

  /* Progression */
  unsigned nbTsPacketsMuxed;  /**< Number of transport packets muxed.        */
  size_t nbBytesWritten;
  double progress;            /**< Progression state between 0 and 1.        */

  BufModelNode tStdModel;
  BufModelBuffersListPtr tStdSystemBuffersList;
} LibbluMuxingContext, *LibbluMuxingContextPtr;

/** \~english
 * \brief Create and initiate a muxer context.
 *
 * \param settings Multiplex settings.
 * \return LibbluMuxingContextPtr Upon successfull allocation and
 * initialization, a pointer to the created object is returned. Otherwise,
 * a NULL pointer is returned.
 *
 * This function will create every stream associated to each Elementary Stream
 * (described by supplied settings structure) and every required system stream.
 * If not disabled, a MPEG-TS T-STD / BDAV-STD buffering model is created and
 * initialized.
 * Scripts (ESMS) associated to each ES are parsed. If a ES does not have a
 * suitable script, this last one is generated.
 */
LibbluMuxingContextPtr createLibbluMuxingContext(
  LibbluMuxingSettings settings
);

void destroyLibbluMuxingContext(
  LibbluMuxingContextPtr ctx
);

/** \~english
 * \brief Return the number of registered Elementary Streams in the supplied
 * muxer context.
 *
 * \param ctx Muxer working context.
 * \return unsigned The number of Elementary Streams used. In normal operation,
 * this value is between 1 (contexts with zero ES are not forbidden but lead to
 * instant completion and shall not considered as a normal operation state)
 * and #LIBBLU_MAX_NB_STREAMS.
 */
static inline unsigned nbESLibbluMuxingContext(
  const LibbluMuxingContextPtr ctx
)
{
  return ctx->settings.nbInputStreams;
}

/** \~english
 * \brief Return if the MPEG-TS T-STD / BDAV-STD buffering model is in use.
 *
 * \param ctx Muxer working context.
 * \return true The buffering model is active and must be used to monitor
 * buffering accuracy and compliance of generated transport stream.
 * \return false No buffering model is currently in use. Multiplexing is done
 * regardless of buffering considerations.
 */
static inline bool isEnabledTStdModelLibbluMuxingContext(
  LibbluMuxingContextPtr ctx
)
{
  return !BUF_MODEL_NODE_IS_VOID(ctx->tStdModel);
}

static inline bool pcrInjectionRequired(
  LibbluMuxingContextPtr ctx,
  uint16_t pid
)
{
  return
    ctx->pcrParam.carriedByES
    && ctx->pcrParam.injectionRequired
    && ctx->pcrParam.esCarryingPID == pid
  ;
}

/** \~english
 * \brief Return true if there is remaining Elementary Stream transport packets
 * to mux.
 *
 * \param ctx Muxer working context.
 * \return true ES data remaining to mux.
 * \return false Processing of every ES data has been completed, muxing can be
 * completed.
 */
static inline bool dataRemainingLibbluMuxingContext(
  LibbluMuxingContextPtr ctx
)
{
  return (0 < ctx->elementaryStreamsHeap->usedSize);
}

static inline void updateCurrentPcrLibbluMuxingContext(
  LibbluMuxingContextPtr ctx,
  const double value
)
{
  ctx->currentPcr = value;
  ctx->currentAt = (uint64_t) MAX(0, value);
}

static inline void increaseCurrentPcrLibbluMuxingContext(
  LibbluMuxingContextPtr ctx
)
{
  updateCurrentPcrLibbluMuxingContext(
    ctx,
    ctx->currentPcr + ctx->tpPcrDuration
  );
}

static inline int retriveAssociatedESUtilities(
  const LibbluMuxingContextPtr ctx,
  LibbluStreamPtr stream,
  LibbluESFormatUtilities * utils
)
{
  unsigned i;

  for (i = 0; i < nbESLibbluMuxingContext(ctx); i++) {
    if (ctx->elementaryStreams[i] == stream) {
      if (NULL != utils)
        *utils = ctx->elementaryStreamsUtilities[i];
      return 0;
    }
  }

  return -1;
}

/** \~english
 * \brief Write on output the next transport packet.
 *
 * \param ctx Muxer context.
 * \param output Destination output file.
 * \return int Upon success, a zero value is returned. Otherwise, a negative
 * value is returned.
 *
 * In order, tries to mux a system packet in priority if required (meaning
 * a certain amount of time elapse since last system packet injection),
 * otherwise, a Elementary Stream packet is injected. If buffering model is
 * enabled, injection of an Elementary Stream packet can cause overflow. In
 * that situation, a NULL packet is used to maintain CBR or nothing for VBR,
 * and is not considered as an error.
 */
int muxNextPacketLibbluMuxingContext(
  LibbluMuxingContextPtr ctx,
  BitstreamWriterPtr output
);

/** \~english
 * \brief Adds NULL packets in order to pad generated transport stream to
 * BDAV "Aligned units" boundaries.
 *
 * \param ctx Muxer context.
 * \param output Destination output file.
 * \return int Upon success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int padAlignedUnitLibbluMuxingContext(
  LibbluMuxingContextPtr ctx,
  BitstreamWriterPtr output
);

#endif