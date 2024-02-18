#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include "pgs_frame.h"

PgsFrame * createPgsFrame(
  void
)
{
  return calloc(1, sizeof(PgsFrame));
}

PgsFrameSequence * createPgsFrameSequence(
  void
)
{
  return calloc(1, sizeof(PgsFrameSequence));
}

void destroyPgsFrameSequence(
  PgsFrameSequence * seq
)
{
  if (NULL == seq)
    return;
  destroyPgsFrameSequence(seq->next_seq);
  destroyPgsFrame(seq->first_frm);
  free(seq);
}

int startPgsFrameSequence(
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

PgsFrame * newFramePgsFrameSequence(
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