

#ifndef __LIBBLU_MUXER__CODECS__PGS_GENERATOR__FRAME_H__
#define __LIBBLU_MUXER__CODECS__PGS_GENERATOR__FRAME_H__

#include "../common/hdmv_bitmap.h"
#include "../common/hdmv_object.h"
#include "../common/hdmv_data.h"

#include "pgs_merging_tree.h"

struct PgsObject_struct;

typedef struct {
  uint16_t obj_id_ref;
  uint8_t  win_id_ref;

  uint16_t x;
  uint16_t y;
  struct {
    uint16_t offset_x;
    uint16_t offset_y;
    uint16_t width;
    uint16_t height;
  } cropping;

  // bool not_in_DS;
} PgsCompositionObject;

#define PGS_ASS_MAX_OBJECTS_PER_FRAME  8

typedef struct PF {
  struct PF * prev;
  struct PF * next;
  int64_t timestamp;

  HdmvBitmap bitmaps[HDMV_MAX_NB_PC_COMPO_OBJ];
  HdmvRectangle bitmaps_pos[HDMV_MAX_NB_PC_COMPO_OBJ];
  unsigned nb_bitmaps;

  PgsCompositionObject compo_obj[HDMV_MAX_NB_PC_COMPO_OBJ];
  unsigned nb_compo_obj;

  uint16_t obj_ids[PGS_ASS_MAX_OBJECTS_PER_FRAME];
  unsigned nb_obj;

  int64_t init_duration;
  int64_t min_drawing_duration;
  int64_t min_obj_decode_duration;
  int64_t decode_duration;

  bool non_acquisition_point;
} PgsFrame;

PgsFrame * createPgsFrame(
  void
);

static inline void destroyPgsFrame(
  PgsFrame * frame
)
{
  if (NULL == frame)
    return;
  destroyPgsFrame(frame->next);
  free(frame);
}

typedef struct {
  HdmvObject object;

  PgsFrame ** references;
  unsigned nb_alloc_references;
  unsigned nb_used_references;
} PgsObjectVersion;

typedef struct {
  PgsObjectVersion * versions;
  unsigned nb_allocated_versions;
  unsigned nb_used_versions;

  uint16_t width;
  uint16_t height;
  int32_t  decode_duration;
} PgsObjectVersionList;

typedef struct FS {
  struct FS * prev_seq;
  struct FS * next_seq;
  PgsFrame * first_frm;
  PgsFrame * last_frm;
  unsigned nb_frames;

  HdmvRectangle windows[HDMV_MAX_NB_WDS_WINDOWS];
  unsigned nb_windows;

  PgsObjectVersionList * objects;
  unsigned nb_allocated_objects;
  unsigned nb_used_objects;

  uint32_t dec_obj_buffer_usage;  /**< Decoded Object Buffer occupancy in
    bytes.                                                                   */
  int64_t min_drawing_duration;   /**< Minimal amount of time required to
    fill all of the Epoch windows.                                           */
} PgsFrameSequence;

PgsFrameSequence * createPgsFrameSequence(
  void
);

void destroyPgsFrameSequence(
  PgsFrameSequence * seq
);

int startPgsFrameSequence(
  PgsFrameSequence ** cur_frame_seq
);

PgsFrame * newFramePgsFrameSequence(
  PgsFrameSequence * sequence,
  int64_t timestamp
);

#endif