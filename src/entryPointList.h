

#ifndef __LIBBLU_MUXER__ENTRY_POINT_LIST_H__
#define __LIBBLU_MUXER__ENTRY_POINT_LIST_H__

#include "util.h"
#include "stream.h"

typedef enum {
  LIBBLU_EPL__VIDEO           = 1,
  LIBBLU_EPL__PRIMARY_AUDIO   = 3,
  LIBBLU_EPL__SECONDARY_AUDIO = 4,
  LIBBLU_EPL__PG              = 6,
  LIBBLU_EPL__IG              = 7
} LibbluEntryPointListStreamType;

typedef struct {
  uint64_t presentation_time_stamp:33;
  uint64_t is_angle_change:1;
  uint32_t source_packet_number;
  uint32_t end_packet_number;
} LibbluEntryPoint;

typedef struct {
  LibbluEntryPointListStreamType stream_type;
  LibbluStreamPtr linked_stream_ptr;

  LibbluEntryPoint entry_points;
  size_t entry_points_allocated_size;
  size_t entry_points_used_size;
} LibbluEntryPointListStream;

typedef struct {
  LibbluEntryPointListStream *tracked_streams;
  unsigned nb_tracked_streams;
} LibbluEntryPointList;

static inline void cleanLibbluEntryPointList(
  LibbluEntryPointList list
)
{
  sizeof(LibbluEntryPoint);
}

#endif