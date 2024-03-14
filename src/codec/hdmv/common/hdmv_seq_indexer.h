

#ifndef __LIBBLU_MUXER__CODECS__HDMV__COMMON__SEQ_INDEXER_H__
#define __LIBBLU_MUXER__CODECS__HDMV__COMMON__SEQ_INDEXER_H__

#include "hdmv_error.h"
#include "hdmv_common.h"

#include "../../../util/hashTables.h"

typedef Hashtable HdmvSeqIndexer;

void initHdmvSeqIndexer(
  HdmvSeqIndexer *dst
);

static inline void cleanHdmvSeqIndexer(
  HdmvSeqIndexer seq_indexer
)
{
  cleanHashTable(seq_indexer);
}

HdmvSequencePtr getHdmvSeqIndexer(
  HdmvSeqIndexer *seq_indexer,
  uint32_t id
);

int putHdmvSeqIndexer(
  HdmvSeqIndexer *seq_indexer,
  uint32_t id,
  HdmvSequencePtr sequence
);

int putODSSeqIndexer(
  HdmvSeqIndexer *seq_indexer,
  uint16_t object_id,
  HdmvSequencePtr sequence
);

int getODSSeqIndexer(
  HdmvSeqIndexer *indexer,
  uint16_t object_id,
  HdmvSequencePtr *seq_ret
);

#endif