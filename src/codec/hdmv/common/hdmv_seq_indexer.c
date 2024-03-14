#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#include "hdmv_seq_indexer.h"

uint32_t hashToUint32(
  const void *key
)
{
  return (uint32_t) ((uintptr_t) key);
}

int compKeys(
  const void *left,
  const void *right
)
{
  if (left < right)
    return -1;
  if (left > right)
    return 1;
  return 0;
}

void initHdmvSeqIndexer(
  HdmvSeqIndexer *dst
)
{
  *dst = newNumHashtable();
  setFunHashTable(dst, NULL, NULL, NULL, NULL);
}

HdmvSequencePtr getHdmvSeqIndexer(
  HdmvSeqIndexer *seq_indexer,
  uint32_t id
)
{
  return getNumHashTable(
    seq_indexer,
    id
  );
}

int putHdmvSeqIndexer(
  HdmvSeqIndexer *seq_indexer,
  uint32_t id,
  HdmvSequencePtr sequence
)
{
  return putNumHashTable(
    seq_indexer,
    id,
    sequence
  );
}

int putODSSeqIndexer(
  HdmvSeqIndexer *seq_indexer,
  uint16_t object_id,
  HdmvSequencePtr sequence
)
{
  return putHdmvSeqIndexer(
    seq_indexer,
    object_id,
    sequence
  );
}

int getODSSeqIndexer(
  HdmvSeqIndexer *indexer,
  uint16_t object_id,
  HdmvSequencePtr *seq_ret
)
{
  assert(0xFFFF != object_id);

  HdmvSequencePtr seq = getHdmvSeqIndexer(indexer, object_id);
  if (NULL == seq)
    LIBBLU_HDMV_COM_ERROR_RETURN(
      "Unable to retrieve the ODS with 'object_id' == 0x%04" PRIX16 " "
      "referenced by the composition, all referenced objects should be "
      "present in the Display Set.\n",
      object_id
    );

  *seq_ret = seq;
  return 0;
}