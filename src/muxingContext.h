/** \~english
 * \file muxingContext.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief Muxing context module.
 *
 * \todo Increase speed when T-STD buffer verifier is activated.
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

#include "codecsUtilities.h"
#include "elementaryStream.h"
#include "entryPointList.h"
#include "muxingSettings.h"
#include "packetIdentifier.h"
#include "stream.h"
#include "streamHeap.h"
#include "systemStream.h"
#include "tsPackets.h"
#include "tStdVerifier/bdavStd.h"

#define SHIFT_PACKETS_BEFORE_DTS true
#define USE_AVERAGE_PES_SIZE false

typedef struct {
  const LibbluMuxingSettings *settings_ptr;  /**< Context associated
    settings reference.                                                      */

  LibbluPIDValues PID_values;

  LibbluStreamPtr *elementary_streams;
  LibbluESFormatUtilities *elementary_streams_utilities;
  unsigned nb_elementary_streams;

  double current_STC;       /**< Current System Time Clock value,
    in #MAIN_CLOCK_27MHZ ticks.                                              */
  uint64_t current_STC_TS;  /**< Current System Time Clock Timestamp,
    floor(#current_STC) derivated.                                            */

  struct {
    bool carried_by_ES;

    uint16_t carrying_ES_PID;
    bool injection_required;  /**< PCR injection is required on next TS
    packet with settings specified PID. Only used if PCR is carried on ES
    TS packets                                                               */
  } PCR_param;

  /* Timing heaps: */
  StreamHeapPtr sys_stream_heap;      /**< System streams timing heap.    */
  StreamHeapPtr elm_stream_heap;  /**< ES timing heap.                */

  /* System packets: */
  PatParameters pat_param;
  PmtParameters pmt_param;
  SitParameters sit_param;

  LibbluStreamPtr pat;
  LibbluStreamPtr pmt;
  LibbluStreamPtr sit;
  LibbluStreamPtr pcr;
  LibbluStreamPtr null;

  /* Used for computation parameters: */
  double byte_STC_dur;  /**< In MAIN_CLOCK_27MHZ ticks
    (= MAIN_CLOCK_27MHZ * 8 / MuxingSettings.muxRate).                       */
  double tp_SRC_dur;    /**< TS packet transmission duration,
    (= 188 * byte_STC_dur). It is the time required for one TP to be
    transmitted at mux-rate in MAIN_CLOCK_27MHZ ticks.                       */
  uint64_t tp_STC_dur_floor;  /**< floor(tp_SRC_dur) */
  uint64_t tp_STC_dur_ceil;  /**< floor(tp_SRC_dur) */

  uint64_t ref_STC_timestamp;  /* Referential initial STC value in
    #MAIN_CLOCK_27MHZ ticks.                                                 */
  uint64_t STD_buf_delay;      /**< STD buffering delay, virtual time
    between muxer and demuxer STCs. Initial STC value can be retrived with
    formula 'ref_STC_timestamp' - 'STD_buf_delay'.                           */

  BufModelNode t_STD_model;
  BufModelBuffersListPtr t_STD_sys_buffers_list;

  /* Progression */
  unsigned nb_tp_muxed;     /**< Number of transport packets muxed.          */
  size_t nb_bytes_written;
  double progress;          /**< Progression state between 0 and 1.          */

  /* Results */
  uint64_t STC_start;  /**< Initial STC value (@27MHz).                      */
  uint64_t STC_end;    /**< Final STC value (@27MHz).                        */
  uint64_t PTS_start;  /**< First video Presentation Unit PTS (@27MHz).      */
  uint64_t PTS_end;    /**< Last video Presentation Unit PTS (@27MHz).       */
  uint64_t PU_dur;     /**< Duration of the last video PU (@27MHz).          */

  LibbluEntryPointList entry_points;  /**< Stream entry points.              */
} LibbluMuxingContext;

/** \~english
 * \brief Create and initiate a muxer context.
 *
 * \param settings Multiplex settings.
 * \return LibbluMuxingContext *Upon successfull allocation and
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
int initLibbluMuxingContext(
  LibbluMuxingContext *dst,
  const LibbluMuxingSettings *settings_ptr
);

static inline void cleanLibbluMuxingContext(
  LibbluMuxingContext ctx
)
{
  cleanLibbluPIDValues(ctx.PID_values);
  for (unsigned i = 0; i < ctx.nb_elementary_streams; i++)
    destroyLibbluStream(ctx.elementary_streams[i]);
  free(ctx.elementary_streams);
  free(ctx.elementary_streams_utilities);

  destroyStreamHeap(ctx.sys_stream_heap);
  destroyStreamHeap(ctx.elm_stream_heap);

  cleanPatParameters(ctx.pat_param);
  cleanPmtParameters(ctx.pmt_param);
  cleanSitParameters(ctx.sit_param);

  destroyLibbluStream(ctx.pat);
  destroyLibbluStream(ctx.pmt);
  destroyLibbluStream(ctx.sit);
  destroyLibbluStream(ctx.pcr);
  destroyLibbluStream(ctx.null);

  destroyBufModel(ctx.t_STD_model);
  destroyBufModelBuffersList(ctx.t_STD_sys_buffers_list);
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
  LibbluMuxingContext ctx
)
{
  return (0 < ctx.elm_stream_heap->usedSize);
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
  LibbluMuxingContext *ctx,
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
  LibbluMuxingContext *ctx,
  BitstreamWriterPtr output
);

#endif