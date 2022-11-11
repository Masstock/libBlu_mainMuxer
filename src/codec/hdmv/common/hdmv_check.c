#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>

#include "hdmv_check.h"

int checkHdmvSequence(
  HdmvSequencePtr sequence
)
{
  int ret = 0;

  switch (sequence->type) {
    case HDMV_SEGMENT_TYPE_PDS:
    case HDMV_SEGMENT_TYPE_ODS:
    case HDMV_SEGMENT_TYPE_PCS:
    case HDMV_SEGMENT_TYPE_WDS:
    case HDMV_SEGMENT_TYPE_ICS:

    case HDMV_SEGMENT_TYPE_END:
    case HDMV_SEGMENT_TYPE_ERROR:
      break;
  }

  return ret;
}

static int _checkMandatorySequences(
  const HdmvDisplaySet * ds,
  HdmvStreamType type
)
{
#define CHECK_SEQ_PRESENCE(idx)                                               \
  do {                                                                        \
    HdmvSequencePtr seq = getSequenceByIdxHdmvDisplaySet(ds, idx);            \
    if (NULL == seq)                                                          \
      return -1;                                                              \
  } while(0)

  switch (type) {
    case HDMV_STREAM_TYPE_IGS:
      CHECK_SEQ_PRESENCE(HDMV_SEGMENT_TYPE_ICS_IDX);
      break;

    case HDMV_STREAM_TYPE_PGS:
      CHECK_SEQ_PRESENCE(HDMV_SEGMENT_TYPE_PCS_IDX);
      break;
  }

#undef CHECK_SEQ_PRESENCE
  return 0;
}

int checkHdmvDisplaySet(
  HdmvDisplaySet * ds,
  HdmvStreamType type
)
{
  hdmv_segtype_idx idx;

  if (_checkMandatorySequences(ds, type) < 0)
    return -1;

  for (idx = 0; idx < HDMV_NB_SEGMENT_TYPES; idx++) {
    HdmvSequencePtr sequence;

    for (
      sequence = getSequenceByIdxHdmvDisplaySet(ds, idx);
      NULL != sequence;
      sequence = sequence->nextSequence
    ) {
      if (checkHdmvSequence(sequence) < 0)
        return -1;
    }
  }

  return 0;
}

int checkDuplicatedHdmvDisplaySet(
  HdmvDisplaySet * ds,
  unsigned lastDSIdx
)
{
  hdmv_segtype_idx idx;

  for (idx = 0; idx < HDMV_NB_SEGMENT_TYPES; idx++) {
    HdmvSequencePtr sequence;

    for (
      sequence = getSequenceByIdxHdmvDisplaySet(ds, idx);
      NULL != sequence;
      sequence = sequence->nextSequence
    ) {
      if (isFromDisplaySetNbHdmvSequence(sequence, lastDSIdx)) {
        /* Presence of a non-refreshed sequence from previous DS. */
        LIBBLU_HDMV_COM_ERROR_RETURN(
          "Missing segments from previous duplicated DS.\n"
        );
      }
    }
  }

  return 0;
}