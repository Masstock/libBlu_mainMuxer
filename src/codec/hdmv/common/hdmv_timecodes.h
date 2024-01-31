/** \~english
 * \file hdmv_timecodes.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief HDMV streams construction presentation timecodes FIFO module.
 */

#ifndef __LIBBLU_MUXER__CODECS__HDMV__TIMECODES_H__
#define __LIBBLU_MUXER__CODECS__HDMV__TIMECODES_H__

#include "../../../util.h"
#include "hdmv_error.h"

/** \~english
 * \brief Default number of timecode values allocated.
 *
 */
#define HDMV_TIMECODES_DEFAULT_NB_VALUES  64

/** \~english
 * \brief HDMV streams generation timecodes storage structure.
 *
 * Timecode values are in user-defined units. Acts as a FIFO (First-in First-
 * out) storage.
 */
typedef struct {
  int64_t * values;        /**< Timecode values array.                       */
  size_t allocatedValues;  /**< Allocation size of the array.                */
  size_t usedValues;       /**< Number of used (stored) values in array.     */
  size_t readedValues;     /**< Number of stored values readed.              */
} HdmvTimecodes;

static inline void initHdmvTimecodes(
  HdmvTimecodes * dst
)
{
  *dst = (HdmvTimecodes) {
    0
  };
}

/** \~english
 * \brief Release memory allocation done in #HdmvTimecodes structure.
 *
 * \param tm
 */
static inline void cleanHdmvTimecodes(
  HdmvTimecodes tm
)
{
  free(tm.values);
}

/** \~english
 * \brief Copy content and state from a #HdmvTimecodes to another.
 *
 * \param dst Destination structure.
 * \param tm Original structure.
 * \return int Upon successfull completion, a zero is returned. Otherwise, if
 * an error happen (memory allocation), a negative value is returned.
 */
int copyHdmvTimecodes(
  HdmvTimecodes * dst,
  HdmvTimecodes tm
);

/** \~english
 * \brief Adds supplied timecode value to the FIFO.
 *
 * \param tm Timecodes FIFO.
 * \param value Timecode value to store.
 * \return int Upon successfull completion, a zero is returned. Otherwise, if
 * an error happen (memory allocation), a negative value is returned.
 */
int addHdmvTimecodes(
  HdmvTimecodes * tm,
  int64_t value
);

/** \~english
 * \brief Get oldest non-already readed timecode value.
 *
 * \param tm Timecodes FIFO.
 * \param value Timecode value return pointer.
 * \return int Upon successfull completion, a zero is returned. Otherwise, if
 * an error happen (out of range), a negative value is returned.
 */
static inline int getHdmvTimecodes(
  HdmvTimecodes * tm,
  int64_t * pres_time_ret
)
{
  assert(NULL != tm);
  assert(NULL != pres_time_ret);

  if (tm->readedValues == tm->usedValues)
    LIBBLU_HDMV_TC_ERROR_RETURN("No timecode to extract.\n");

  *pres_time_ret = tm->values[tm->readedValues++];
  return 0;
}

#endif