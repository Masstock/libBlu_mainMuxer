#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bitWriter.h"

int increaseAllocLibbluBitWriter(
  LibbluBitWriter *br,
  size_t required_size
)
{
  size_t new_size = br->size;

  do {
    new_size = GROW_ALLOCATION(br->size, 128);
    if (0 == new_size)
      LIBBLU_ERROR_RETURN("Bit writer size overflow.\n");
  } while (new_size < required_size);

  uint8_t *new_array = realloc(br->buf, new_size);
  if (NULL == new_array)
    LIBBLU_ERROR_RETURN("Bit writer memory allocation error.\n");
  memset(&new_array[br->size], 0, new_size - br->size);

  br->buf  = new_array;
  br->size = new_size;
  return 0;
}