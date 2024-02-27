#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>

#include "hdmv_common.h"

/* ### HDMV Sequence : ##################################################### */

int parseFragmentHdmvSequence(
  HdmvSequencePtr dst,
  BitstreamReaderPtr input,
  uint16_t fragment_length
)
{

  if (lb_u32add_overflow(dst->fragments_used_len, fragment_length))
    LIBBLU_HDMV_COM_ERROR_RETURN(
      "Reconstructed sequence data from segment fragments is too large.\n"
    );

  if (dst->fragments_allocated_len < dst->fragments_used_len + fragment_length) {
    /* Need sequence data size increasing */
    uint32_t new_size = dst->fragments_used_len + fragment_length;

    uint8_t * new_array = (uint8_t *) realloc(dst->fragments, new_size);
    if (NULL == new_array)
      LIBBLU_HDMV_COM_ERROR_RETURN("Memory allocation error.\n");

    dst->fragments = new_array;
    dst->fragments_allocated_len = new_size;
  }

  LIBBLU_HDMV_PARSER_DEBUG(
    "  size: %" PRIu16 " byte(s).\n",
    fragment_length
  );

  if (readBytes(input, &dst->fragments[dst->fragments_used_len], fragment_length) < 0)
    return -1;
  dst->fragments_used_len += fragment_length;

  return 0;
}

/* ### HDMV Display Set : ################################################## */

HdmvSequencePtr getFirstSequenceByIdxHdmvDSState(
  const HdmvDSState * ds_state,
  hdmv_segtype_idx idx
)
{
  assert(NULL != ds_state);
  return ds_state->sequences[idx].ds_head;
}

static HdmvSequencePtr _getDSLastSequenceByIdxHdmvDSState(
  const HdmvDSState * ds_state,
  hdmv_segtype_idx idx
)
{
  return ds_state->sequences[idx].ds_last;
}

HdmvSequencePtr getICSPCSSequenceHdmvDSState(
  const HdmvDSState * ds_state
)
{
  HdmvSequencePtr seq;

  seq = getFirstSequenceByIdxHdmvDSState(ds_state, HDMV_SEGMENT_TYPE_ICS_IDX);
  if (NULL != seq) {
    assert(isUniqueInDisplaySetHdmvSequence(seq));
    return seq;
  }

  seq = getFirstSequenceByIdxHdmvDSState(ds_state, HDMV_SEGMENT_TYPE_PCS_IDX);
  if (NULL != seq) {
    assert(isUniqueInDisplaySetHdmvSequence(seq));
    return seq;
  }

  LIBBLU_HDMV_COM_ERROR_NRETURN("Internal error, unexpected missing ICS/PCS segment.\n");
}

HdmvSequencePtr getICSSequenceHdmvDSState(
  const HdmvDSState * ds_state
)
{
  HdmvSequencePtr seq = getFirstSequenceByIdxHdmvDSState(ds_state, HDMV_SEGMENT_TYPE_ICS_IDX);
  if (NULL != seq) {
    assert(isUniqueInDisplaySetHdmvSequence(seq));
    return seq;
  }

  LIBBLU_HDMV_COM_ERROR_NRETURN("Internal error, unexpected missing ICS segment.\n");
}

HdmvSequencePtr getPCSSequenceHdmvDSState(
  const HdmvDSState * ds_state
)
{
  HdmvSequencePtr seq = getFirstSequenceByIdxHdmvDSState(ds_state, HDMV_SEGMENT_TYPE_PCS_IDX);
  if (NULL != seq) {
    assert(isUniqueInDisplaySetHdmvSequence(seq));
    return seq;
  }

  LIBBLU_HDMV_COM_ERROR_NRETURN("Internal error, unexpected missing PCS segment.\n");
}

HdmvSequencePtr getWDSSequenceHdmvDSState(
  const HdmvDSState * ds_state
)
{
  HdmvSequencePtr seq = getFirstSequenceByIdxHdmvDSState(ds_state, HDMV_SEGMENT_TYPE_WDS_IDX);
  assert(atMostOneInDisplaySetHdmvSequence(seq));
  return seq;
}

HdmvSequencePtr getFirstPDSSequenceHdmvDSState(
  const HdmvDSState * ds_state
)
{
  return getFirstSequenceByIdxHdmvDSState(ds_state, HDMV_SEGMENT_TYPE_PDS_IDX);
}

HdmvSequencePtr getLastPDSSequenceHdmvDSState(
  const HdmvDSState * ds_state
)
{
  return _getDSLastSequenceByIdxHdmvDSState(ds_state, HDMV_SEGMENT_TYPE_PDS_IDX);
}

HdmvSequencePtr getFirstODSSequenceHdmvDSState(
  const HdmvDSState * ds_state
)
{
  return getFirstSequenceByIdxHdmvDSState(ds_state, HDMV_SEGMENT_TYPE_ODS_IDX);
}

HdmvSequencePtr getLastODSSequenceHdmvDSState(
  const HdmvDSState * ds_state
)
{
  return _getDSLastSequenceByIdxHdmvDSState(ds_state, HDMV_SEGMENT_TYPE_ODS_IDX);
}

HdmvSequencePtr getENDSequenceHdmvDSState(
  const HdmvDSState * ds_state
)
{
  HdmvSequencePtr seq = getFirstSequenceByIdxHdmvDSState(ds_state, HDMV_SEGMENT_TYPE_END_IDX);
  if (NULL != seq) {
    assert(isUniqueInDisplaySetHdmvSequence(seq));
    return seq;
  }

  LIBBLU_HDMV_COM_ERROR_NRETURN("Internal error, unexpected missing END segment.\n");
}

/* ### HDMV Epoch memory state : ########################################### */

bool fetchPaletteHdmvEpochState(
  const HdmvEpochState * epoch_state,
  uint8_t palette_id,
  bool must_be_in_current_DS,
  HdmvPDSegmentParameters * pd_ret,
  bool * has_been_updated_ret
)
{
  assert(UINT8_MAX <= HDMV_MAX_NB_PAL); // Avoid type limited range warning
  HdmvEpochStatePalette pal = epoch_state->palettes[palette_id];
  if (HDMV_DEF_NEVER_PROVIDED == pal.state)
    return false; // Never transmitted
  if (HDMV_DEF_NON_REFRESHED == pal.state && must_be_in_current_DS)
    return false; // Not refreshed

  if (NULL != pd_ret)
    *pd_ret = pal.def;
  if (NULL != has_been_updated_ret)
    *has_been_updated_ret = (HDMV_DEF_JUST_UPDATED == pal.state);
  return true;
}

void setPaletteHdmvEpochState(
  HdmvEpochState * epoch_state,
  const HdmvPDSegmentParameters pal_def
)
{
  HdmvEpochStatePalette * dst_pal = &epoch_state->palettes[
    pal_def.palette_descriptor.palette_id
  ];

  if (HDMV_DEF_NEVER_PROVIDED == dst_pal->state) {
    dst_pal->def = pal_def; // Initialization, copy whole definition
    dst_pal->state = HDMV_DEF_JUST_PROVIDED;
  }
  else {
    bool is_update = (
      dst_pal->def.palette_descriptor.palette_version_number
      != pal_def.palette_descriptor.palette_version_number
    );

    if (is_update) {
      /* Update only supplied values */
      dst_pal->def.palette_descriptor = pal_def.palette_descriptor;
      for (unsigned i = 0; i < HDMV_NB_PAL_ENTRIES; i++) {
        HdmvPaletteEntryParameters entry = pal_def.palette_entries[i];
        if (entry.updated)
          dst_pal->def.palette_entries[i] = entry;
      }
      dst_pal->state = HDMV_DEF_JUST_UPDATED;
    }
    else
      dst_pal->state = HDMV_DEF_JUST_PROVIDED;
  }

  epoch_state->last_PDS[pal_def.palette_descriptor.palette_id] = pal_def;
}

void setObjectHdmvEpochState(
  HdmvEpochState * epoch_state,
  const HdmvODParameters obj_def
)
{
  HdmvEpochStateObject * dst_obj = &epoch_state->objects[
    obj_def.object_descriptor.object_id
  ];

  if (HDMV_DEF_NEVER_PROVIDED == dst_obj->state) {
    dst_obj->def   = obj_def; // Initialization, copy whole definition
    dst_obj->state = HDMV_DEF_JUST_PROVIDED;
  }
  else {
    bool is_update = (
      dst_obj->def.object_descriptor.object_version_number
      != obj_def.object_descriptor.object_version_number
    );

    if (is_update) {
      dst_obj->def   = obj_def;
      dst_obj->state = HDMV_DEF_JUST_UPDATED;
    }
    else
      dst_obj->state = HDMV_DEF_JUST_PROVIDED;
  }
}

void setActivePCHdmvEpochState(
  HdmvEpochState * epoch_state,
  HdmvPCParameters pc,
  int64_t pc_pts
)
{
  epoch_state->cur_active_pc = (epoch_state->cur_active_pc + 1) % HDMV_MAX_NB_ACTIVE_PC;
  epoch_state->nb_active_pc  = MIN(epoch_state->nb_active_pc + 1, HDMV_MAX_NB_ACTIVE_PC);

  epoch_state->active_pc[epoch_state->cur_active_pc]     = pc;
  epoch_state->active_pc_pts[epoch_state->cur_active_pc] = pc_pts;
}

void setWDHdmvEpochState(
  HdmvEpochState * epoch_state,
  const HdmvWDParameters wd
)
{
  epoch_state->window_def = wd;
  epoch_state->window_def_present  = true;
  epoch_state->window_def_provided = true;
}

int setActiveICHdmvEpochState(
  HdmvEpochState * epoch_state,
  HdmvICParameters ic,
  int64_t ic_pts
)
{
  if (!epoch_state->active_ic_present) {
    if (copyHdmvICParameters(&epoch_state->active_ic, ic) < 0)
      return -1;
  }
  else {
    /* Update current active IC */
    HdmvICParameters * dst = &epoch_state->active_ic;
    assert(dst->number_of_pages == ic.number_of_pages);
    for (uint8_t i = 0; i < ic.number_of_pages; i++) {
      if (ic.pages[i].page_version_number != dst->pages[i].page_version_number) {
        cleanHdmvPageParameters(dst->pages[i]);
        if (copyHdmvPageParameters(&dst->pages[i], ic.pages[i]) < 0)
          return -1;
      }
    }
  }

  epoch_state->active_ic_pts = ic_pts;
  epoch_state->active_ic_present = true;
  return 0;
}
