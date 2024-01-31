

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

  HdmvBitmap bitmaps[HDMV_MAX_NB_PCS_COMPOS];
  HdmvRectangle bitmaps_pos[HDMV_MAX_NB_PCS_COMPOS];
  unsigned nb_bitmaps;

  PgsCompositionObject compo_obj[HDMV_MAX_NB_PCS_COMPOS];
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

static inline void destroyPgsFrameSequence(
  PgsFrameSequence * seq
)
{
  if (NULL == seq)
    return;
  destroyPgsFrameSequence(seq->next_seq);
  destroyPgsFrame(seq->first_frm);
  free(seq);
}

static inline int startPgsFrameSequence(
  PgsFrameSequence ** cur_frame_seq
)
{
  PgsFrameSequence * new_frame_seq = createPgsFrameSequence();
  if (NULL == new_frame_seq)
    return -1;

  if (NULL == *cur_frame_seq)
    *cur_frame_seq = new_frame_seq;
  else {
    (*cur_frame_seq)->next_seq = new_frame_seq;
    new_frame_seq->prev_seq = *cur_frame_seq; // Only set prev_seq for non-first
  }

  *cur_frame_seq = new_frame_seq;

  return 0;
}

static inline PgsFrame * newFramePgsFrameSequence(
  PgsFrameSequence * sequence,
  int64_t timestamp
)
{
  PgsFrame * frame = createPgsFrame();
  if (NULL == frame)
    return NULL;
  frame->timestamp = timestamp;

  if (NULL == sequence->first_frm)
    sequence->first_frm = frame;
  else
    sequence->last_frm->next = frame;

  frame->prev = sequence->last_frm;
  sequence->last_frm = frame;
  sequence->nb_frames++;

  return frame;
}

#endif