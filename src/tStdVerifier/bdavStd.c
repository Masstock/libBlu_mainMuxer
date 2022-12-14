#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include "bdavStd.h"

int pidSwitchFilterDecisionFunBdavStd(
  BufModelFilterPtr filter,
  unsigned * idx,
  void * streamPtr
)
{
  unsigned i, j, voidBuf;
  bool voidBufDef;

  LibbluStreamPtr stream;

  assert(NULL != filter);
  assert(NULL != streamPtr);

  stream = (LibbluStreamPtr) streamPtr;

  voidBufDef = false;
  for (i = 0; i < filter->nbUsedNodes; i++) {
    switch (filter->labels[i].type) {
      case BUF_MODEL_FILTER_LABEL_TYPE_LIST:
        assert(
          BUF_MODEL_FILTER_LABEL_TYPE_NUMERIC
          == filter->labels[i].value.listItemsType
        );

        for (j = 0; j < filter->labels[i].value.listLength; j++) {
          if (filter->labels[i].value.list[j].number == stream->pid) {
            *idx = i;
            return 0;
          }
        }
        break;

      case BUF_MODEL_FILTER_LABEL_TYPE_NUMERIC:
        if (filter->labels[i].value.number == stream->pid) {
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
      "No defined route for stream PID 0x%04" PRIu16 " in buffering model.\n",
      stream->pid
    );

    *idx = voidBuf;
    return 0;
  }

  LIBBLU_ERROR_RETURN(
    "Unable to find a output route for PID=0x%04" PRIX16 " "
    "nor use a void route.",
    stream->pid
  );
}

int createBdavStd(BufModelNode * rootNode)
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
    BUF_MODEL_NEW_NODE(),
    BUF_MODEL_FILTER_NUMERIC_LABEL(-1)
  );
}

bool isManagedSystemBdavStd(
  uint16_t systemStreamPid,
  PatParameters pat
)
{
  unsigned i;

  if (systemStreamPid <= 0x000F)
    return true;

  for (i = 0; i < pat.usedPrograms; i++) {
    if (pat.programs[i].programNumber == 0x0)
      continue; /* network_pid */

    if (pat.programs[i].programMapPid == systemStreamPid)
      return true;
  }

  return false;
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
  LibbluESPtr es,
  uint16_t pid,
  uint64_t initialTimestamp
)
{
  int ret;

  BufModelBuffersListPtr strmBufList;
  BufModelNode streamNode;

  if (!BUF_MODEL_NODE_IS_FILTER(rootNode))
    LIBBLU_ERROR_RETURN("Expect PID filter as T-STD model root node.\n");

  if (NULL == (strmBufList = createBufModelBuffersList()))
    return -1;

  switch (es->prop.codingType) {
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

#if 1
    case STREAM_CODING_TYPE_LPCM:
      ret = createLpcmBufferingChainBdavStd(
        &streamNode, es, initialTimestamp, strmBufList
      );
      break;
#endif

    default:
      LIBBLU_INFO(
        "BDAV-STD implementation does not support "
        "es coding type 0x%02" PRIX8 ".\n",
        es->prop.codingType
      );

      destroyBufModelBuffersList(strmBufList);

      ret = 0;
      strmBufList = NULL;
      streamNode = BUF_MODEL_NEW_NODE();
  }

  if (ret < 0)
    return -1;

  es->lnkdBufList = strmBufList;

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
        .headerSize = headerLength * 8,
        .dataSize = payloadLength * 8,
        .outputDataSize = 0,
        .doNotRemove = false
      }
    ) < 0
  )
    return -1;

  return addFrameToBufferFromList(
    dst, MAIN_BUFFER, NULL,
    (BufModelBufferFrame) {
      .headerSize = 0,
      .dataSize = payloadLength * 8,
      .outputDataSize = 0,
      .doNotRemove = false
    }
  );
}

int addESPesFrameToBdavStd(
  LibbluStreamPtr stream,
  size_t headerLength,
  size_t payloadLength,
  uint64_t referentialStc
)
{
  LibbluESPtr es;

  uint64_t removalTimestamp, outputTimestamp;
  LibbluESPesPacketProperties curPesPacket;

  assert(NULL != stream);
  assert(isESLibbluStream(stream));
  es = &stream->es;

  if (NULL == es->lnkdBufList)
    return 0; /* No buffering chain to use. */

  curPesPacket = es->curPesPacket.prop;
  if (
    es->prop.codingType == STREAM_CODING_TYPE_AVC
    && curPesPacket.extDataPresent
    && 0
  ) {
    removalTimestamp =
      curPesPacket.extData.h264.cpbRemovalTime
      + referentialStc
    ;
    outputTimestamp =
      curPesPacket.extData.h264.dpbOutputTime
      + referentialStc
    ;
  }
  else {
    removalTimestamp =
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
    removalTimestamp, outputTimestamp
  );

  if (es->prop.type == ES_VIDEO) {
    if (
      addFrameToBufferFromList(
        es->lnkdBufList, MULTIPLEX_BUFFER, NULL,
        (BufModelBufferFrame) {
          .headerSize = headerLength * 8,
          .dataSize = payloadLength * 8,
          .removalTimestamp = removalTimestamp,
          .outputDataSize = 0,
          .doNotRemove = false
        }
      ) < 0
    )
      return -1;

    return addFrameToBufferFromList(
      es->lnkdBufList, ELEMENTARY_BUFFER, NULL,
      (BufModelBufferFrame) {
        .headerSize = 0,
        .dataSize = payloadLength * 8,
        .removalTimestamp = outputTimestamp,
        .outputDataSize = 0,
        .doNotRemove = false
      }
    );
  }

  return addFrameToBufferFromList(
    es->lnkdBufList, MAIN_BUFFER, NULL,
    (BufModelBufferFrame) {
      .headerSize = headerLength * 8,
      .dataSize = payloadLength * 8,
      .removalTimestamp = removalTimestamp,
      .outputDataSize = 0,
      .doNotRemove = false
    }
  );
}

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
      .headerSize = headerLength * 8,
      .dataSize = payloadLength * 8,
      .outputDataSize = 0,
      .doNotRemove = false
    }
  );
}