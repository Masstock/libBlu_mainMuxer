#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "hdmv_timecodes.h"

int copyHdmvTimecodes(
  HdmvTimecodes * dst,
  HdmvTimecodes tm
)
{
  int64_t * array = NULL;

  assert(NULL != dst);

  if (0 < tm.usedValues) {
    /* Copy array */
    if (NULL == (array = (int64_t *) malloc(tm.allocatedValues * sizeof(int64_t))))
      LIBBLU_HDMV_TC_ERROR_RETURN("Memory allocation error.\n");
    memcpy(array, tm.values, tm.usedValues * sizeof(int64_t));
  }

  *dst = (HdmvTimecodes) {
    .values = array,
    .allocatedValues = tm.allocatedValues,
    .usedValues = tm.usedValues,
    .readedValues = tm.readedValues
  };
  return 0;
}

int _incrementAllocationHdmvTimecodes(
  HdmvTimecodes * tm
)
{
  size_t newSize;
  int64_t * newArray;

  newSize = GROW_ALLOCATION(
    tm->allocatedValues,
    HDMV_TIMECODES_DEFAULT_NB_VALUES
  );

  if (lb_mul_overflow(newSize, sizeof(int64_t)))
    LIBBLU_HDMV_TC_ERROR_RETURN("Too many timecode values, overflow.\n");

  newArray = (int64_t *) realloc(tm->values, newSize * sizeof(int64_t));
  if (NULL == newArray)
    LIBBLU_HDMV_TC_ERROR_RETURN("Memory allocation error.\n");

  tm->values = newArray;
  tm->allocatedValues = newSize;

  return 0;
}

int addHdmvTimecodes(
  HdmvTimecodes * tm,
  int64_t value
)
{
  if (tm->allocatedValues <= tm->usedValues) {
    if (_incrementAllocationHdmvTimecodes(tm) < 0)
      return -1;
  }

  tm->values[tm->usedValues++] = value;
  return 0;
}