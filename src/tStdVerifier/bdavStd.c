#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include "bdavStd.h"

static bool _matchPidSwitchFilterDecisionListBdavStd(
  BufModelFilterLbl label,
  uint16_t pid
)
{
  unsigned i;

  assert(BUF_MODEL_FILTER_LABEL_TYPE_NUMERIC == label.value.list_item_type);

  for (i = 0; i < label.value.list_length; i++) {
    if (label.value.list[i].number == pid)
      return true;
  }

  return false;
}

int pidSwitchFilterDecisionFunBdavStd(
  BufModelFilterPtr filter,
  unsigned *idx,
  void *streamPtr
)
{
  unsigned i, voidBuf;
  bool voidBufDef;
  uint16_t pid;

  assert(NULL != filter);
  assert(NULL != streamPtr);

  pid = ((LibbluStreamPtr) streamPtr)->pid;

  voidBufDef = false;
  for (i = 0; i < filter->nb_used_nodes; i++) {
    switch (filter->labels[i].type) {
    case BUF_MODEL_FILTER_LABEL_TYPE_LIST:
      if (_matchPidSwitchFilterDecisionListBdavStd(filter->labels[i], pid)) {
        *idx = i;
        return 0;
      }
      break;

    case BUF_MODEL_FILTER_LABEL_TYPE_NUMERIC:
      if (filter->labels[i].value.number == pid) {
        *idx = i;
        return 0;
      }

      if (filter->labels[i].value.number == -1)
        voidBuf = i, voidBufDef = true;
      break;

    default:
      LIBBLU_ERROR_RETURN(
        "Unexpected PID filter label type '%s'.\n",
        getBufModelFilterLblTypeString(filter->labels[i].type)
      );
    }
  }

  /* Unable to find a appropriate route for PID, push in void */
  if (voidBufDef) {
    LIBBLU_INFO(
      "No defined route for stream PID 0x%04" PRIX16 " in buffering model.\n",
      pid
    );

    *idx = voidBuf;
    return 0;
  }

  LIBBLU_ERROR_RETURN(
    "Unable to find a output route for PID=0x%04" PRIX16 " "
    "nor use a void route.",
    pid
  );
}

int createBdavStd(BufModelNode *rootNode)
{
  assert(NULL != rootNode);

  if (
    createBufModelFilterNode(
      rootNode,
      pidSwitchFilterDecisionFunBdavStd,
      BUF_MODEL_FILTER_LABEL_TYPE_NUMERIC
    ) < 0
  )
    return -1;

  /* Add default root (void buffer) to discard untracked PIDs */
  return addNodeToBufModelFilterNode(
    *rootNode,
    newVoidBufModelNode(),
    BUF_MODEL_FILTER_NUMERIC_LABEL(-1)
  );
}

int addSystemToBdavStd(
  BufModelBuffersListPtr bufList,
  BufModelNode rootNode,
  uint64_t initialTimestamp,
  uint64_t transportRate
)
{
  /**
   * TODO: Patch to remove static supported PIDs list, use standard defined
   * conditions (according to PAT parameters).
   */
  int ret;

  BufModelNode branch;
  BufModelFilterLbl label;

  static const unsigned systemPidsNb = BDAV_STD_SYSTEM_STREAMS_NB_PIDS;
  static const int systemPids[] = BDAV_STD_SYSTEM_STREAMS_PIDS;

  ret = createSystemBufferingChainBdavStd(
    bufList, &branch, initialTimestamp, transportRate
  );
  if (ret < 0)
    return -1;

  ret = setBufModelFilterLblList(
    &label,
    BUF_MODEL_FILTER_LABEL_TYPE_NUMERIC,
    systemPids,
    systemPidsNb
  );
  if (ret < 0)
    return -1;

  return addNodeToBufModelFilterNode(
    rootNode,
    branch,
    label
  );
}

int addESToBdavStd(
  BufModelNode rootNode,
  LibbluES *es,
  uint16_t pid,
  uint64_t initialTimestamp
)
{
  int ret;

  BufModelBuffersListPtr strmBufList;
  BufModelNode streamNode;

  if (!isFilterBufModelNode(rootNode))
    LIBBLU_ERROR_RETURN("Expect PID filter as T-STD model root node.\n");

  if (NULL == (strmBufList = createBufModelBuffersList()))
    return -1;

  switch (es->prop.coding_type) {
  case STREAM_CODING_TYPE_AVC:
    ret = createH264BufferingChainBdavStd(
      &streamNode, es, initialTimestamp, strmBufList
    );
    break;

  case STREAM_CODING_TYPE_DTS:
  case STREAM_CODING_TYPE_HDHR:
  case STREAM_CODING_TYPE_HDMA:
  case STREAM_CODING_TYPE_DTSE_SEC:
    ret = createDtsBufferingChainBdavStd(
      &streamNode, es, initialTimestamp, strmBufList
    );
    break;

  case STREAM_CODING_TYPE_AC3:
    ret = createAc3BufferingChainBdavStd(
      &streamNode, es, initialTimestamp, strmBufList
    );
    break;

  case STREAM_CODING_TYPE_LPCM:
    ret = createLpcmBufferingChainBdavStd(
      &streamNode, es, initialTimestamp, strmBufList
    );
    break;

  case STREAM_CODING_TYPE_PG:
  case STREAM_CODING_TYPE_IG:
    ret = createHdmvBufferingChainBdavStd(
      &streamNode, es, initialTimestamp, strmBufList
    );
    break;

  default:
    LIBBLU_INFO(
      "BDAV-STD implementation does not support "
      "es coding type 0x%02" PRIX8 ".\n",
      es->prop.coding_type
    );

    destroyBufModelBuffersList(strmBufList);

    ret = 0;
    strmBufList = NULL;
    streamNode = newVoidBufModelNode();
  }

  if (ret < 0)
    return -1;

  es->lnkd_tstd_buf_list = strmBufList;

  return addNodeToBufModelFilterNode(
    rootNode,
    streamNode,
    BUF_MODEL_FILTER_NUMERIC_LABEL(pid)
  );
}

int addSystemFramesToBdavStd(
  BufModelBuffersListPtr dst,
  size_t headerLength,
  size_t payloadLength
)
{
  if (
    addFrameToBufferFromList(
      dst, TRANSPORT_BUFFER, NULL,
      (BufModelBufferFrame) {
        .header_size = headerLength * 8,
        .data_size = payloadLength * 8,
        .output_data_size = 0,
        .do_not_remove = false
      }
    ) < 0
  )
    return -1;

  return addFrameToBufferFromList(
    dst, MAIN_BUFFER, NULL,
    (BufModelBufferFrame) {
      .header_size = 0,
      .data_size = payloadLength * 8,
      .output_data_size = 0,
      .do_not_remove = false
    }
  );
}

#if 0

int addESPesPacketToBdavStd(
  LibbluStreamPtr stream,
  size_t headerLength,
  size_t payloadLength,
  uint64_t referentialStc
)
{
  LibbluES *es;

  uint64_t removal_timestamp, outputTimestamp;
  LibbluESPesPacketProperties curPesPacket;

  assert(NULL != stream);
  assert(isESLibbluStream(stream));
  es = &stream->es;

  if (NULL == es->lnkd_tstd_buf_list)
    return 0; /* No buffering chain to use. */

  curPesPacket = es->curPesPacket.prop;
  if (
    es->prop.coding_type == STREAM_CODING_TYPE_AVC
    && curPesPacket.extension_data_pres
    && 0
  ) {
    removal_timestamp =
      curPesPacket.extension_data.h264.cpb_removal_time
      + referentialStc
    ;
    outputTimestamp =
      curPesPacket.extension_data.h264.dpb_output_time
      + referentialStc
    ;
  }
  else {
    removal_timestamp =
      curPesPacket.dts
    ;
    outputTimestamp =
      curPesPacket.pts
    ;
  }

  LIBBLU_T_STD_VERIF_DECL_DEBUG(
    "Registering %zu+%zu=%zu bytes of PES for PID 0x%04" PRIX16 ", "
    "Remove: %" PRIu64 ", Output: %" PRIu64 ".\n",
    headerLength, payloadLength, headerLength + payloadLength, stream->pid,
    removal_timestamp, outputTimestamp
  );

  if (es->prop.type == ES_VIDEO) {
    if (
      addFrameToBufferFromList(
        es->lnkd_tstd_buf_list, MULTIPLEX_BUFFER, NULL,
        (BufModelBufferFrame) {
          .header_size = headerLength * 8,
          .data_size = payloadLength * 8,
          .removal_timestamp = removal_timestamp,
          .output_data_size = 0,
          .do_not_remove = false
        }
      ) < 0
    )
      return -1;

    return addFrameToBufferFromList(
      es->lnkd_tstd_buf_list, ELEMENTARY_BUFFER, NULL,
      (BufModelBufferFrame) {
        .header_size = 0,
        .data_size = payloadLength * 8,
        .removal_timestamp = outputTimestamp,
        .output_data_size = 0,
        .do_not_remove = false
      }
    );
  }

  return addFrameToBufferFromList(
    es->lnkd_tstd_buf_list, MAIN_BUFFER, NULL,
    (BufModelBufferFrame) {
      .header_size = headerLength * 8,
      .data_size = payloadLength * 8,
      .removal_timestamp = removal_timestamp,
      .output_data_size = 0,
      .do_not_remove = false
    }
  );
}

#endif

int addESTsFrameToBdavStd(
  BufModelBuffersListPtr dst,
  size_t headerLength,
  size_t payloadLength
)
{
  if (NULL == dst)
    return 0; /* No buffering chain to use. */

  return addFrameToBufferFromList(
    dst, TRANSPORT_BUFFER, NULL,
    (BufModelBufferFrame) {
      .header_size = headerLength * 8,
      .data_size = payloadLength * 8,
      .output_data_size = 0,
      .do_not_remove = false
    }
  );
}