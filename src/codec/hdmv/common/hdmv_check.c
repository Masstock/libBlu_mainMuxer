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
  HdmvStreamType type,
  unsigned curDSIdx
)
{
  hdmv_segtype_idx idx;

  LIBBLU_HDMV_CK_DEBUG("  Checking mandatory segments/sequences presence:\n");
  if (_checkMandatorySequences(ds, type) < 0)
    return -1;

  LIBBLU_HDMV_CK_DEBUG("  Checking segments/sequences:\n");
  for (idx = 0; idx < HDMV_NB_SEGMENT_TYPES; idx++) {
    HdmvSequencePtr seq = getSequenceByIdxHdmvDisplaySet(ds, idx);

    for (; NULL != seq; seq = seq->nextSequence) {
      if (!isFromDisplaySetNbHdmvSequence(seq, curDSIdx))
        continue;

      if (checkHdmvSequence(seq) < 0)
        return -1;
    }
  }

  return 0;
}

/** \~english
 * \brief Coded Object Buffer (EB) size.
 *
 * \return size_t
 */
static size_t _getCodedObjectBufferSize(
  void
)
{
  return 1 << 20;
}

/** \~english
 * \brief Decoded Object Buffer (DB) size.
 *
 * \param type
 * \return size_t
 */
static size_t _getDecodedObjectBufferSize(
  HdmvStreamType type
)
{
  static const size_t sizes[] = {
    16 << 20, /**< Interactive Graphics  - 16MiB  */
    4  << 20, /**< Presentation Graphics -  4MiB  */
  };

  return sizes[type];
}

int checkObjectsBufferingHdmvDisplaySet(
  HdmvDisplaySet * ds,
  HdmvStreamType type
)
{
  HdmvSequencePtr seq = getSequenceByIdxHdmvDisplaySet(ds, HDMV_SEGMENT_TYPE_ODS_IDX);

  size_t codedObjBufferUsage = 0;
  size_t decodedObjBufferUsage = 0;
  size_t codedObjBufferSize;
  size_t decodedObjBufferSize;

  LIBBLU_HDMV_CK_DEBUG("  Checking objects buffer usage:\n");
  LIBBLU_HDMV_CK_DEBUG("     Compressed size => Uncompressed size.\n");
  for (; NULL != seq; seq = seq->nextSequence) {
    HdmvODParameters object_data = seq->data.od;
    size_t objCodedSize = object_data.object_data_length;
    size_t objDecodedSize = object_data.object_width * object_data.object_height;

    LIBBLU_HDMV_CK_DEBUG(
      "   - Object %u: %ux%u, %zu byte(s) => %zu byte(s).\n",
      object_data.object_descriptor.object_id,
      object_data.object_width,
      object_data.object_height,
      objCodedSize,
      objDecodedSize
    );

    codedObjBufferUsage += objCodedSize;
    decodedObjBufferUsage += objDecodedSize;
  }

  codedObjBufferSize = _getCodedObjectBufferSize();
  LIBBLU_HDMV_CK_DEBUG(
    "    => Coded Object Buffer (EB) usage: %zu bytes / %zu bytes.\n",
    codedObjBufferUsage,
    codedObjBufferSize
  );

  if (codedObjBufferSize < codedObjBufferUsage)
    LIBBLU_HDMV_CK_ERROR_RETURN(
      "Coded Object Buffer (EB) overflows, "
      "%zu bytes used out of buffer size of %zu bytes.\n",
      codedObjBufferUsage,
      codedObjBufferSize
    );

  decodedObjBufferSize = _getDecodedObjectBufferSize(type);
  LIBBLU_HDMV_CK_DEBUG(
    "    => Decoded Object Buffer (DB) usage: %zu bytes / %zu bytes.\n",
    decodedObjBufferUsage,
    decodedObjBufferSize
  );

  if (decodedObjBufferSize < decodedObjBufferUsage)
    LIBBLU_HDMV_CK_ERROR_RETURN(
      "Decoded Object Buffer (DB) overflows, "
      "%zu bytes used out of buffer size of %zu bytes.\n",
      decodedObjBufferUsage,
      decodedObjBufferSize
    );

  return 0;
}

int checkDuplicatedHdmvDisplaySet(
  HdmvDisplaySet * ds,
  unsigned lastDSIdx
)
{
  hdmv_segtype_idx idx;

  for (idx = 0; idx < HDMV_NB_SEGMENT_TYPES; idx++) {
    HdmvSequencePtr seq;

    for (
      seq = getSequenceByIdxHdmvDisplaySet(ds, idx);
      NULL != seq;
      seq = seq->nextSequence
    ) {
      if (isFromDisplaySetNbHdmvSequence(seq, lastDSIdx)) {
        /* Presence of a non-refreshed sequence from previous DS. */
        LIBBLU_HDMV_COM_ERROR_RETURN(
          "Missing segments from previous duplicated DS.\n"
        );
      }
    }
  }

  return 0;
}