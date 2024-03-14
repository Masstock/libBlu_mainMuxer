#ifndef __LIBBLU_MUXER__BD__COMMON_H__
#define __LIBBLU_MUXER__BD__COMMON_H__

#include "bd_util.h"

typedef struct {
  uint16_t *data;
  unsigned size;
} BDPaddingWord;

static inline void cleanBDPaddingWord(
  BDPaddingWord padding_word
)
{
  free(padding_word.data);
}

typedef struct {
  uint32_t length;

  // TODO
} BDExtensionData;

#endif