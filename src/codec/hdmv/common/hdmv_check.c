#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>

#include "hdmv_check.h"

static bool _isPaletteIDInActiveUseHdmvEffectSequence(
  const HdmvEffectSequenceParameters effects,
  uint8_t palette_id
)
{
  for (unsigned i = 0; i < effects.number_of_effects; i++) {
    if (effects.effects[i].palette_id_ref == palette_id)
      return true;
  }

  return false;
}

static bool _isPaletteIDInActiveUseHdmvEpochState(
  const HdmvEpochState * epoch_state,
  uint8_t palette_id,
  int64_t * latest_pts_ret
)
{
  int64_t latest_pts;

  if (HDMV_STREAM_TYPE_IGS == epoch_state->type) {
    /* IGS */
    HdmvICParameters ic;
    int64_t ic_pts;
    if (fetchActiveICHdmvEpochState(epoch_state, &ic, &ic_pts)) {
      for (uint8_t page_idx = 0; page_idx < ic.number_of_pages; page_idx++) {
        const HdmvPageParameters page = ic.pages[page_idx];

        if (
          page.palette_id_ref == palette_id
            // Used in a page of an interactive composition
          || _isPaletteIDInActiveUseHdmvEffectSequence(page.in_effects, palette_id)
            // Used in in_effects() of an interactive composition page
          || _isPaletteIDInActiveUseHdmvEffectSequence(page.out_effects, palette_id)
            // Used in out_effects() of an interactive composition page
        ) {
          latest_pts = ic_pts;
          goto in_use;
        }
      }
    }
  }
  else {
    /* PGS */
    for (unsigned i = 0; i < epoch_state->cur_active_pc; i++) {
      HdmvPCParameters pc;
      int64_t pc_pts;
      lb_assert(fetchPCHdmvEpochState(epoch_state, i, &pc, &pc_pts));

      if (pc.palette_id_ref == palette_id) {
        latest_pts = pc_pts;
        goto in_use; // Used in a presentation composition
      }
    }
  }

  return false;

in_use:
  if (NULL != latest_pts_ret)
    *latest_pts_ret = latest_pts;
  return true;
}

static int _checkAndUpdateHdmvPDSegment(
  const HdmvPDSegmentParameters pal_def,
  HdmvEpochState * epoch_state
)
{
  const HdmvPDParameters pal_desc = pal_def.palette_descriptor;

  uint8_t max_palette_id = getHdmvMaxNbPal(epoch_state->type) - 1;
  /* palette_id */
  if (max_palette_id < pal_desc.palette_id)
    LIBBLU_HDMV_CK_ERROR_RETURN(
      "Invalid PDS, "
      "out of range palette ID, values must be between "
      "0x00 to 0x%02" PRIX8 " inclusive "
      "('palette_id' == 0x%02" PRIX8 ").\n",
      max_palette_id,
      pal_desc.palette_id
    );

  if (justProvidedPaletteHdmvEpochState(epoch_state, pal_desc.palette_id))
    LIBBLU_HDMV_CK_ERROR_RETURN(
      "Invalid PDS, "
      "a palette using the same identifier "
      "as a previous palette of the same DS was encountered "
      "('palette_id' == 0x%02" PRIX8 ").\n",
      pal_desc.palette_id
    );

  HdmvPDSegmentParameters prev_pal_def;
  bool known_palette_id = fetchPaletteHdmvEpochState(
    epoch_state,
    pal_desc.palette_id,
    false,
    &prev_pal_def,
    NULL
  );

  // if (no_display_update && !known_palette_id)
  //   LIBBLU_HDMV_CK_ERROR_RETURN(
  //     "Invalid PDS, "
  //     "unexpected unknown palette ID in a non-Display Update DS "
  //     "('palette_id' == 0x%02" PRIX8 ").\n",
  //     pal_desc.palette_id
  //   );

  /* palette_version_number */
  if (!known_palette_id) {
    if (0x00 != pal_desc.palette_version_number)
      LIBBLU_HDMV_CK_ERROR_RETURN(
        "Invalid PDS, "
        "a palette that has not yet been encountered must be initialized "
        "as version number 0 ('palette_version_number' == 0x%02" PRIX8 ").\n",
        pal_desc.palette_version_number
      );
  }
  else {
    uint8_t prev_ver_nb = prev_pal_def.palette_descriptor.palette_version_number;
    uint8_t cur_ver_nb  = pal_desc.palette_version_number;
    bool shall_be_same  = false; // Shall be same as previous definition
    if (!interpretHdmvVersionNumber(prev_ver_nb, cur_ver_nb, &shall_be_same))
      LIBBLU_HDMV_CK_ERROR_RETURN(
        "Invalid PDS, "
        "unexpected 'palette_version_number' not equal or incremented correctly "
        "(old: 0x%02X / new: 0x%02X).\n",
        prev_ver_nb,
        cur_ver_nb
      );

    // if (no_display_update && !shall_be_same)
    //   LIBBLU_HDMV_CK_ERROR_RETURN(
    //     "Invalid PDS, "
    //     "unexpected incremented palette version number in a "
    //     "non-Display Update DS ('palette_id' == 0x%02" PRIX8 ").\n",
    //     pal_desc.palette_id
    //   );

    if (shall_be_same) {
      TOC();
      if (!comparePDSEpochState(epoch_state, pal_def))
        LIBBLU_HDMV_CK_ERROR_RETURN(
          "Invalid PDS, "
          "non-Display Update segment content is different than previous one "
          "('palette_id' == 0x%02" PRIX8 ").\n",
          pal_desc.palette_id
        );
    }
  }

  setPaletteHdmvEpochState(epoch_state, pal_def);
  return 0;
}

static int _checkAndUpdatePDHdmvDSState(
  const HdmvDSState * ds,
  HdmvEpochState * epoch_state
)
{
  LIBBLU_HDMV_CK_DEBUG("   Checking Palette Definition Segments (PDS).\n");

  HdmvSequencePtr pds_seq = getFirstPDSSequenceHdmvDSState(ds);

  for (; NULL != pds_seq; pds_seq = pds_seq->next_sequence) {
    assert(HDMV_SEGMENT_TYPE_PDS == pds_seq->type);
    HdmvPDSegmentParameters pd = pds_seq->data.pd;

    if (_checkAndUpdateHdmvPDSegment(pd, epoch_state) < 0)
      return -1;

    /* Check PTS of the last composition using updated palette_id */
    uint8_t pal_id = pd.palette_descriptor.palette_id;
    int64_t latest_use_pts;
    if (_isPaletteIDInActiveUseHdmvEpochState(epoch_state, pal_id, &latest_use_pts)) {
      /**
       * This PDS might cause a disruption if it is decoded before the
       * "completness" of the last previous composition that uses it
       * For PGS, this "completness" is the PTS of the Presentation
       * Composition Segment at which Display Update is performed.
       * For IGS, this "completness" is the PTS of the next DS which does not
       * uses the palette or this DS one.
       */
      pds_seq->earliest_available_pts = latest_use_pts;
      pds_seq->has_earliest_available_pts = true;
    }
  }

  return 0;
}

static int _checkDuplicatedPDHdmvDSState(
  const HdmvDSState * prev_ds,
  const HdmvDSState * cur_ds,
  HdmvEpochState * epoch_state
)
{
  HdmvSequencePtr cur_pds_seq = getFirstPDSSequenceHdmvDSState(cur_ds);

  // FIXME: May a DS with non-Display Update composition carry updated PDS
  // which are not referenced by the composition?
  // Assumed to be allowed. The DS is also not required to contain all
  // referenced PDS if its composition_state is Normal.

#if defined(HDMV_STRICT_DUPLICATED_DS)
  HdmvSequencePtr prev_pds_seq = getFirstPDSSequenceHdmvDSState(prev_ds);
  unsigned cur_ds_nb_seq = 0;
#else
  (void) prev_ds;
#endif

  for (; NULL != cur_pds_seq; cur_pds_seq = cur_pds_seq->next_sequence) {
    assert(HDMV_SEGMENT_TYPE_PDS == cur_pds_seq->type);
    HdmvPDSegmentParameters pd = cur_pds_seq->data.pd;

    if (_checkAndUpdateHdmvPDSegment(pd, epoch_state) < 0)
      return -1;

#if defined(HDMV_STRICT_DUPLICATED_DS)
    // Search for the 'palette_id' in previous DS PDS list
    HdmvSequencePtr prev_seq = prev_pds_seq;
    for (; NULL != prev_seq; prev_seq = prev_seq->next_sequence) {
      HdmvPDSegmentParameters prev_pd = prev_seq->data.pd;
      if (prev_pd.palette_descriptor.palette_id == pd.palette_descriptor.palette_id)
        break;
    }

    if (NULL == prev_seq)
      LIBBLU_HDMV_CK_ERROR_RETURN(
        "Invalid PDS, "
        "unknown palette present in non-Display Update DS which were not in "
        "previous DS ('palette_id' == 0x%02" PRIX8 ").",
        pd.palette_descriptor.palette_id
      );

    cur_ds_nb_seq++;
#endif
  }

#if defined(HDMV_STRICT_DUPLICATED_DS)
  // Check for missing PDS (mismatch between the number of PDS in prev and cur DSs)
  unsigned prev_ds_nb_seq = 0;
  HdmvSequencePtr prev_seq = prev_pds_seq;
  for (; NULL != prev_seq; prev_seq = prev_seq->next_sequence)
    prev_ds_nb_seq++;

  if (prev_ds_nb_seq != cur_ds_nb_seq)
    LIBBLU_HDMV_CK_ERROR_RETURN(
      "Invalid non-Display Update DS, "
      "mismatch on the number of PDS (from %u to %u).",
      prev_ds_nb_seq,
      cur_ds_nb_seq
    );
#endif

  return 0;
}

static bool _isObjectIDInActiveUseHdmvEffectSequence(
  const HdmvEffectSequenceParameters effects,
  uint16_t object_id
)
{
  for (unsigned i = 0; i < effects.number_of_effects; i++) {
    const HdmvEffectInfoParameters effect = effects.effects[i];
    for (unsigned j = 0; j < effect.number_of_composition_objects; j++) {
      if (effect.composition_objects[j].object_id_ref == object_id)
        return true; // Used in composition object
    }
  }

  return false;
}

static bool _isObjectIDInActiveUseSequence(
  uint16_t start_object_id_ref,
  uint16_t end_object_id_ref,
  uint16_t object_id
)
{
  if (0xFFFF == start_object_id_ref)
    return false;

  uint16_t start, end;
  if (start_object_id_ref <= end_object_id_ref)
    start = start_object_id_ref, end = end_object_id_ref;
  else
    start = end_object_id_ref, end = start_object_id_ref;

  for (uint16_t object_id_ref = start; object_id_ref <= end; object_id_ref++) {
    if (object_id_ref == object_id)
      return true;
  }
  return false;
}

static bool _isObjectIDInActiveUseHdmvButtonOverlapGroup(
  const HdmvButtonOverlapGroupParameters bog,
  uint16_t object_id
)
{
  for (unsigned i = 0; i < bog.number_of_buttons; i++) {
    const HdmvButtonNormalStateInfoParam nsi = bog.buttons[i].normal_state_info;
    if (_isObjectIDInActiveUseSequence(nsi.start_object_id_ref, nsi.end_object_id_ref, object_id))
      return true;
    const HdmvButtonSelectedStateInfoParam ssi = bog.buttons[i].selected_state_info;
    if (_isObjectIDInActiveUseSequence(ssi.start_object_id_ref, ssi.end_object_id_ref, object_id))
      return true;
    const HdmvButtonActivatedStateInfoParam asi = bog.buttons[i].activated_state_info;
    if (_isObjectIDInActiveUseSequence(asi.start_object_id_ref, asi.end_object_id_ref, object_id))
      return true;
  }

  return false;
}

static bool _isObjectIDInActiveUseHdmvPage(
  const HdmvPageParameters page,
  uint16_t object_id
)
{
  if (_isObjectIDInActiveUseHdmvEffectSequence(page.in_effects, object_id))
    return true; // Used in in_effects
  if (_isObjectIDInActiveUseHdmvEffectSequence(page.out_effects, object_id))
    return true; // Used in out_effects
  for (unsigned i = 0; i < page.number_of_BOGs; i++) {
    if (_isObjectIDInActiveUseHdmvButtonOverlapGroup(page.bogs[i], object_id))
      return true; // Used in BOG
  }
  return false;
}

static bool _isObjectIDInActiveUseHdmvIC(
  const HdmvICParameters ic,
  uint16_t object_id
)
{
  for (uint8_t page_idx = 0; page_idx < ic.number_of_pages; page_idx++) {
    const HdmvPageParameters page = ic.pages[page_idx];
    if (_isObjectIDInActiveUseHdmvPage(page, object_id))
      return true; // Used in page
  }

  return false;
}

static bool _isObjectIDInActiveUseHdmvPC(
  const HdmvPCParameters pc,
  uint16_t object_id
)
{
  for (
    unsigned co_idx = 0;
    co_idx < pc.number_of_composition_objects;
    co_idx++
  ) {
    if (pc.composition_objects[co_idx].object_id_ref == object_id)
      return true; // Used in composition object
  }

  return false;
}

static bool _isObjectIDInActiveUseHdmvEpochState(
  const HdmvEpochState * epoch_state,
  uint16_t object_id,
  int64_t * latest_pts_ret
)
{
  int64_t latest_pts;

  if (HDMV_STREAM_TYPE_IGS == epoch_state->type) {
    /* IGS */
    HdmvICParameters ic;
    int64_t ic_pts;
    if (fetchActiveICHdmvEpochState(epoch_state, &ic, &ic_pts)) {
      if (_isObjectIDInActiveUseHdmvIC(ic, object_id)) {
        latest_pts = ic_pts;
        goto in_use; // Used in interactive composition
      }
    }
  }
  else {
    /* PGS */
    for (unsigned i = 0; i < epoch_state->cur_active_pc; i++) {
      HdmvPCParameters pc;
      int64_t pc_pts;
      lb_assert(fetchPCHdmvEpochState(epoch_state, i, &pc, &pc_pts));

      if (_isObjectIDInActiveUseHdmvPC(pc, object_id)) {
        latest_pts = pc_pts;
        goto in_use; // Used in presentation composition
      }
    }
  }

  return false;

in_use:
  if (NULL != latest_pts_ret)
    *latest_pts_ret = latest_pts;
  return true;
}

static uint16_t _getObjectMaxWidthHdmvEpochState(
  const HdmvEpochState * epoch_state
)
{
  switch (epoch_state->type) {
  case HDMV_STREAM_TYPE_IGS:
    return epoch_state->video_descriptor.video_width;
  case HDMV_STREAM_TYPE_PGS:
    return HDMV_OD_PG_MAX_SIZE;
  }
  LIBBLU_TODO_MSG("Unreachable");
}

static uint16_t _getObjectMaxHeightHdmvEpochState(
  const HdmvEpochState * epoch_state
)
{
  switch (epoch_state->type) {
  case HDMV_STREAM_TYPE_IGS:
    return epoch_state->video_descriptor.video_height;
  case HDMV_STREAM_TYPE_PGS:
    return HDMV_OD_PG_MAX_SIZE;
  }
  LIBBLU_TODO_MSG("Unreachable");
}

static int _checkDimensionsHdmvODParameters(
  const HdmvODParameters obj_def,
  const HdmvEpochState * epoch_state
)
{
  bool invalid = false;

  /* object_width */
  if (obj_def.object_width < HDMV_OD_MIN_SIZE) {
    LIBBLU_HDMV_CK_ERROR(
      "Invalid ODS, "
      "object width shall be greater than or equal to "
      STR(HDMV_OD_MIN_SIZE) " pixels ('object_width' == %" PRIu16 ").\n",
      obj_def.object_width
    );
    invalid = true;
  }

  uint16_t max_object_width = _getObjectMaxWidthHdmvEpochState(epoch_state);
  if (max_object_width < obj_def.object_width) {
    LIBBLU_HDMV_CK_ERROR(
      "Invalid ODS, "
      "object width shall be less than or equal to %" PRIu16 " pixels "
      "('object_width' == %" PRIu16 ").\n",
      max_object_width,
      obj_def.object_width
    );
    invalid = true;
  }

  /* object_height */
  if (obj_def.object_height < HDMV_OD_MIN_SIZE) {
    LIBBLU_HDMV_CK_ERROR(
      "Invalid ODS, "
      "object height shall be greater than or equal to "
      STR(HDMV_OD_MIN_SIZE) " pixels ('object_height' == %" PRIu16 ").\n",
      obj_def.object_height
    );
    invalid = true;
  }

  uint16_t max_object_height = _getObjectMaxHeightHdmvEpochState(epoch_state);
  if (max_object_height < obj_def.object_height) {
    LIBBLU_HDMV_CK_ERROR(
      "Invalid ODS, "
      "object height shall be less than or equal to %" PRIu16 " pixels "
      "('object_height' == %" PRIu16 ").\n",
      max_object_height,
      obj_def.object_height
    );
    invalid = true;
  }

  return invalid ? -1 : 0;
}

static int _checkUpdateDimensionsHdmvODParameters(
  const HdmvODParameters prev_obj_def,
  const HdmvODParameters cur_obj_def
)
{
  bool invalid = false;

  /* object_width */
  if (prev_obj_def.object_width < cur_obj_def.object_width) {
    LIBBLU_HDMV_CK_ERROR(
      "Invalid ODS, "
      "the width of the update object must be the same as the updated object "
      " (from 'object_width' == %" PRIu16 " to 'object_width' == %" PRIu16 ").\n",
      prev_obj_def.object_width,
      cur_obj_def.object_width
    );
    invalid = true;
  }

  /* object_height */
  if (prev_obj_def.object_height < cur_obj_def.object_height) {
    LIBBLU_HDMV_CK_ERROR(
      "Invalid ODS, "
      "the height of the update object must be the same as the updated object "
      " (from 'object_height' == %" PRIu16 " to 'object_height' == %" PRIu16 ").\n",
      prev_obj_def.object_height,
      cur_obj_def.object_height
    );
    invalid = true;
  }

  return invalid ? -1 : 0;
}

static int _checkAndUpdateHdmvODSegment(
  const HdmvODParameters obj_def,
  HdmvEpochState * epoch_state
)
{
  const HdmvODescParameters obj_desc = obj_def.object_descriptor;

  uint16_t max_object_id = getHdmvMaxNbObj(epoch_state->type) - 1;
  /* object_id */
  if (max_object_id < obj_desc.object_id)
    LIBBLU_HDMV_CK_ERROR_RETURN(
      "Invalid ODS, "
      "out of range object ID, values must be between "
      "0x0000 to 0x%04" PRIX16 " inclusive "
      "('object_id' == 0x%04" PRIX16 ").\n",
      max_object_id,
      obj_desc.object_id
    );

  if (justProvidedObjectHdmvEpochState(epoch_state, obj_desc.object_id))
    LIBBLU_HDMV_CK_ERROR_RETURN(
      "Invalid ODS, "
      "an object using the same identifier "
      "as a previous object of the same DS was encountered "
      "('object_id' == 0x%04" PRIX16 ").\n",
      obj_desc.object_id
    );

  HdmvODParameters prev_obj_def;
  bool known_object_id = fetchObjectHdmvEpochState(
    epoch_state,
    obj_desc.object_id,
    false,
    &prev_obj_def,
    NULL
  );

  // if (no_display_update && !known_object_id)
  //   LIBBLU_HDMV_CK_ERROR_RETURN(
  //     "Invalid ODS, "
  //     "unexpected unknown object ID in a non-Display Update DS "
  //     "('object_id' == 0x%04" PRIX16 ").\n",
  //     obj_desc.object_id
  //   );

  /* object_version_number */
  if (!known_object_id) {
    if (0x00 != obj_desc.object_version_number)
      LIBBLU_HDMV_CK_ERROR_RETURN(
        "Invalid ODS, "
        "an object that has not yet been encountered must be initialized "
        "as version number 0 ('object_version_number' == 0x%02" PRIX8 ").\n",
        obj_desc.object_version_number
      );

    if (_checkDimensionsHdmvODParameters(obj_def, epoch_state) < 0)
      LIBBLU_HDMV_CK_ERROR_RETURN(
        "Invalid ODS, "
        "object with ID 0x%04" PRIX16 " as invalid dimensions.\n",
        obj_desc.object_id
      );

    // TODO: Check RLE data?
  }
  else {
    uint8_t prev_ver_nb = prev_obj_def.object_descriptor.object_version_number;
    uint8_t cur_ver_nb  = obj_desc.object_version_number;
    bool shall_be_same  = false; // Shall be same as previous definition
    if (!interpretHdmvVersionNumber(prev_ver_nb, cur_ver_nb, &shall_be_same))
      LIBBLU_HDMV_CK_ERROR_RETURN(
        "Invalid ODS, "
        "unexpected 'object_version_number' not equal or incremented correctly "
        "(old: 0x%02X / new: 0x%02X).\n",
        prev_ver_nb,
        cur_ver_nb
      );

    // if (no_display_update && !shall_be_same)
    //   LIBBLU_HDMV_CK_ERROR_RETURN(
    //     "Invalid ODS, "
    //     "unexpected incremented object version number in a "
    //     "non-Display Update DS ('object_id' == 0x%04" PRIX16 ").\n",
    //     obj_desc.object_id
    //   );

    /* object_data_length */ // Already checked

    /* object_width/object_height */
    if (_checkUpdateDimensionsHdmvODParameters(prev_obj_def, obj_def) < 0)
      LIBBLU_HDMV_CK_ERROR_RETURN(
        "Invalid ODS, "
        "object with ID 0x%04" PRIX16 " as invalid dimensions.\n",
        obj_desc.object_id
      );

    if (shall_be_same) {
      // TODO: Check RLE data?
      if (!compareODSEpochState(epoch_state, obj_def))
        LIBBLU_HDMV_CK_ERROR_RETURN(
          "Invalid ODS, "
          "non-Display Update segment content is different than previous one "
          "('object_id' == 0x%04" PRIX16 ").\n",
          obj_desc.object_id
        );
    }
  }

  setObjectHdmvEpochState(epoch_state, obj_def);
  return 0;
}

static int _checkAndUpdateODHdmvDSState(
  const HdmvDSState * ds,
  HdmvEpochState * epoch_state
)
{
  LIBBLU_HDMV_CK_DEBUG("   Checking Object Definition Segments (ODS).\n");

  HdmvSequencePtr ods_seq = getFirstODSSequenceHdmvDSState(ds);

  for (; NULL != ods_seq; ods_seq = ods_seq->next_sequence) {
    assert(HDMV_SEGMENT_TYPE_ODS == ods_seq->type);
    HdmvODParameters od = ods_seq->data.od;

    if (_checkAndUpdateHdmvODSegment(od, epoch_state) < 0)
      return -1;

    uint16_t obj_id = od.object_descriptor.object_id;
    int64_t latest_use_pts;
    if (_isObjectIDInActiveUseHdmvEpochState(epoch_state, obj_id, &latest_use_pts)) {
      /**
       * This PDS might cause a disruption if it is decoded before the
       * "completeness" of the last previous composition that uses it
       * For PGS, this "completeness" is the PTS of the Presentation
       * Composition Segment at which Display Update is performed.
       * For IGS, this "completeness" is the PTS of the next DS which does not
       * uses the palette or this DS one.
       */
      ods_seq->earliest_available_pts = latest_use_pts;
      ods_seq->has_earliest_available_pts = true;
    }
  }

  return 0;
}

static int _checkDuplicatedODHdmvDSState(
  const HdmvDSState * prev_ds,
  const HdmvDSState * cur_ds,
  HdmvEpochState * epoch_state
)
{
  HdmvSequencePtr cur_ods_seq  = getFirstODSSequenceHdmvDSState(cur_ds);

  // FIXME: May a DS with non-Display Update composition carry updated ODS
  // which are not referenced by the composition?
  // Assumed to be allowed. The DS is also not required to contain all
  // referenced ODS if its composition_state is Normal.

#if defined(HDMV_STRICT_DUPLICATED_DS)
  HdmvSequencePtr prev_ods_seq = getFirstODSSequenceHdmvDSState(prev_ds);
  unsigned cur_ds_nb_seq = 0;
#else
  (void) prev_ds;
#endif

  for (; NULL != cur_ods_seq; cur_ods_seq = cur_ods_seq->next_sequence) {
    assert(HDMV_SEGMENT_TYPE_ODS == cur_ods_seq->type);
    HdmvODParameters od = cur_ods_seq->data.od;

    if (_checkAndUpdateHdmvODSegment(od, epoch_state) < 0)
      return -1;

#if defined(HDMV_STRICT_DUPLICATED_DS)
    // Search for the 'object_id' in previous DS ODS list
    HdmvSequencePtr prev_seq = prev_ods_seq;
    for (; NULL != prev_seq; prev_seq = prev_seq->next_sequence) {
      HdmvODParameters prev_od = prev_seq->data.od;
      if (prev_od.object_descriptor.object_id == od.object_descriptor.object_id)
        break;
    }

    if (NULL == prev_seq)
      LIBBLU_HDMV_CK_ERROR_RETURN(
        "Invalid ODS, "
        "unknown object present in non-Display Update DS which were not in "
        "previous DS ('object_id' == 0x%02" PRIX8 ").",
        od.object_descriptor.object_id
      );

    cur_ds_nb_seq++;
#endif
  }

#if defined(HDMV_STRICT_DUPLICATED_DS)
  // Check for missing ODS (mismatch between the number of ODS in prev and cur DSs)
  unsigned prev_ds_nb_seq = 0;
  HdmvSequencePtr prev_seq = prev_ods_seq;
  for (; NULL != prev_seq; prev_seq = prev_seq->next_sequence)
    prev_ds_nb_seq++;

  if (prev_ds_nb_seq != cur_ds_nb_seq)
    LIBBLU_HDMV_CK_ERROR_RETURN(
      "Invalid non-Display Update DS, "
      "mismatch on the number of ODS (from %u to %u).",
      prev_ds_nb_seq,
      cur_ds_nb_seq
    );
#endif

  return 0;
}

static int _checkWindowInfo(
  const HdmvWindowInfoParameters window,
  HdmvVDParameters video_desc,
  HdmvRectangle * window_rectangles_arr_ref,
  unsigned window_idx
)
{
  /* window_id */ // Already checked

  /* window_horizontal_position/window_width */
  if (video_desc.video_width < window.window_horizontal_position + window.window_width) {
    LIBBLU_HDMV_CK_ERROR_RETURN(
      "Out of Graphical Plane window 0x%02" PRIX8 " horizontal position/width, "
      "('video_width' == %u, "
      "'window_horizontal_position' == %u, "
      "'window_width' == %u).\n",
      window.window_id,
      video_desc.video_width,
      window.window_horizontal_position,
      window.window_width
    );
  }

  /* window_vertical_position/window_height */
  if (video_desc.video_height < window.window_vertical_position + window.window_height) {
    LIBBLU_HDMV_CK_ERROR_RETURN(
      "Out of Graphical Plane window 0x%02" PRIX8 " vertical position/height, "
      "('video_height' == %u, "
      "'window_vertical_position' == %u, "
      "'window_height' == %u).\n",
      window.window_id,
      video_desc.video_height,
      window.window_vertical_position,
      window.window_height
    );
  }

  HdmvRectangle win_rect = {
    .x = window.window_horizontal_position,
    .y = window.window_vertical_position,
    .w = window.window_width,
    .h = window.window_height
  };

  for (unsigned i = 0; i < window_idx; i++) {
    if (areCollidingRectangle(window_rectangles_arr_ref[i], win_rect)) {
      LIBBLU_HDMV_CK_ERROR_RETURN(
        "Invalid window position, pixels overlap those of a previous window.\n"
      );
    }
  }

  window_rectangles_arr_ref[window_idx] = win_rect;
  return 0;
}

static int _checkCO(
  const HdmvCOParameters compo_obj,
  const HdmvWindowInfoParameters * windows_arr,
  uint8_t number_of_windows,
  const HdmvEpochState * epoch_state,
  bool object_must_be_in_current_DS,
  bool no_display_update,
  HdmvRectangle * compo_obj_rect_ret
)
{

  /* object_id_ref */
  HdmvODParameters obj_def;
  bool updated_object;
  bool found_object = fetchObjectHdmvEpochState(
    epoch_state,
    compo_obj.object_id_ref,
    object_must_be_in_current_DS,
    &obj_def,
    &updated_object
  );

  if (!found_object)
    LIBBLU_HDMV_CK_ERROR_RETURN(
      "Object ID referenced by Composition Object is unknown "
      "('object_id_ref' == 0x%02" PRIX8 " does not match any 'object_id' "
      "or has not been supplied since last Acquistion Point).\n",
      compo_obj.object_id_ref
    );

  if (no_display_update && updated_object)
    LIBBLU_HDMV_CK_ERROR_RETURN(
      "Invalid non-Display Update Composition Object, "
      "the referenced object has been updated although the composition "
      "does not describe a Display Update "
      "('object_id_ref' == 0x%02" PRIX8 ").",
      compo_obj.object_id_ref
    );

  /* window_id_ref */
  if (number_of_windows < compo_obj.window_id_ref)
    LIBBLU_HDMV_CK_ERROR_RETURN(
      "Window ID referenced by Composition Object is unknown "
      "('window_id_ref' == 0x%02" PRIX8 " does not match any 'window_id').\n",
      compo_obj.window_id_ref
    );
  const HdmvWindowInfoParameters win = windows_arr[compo_obj.window_id_ref];
  HdmvRectangle win_rect = {
    .x = win.window_horizontal_position,
    .y = win.window_vertical_position,
    .w = win.window_width,
    .h = win.window_height
  };

  /* object_cropped_flag */ // Not checked
  /* forced_on_flag */ // Not checked

  /* composition_object_horizontal_position */
  /* composition_object_vertical_position */
  HdmvRectangle compo_rect = {
    .x = compo_obj.composition_object_horizontal_position,
    .y = compo_obj.composition_object_vertical_position,
    .w = obj_def.object_width,
    .h = obj_def.object_height
  };

  if (compo_obj.object_cropped_flag) {
    /* object_cropping_horizontal_position */
    if (obj_def.object_width <= compo_obj.object_cropping.horizontal_position)
      LIBBLU_HDMV_CK_ERROR_RETURN(
        "Invalid object cropping horizontal position, "
        "shall be less than cropped object width "
        "('object_width' == %u, 'object_cropping_horizontal_position' == %u).\n",
        obj_def.object_width,
        compo_obj.object_cropping.horizontal_position
      );

    /* object_cropping_vertical_position */
    if (obj_def.object_height <= compo_obj.object_cropping.vertical_position)
      LIBBLU_HDMV_CK_ERROR_RETURN(
        "Invalid object cropping verical position, "
        "shall be less than cropped object height "
        "('object_height' == %u, 'object_cropping_vertical_position' == %u).\n",
        obj_def.object_height,
        compo_obj.object_cropping.vertical_position
      );

    /* object_cropping_width */
    if (!compo_obj.object_cropping.width)
      LIBBLU_HDMV_CK_ERROR_RETURN(
        "Invalid object cropping width parameter, shall not be 0.\n"
      );
    if (
      obj_def.object_width - compo_obj.object_cropping.horizontal_position
      < compo_obj.object_cropping.width
    ) {
      LIBBLU_HDMV_CK_ERROR_RETURN(
        "Invalid object cropping width parameter, "
        "outside object dimensions "
        "('object_width' == %u, 'object_cropping_horizontal_position' = %u, "
        "'object_cropping_width' = %u).\n",
        obj_def.object_width,
        compo_obj.object_cropping.horizontal_position,
        compo_obj.object_cropping.width
      );
    }

    /* object_cropping_height */
    if (!compo_obj.object_cropping.height)
      LIBBLU_HDMV_CK_ERROR_RETURN(
        "Invalid object cropping height parameter, shall not be 0.\n"
      );
    if (
      obj_def.object_height - compo_obj.object_cropping.vertical_position
      < compo_obj.object_cropping.height
    ) {
      LIBBLU_HDMV_CK_ERROR_RETURN(
        "Invalid object cropping height parameter, "
        "outside object dimensions "
        "('object_height' == %u, 'object_cropping_vertical_position' = %u, "
        "'object_cropping_width' = %u).\n",
        obj_def.object_height,
        compo_obj.object_cropping.vertical_position,
        compo_obj.object_cropping.height
      );
    }

    compo_rect.w = compo_obj.object_cropping.width;
    compo_rect.h = compo_obj.object_cropping.height;
  }

  if (!isInsideRectangle(win_rect, compo_rect))
    LIBBLU_HDMV_CK_ERROR_RETURN(
      "Invalid composition object position, "
      "pixels outside of associated window.\n"
    );

  if (NULL != compo_obj_rect_ret)
    *compo_obj_rect_ret = compo_rect;

  return 0;
}

static int _checkNonCollidingCO(
  HdmvRectangle * compo_obj_rectangles_arr_ref,
  unsigned compo_obj_idx
)
{
  HdmvRectangle compo_rect = compo_obj_rectangles_arr_ref[compo_obj_idx];

  for (unsigned i = 0; i < compo_obj_idx; i++) {
    if (areCollidingRectangle(compo_obj_rectangles_arr_ref[i], compo_rect)) {
      LIBBLU_HDMV_CK_ERROR_RETURN(
        "Invalid composition object position, "
        "pixels overlap those of a previous composition object.\n"
      );
    }
  }

  return 0;
}

#define LIBBLU_HDMV_CK_IC_DEBUG(format, ...)  \
  LIBBLU_HDMV_CK_DEBUG("    " format, ##__VA_ARGS__)

static int _checkEffectInfo(
  const HdmvEffectSequenceParameters effects_sequence,
  const HdmvEffectInfoParameters effect,
  const HdmvEpochState * epoch_state,
  bool no_display_update
)
{
  /* effect_duration */ // TODO: Unchecked?

  /* palette_id_ref */
  bool updated_palette;
  if (!fetchPaletteHdmvEpochState(epoch_state, effect.palette_id_ref, true, NULL, &updated_palette))
    LIBBLU_HDMV_CK_ERROR_RETURN(
      "Palette ID referenced by Effect is unknown "
      "('palette_id_ref' == 0x%02" PRIX8 " does not match any 'palette_id', "
      "or has not been supplied since last Acquistion Point).\n",
      effect.palette_id_ref
    );

  if (no_display_update && updated_palette)
    LIBBLU_HDMV_CK_ERROR_RETURN(
      "Invalid non-Display Update Interactive Composition, "
      "the referenced palette has been updated although the composition "
      "does not describe a Display Update "
      "('palette_id_ref' == 0x%02" PRIX8 ").",
      effect.palette_id_ref
    );

  /* number_of_composition_objects */ // Already checked

  const uint8_t nb_win = effects_sequence.number_of_windows;
  const HdmvWindowInfoParameters * win = effects_sequence.windows;
  HdmvRectangle co_rect[HDMV_MAX_NB_EFFECT_COMPO_OBJ];

  for (unsigned idx = 0; idx < effect.number_of_composition_objects; idx++) {
    /* Composition_object() */
    const HdmvCOParameters co = effect.composition_objects[idx];
    if (_checkCO(co, win, nb_win, epoch_state, true, no_display_update, &co_rect[idx]) < 0)
      return -1;
    if (_checkNonCollidingCO(co_rect, idx) < 0)
      return -1;
  }

  return 0;
}

static int _checkEffectSequence(
  HdmvEffectSequenceParameters effects_sequence,
  const HdmvEpochState * epoch_state,
  bool no_display_update
)
{
  /* number_of_windows */ // Already checked

  if (!no_display_update) {
    const HdmvVDParameters video_desc = epoch_state->video_descriptor;
    HdmvRectangle win_rect[HDMV_MAX_NB_ES_WINDOWS];

    for (uint8_t win_i = 0; win_i < effects_sequence.number_of_windows; win_i++) {
      /* Window() */
      const HdmvWindowInfoParameters win = effects_sequence.windows[win_i];

      /* window_id */
      if (win_i != win.window_id)
        LIBBLU_HDMV_CK_ERROR_RETURN(
          "Unexpected effect sequence non sequential window id "
          "(got 0x%02" PRIX8 ", expect 0x%02" PRIX8 ").\n",
          win.window_id,
          win_i
        );

      if (_checkWindowInfo(win, video_desc, win_rect, win_i) < 0)
        return -1;
    }
  }

  /* number_of_effects */ // Already checked

  for (uint8_t eff_i = 0; eff_i < effects_sequence.number_of_effects; eff_i++) {
    /* Effect() */
    const HdmvEffectInfoParameters eff = effects_sequence.effects[eff_i];
    if (_checkEffectInfo(effects_sequence, eff, epoch_state, no_display_update) < 0)
      LIBBLU_HDMV_CK_ERROR_RETURN(
        "Error in Effect %u of Effect Sequence.\n",
        eff_i
      );
  }

  return 0;
}

static int _checkBtnStateObj(
  const HdmvEpochState * epoch_state,
  uint16_t start_object_id_ref,
  uint16_t end_object_id_ref,
  uint16_t * btn_state_obj_width_ret,
  uint16_t * btn_state_obj_height_ret,
  bool no_display_update
)
{
  if (0xFFFF == start_object_id_ref)  {
    // No object attached
    if (NULL != btn_state_obj_width_ret)
      *btn_state_obj_width_ret = 0;
    if (NULL != btn_state_obj_height_ret)
      *btn_state_obj_height_ret = 0;
    return 0;
  }

  if (0xFFFF == end_object_id_ref)
    LIBBLU_HDMV_CK_ERROR_RETURN(
      "Invalid button state objects sequence.\n",
      start_object_id_ref,
      end_object_id_ref
    );

  uint16_t start, end;
  if (start_object_id_ref <= end_object_id_ref)
    start = start_object_id_ref, end = end_object_id_ref;
  else
    start = end_object_id_ref, end = start_object_id_ref;

  uint16_t state_width = 0, state_height = 0;
  for (uint16_t object_id_ref = start; object_id_ref <= end; object_id_ref++) {
    HdmvODParameters od;
    bool updated_od;
    if (!fetchObjectHdmvEpochState(epoch_state, object_id_ref, true, &od, &updated_od))
      LIBBLU_HDMV_CK_ERROR_RETURN(
        "Object ID referenced by Composition Object is unknown "
        "('object_id_ref' == 0x%02" PRIX8 " does not match any 'object_id' "
        "or has not been supplied since last Acquistion Point).\n",
        object_id_ref
      );

    if (no_display_update && updated_od)
      LIBBLU_HDMV_CK_ERROR_RETURN(
        "Invalid non-Display Update Interactive Composition, "
        "the referenced object has been updated although the composition "
        "does not describe a Display Update "
        "('object_id_ref' == 0x%02" PRIX8 ").",
        object_id_ref
      );

    /* Check objects dimensions uniformization */
    if (!state_width) {
      state_width  = od.object_width;
      state_height = od.object_height;
    }
    else {
      if (state_width != od.object_width || state_height != od.object_height)
        LIBBLU_HDMV_CK_ERROR_RETURN(
          "Button state object sequence dimensions mismatch "
          "(from %" PRIu16 "x%" PRIu16 " on 'object_id' 0x%04" PRIX16
          " to %" PRIu16 "x%" PRIu16 " on 'object_id' 0x%04" PRIX16 ").\n",
          state_width, state_height, start,
          od.object_width, od.object_height, object_id_ref
        );
    }
  }

  if (NULL != btn_state_obj_width_ret)
    *btn_state_obj_width_ret = state_width;
  if (NULL != btn_state_obj_height_ret)
    *btn_state_obj_height_ret = state_height;

  return 0;
}

static int _checkBtnObjDims(
  uint16_t * btn_width_ref,
  uint16_t * btn_height_ref,
  uint16_t btn_state_width,
  uint16_t btn_state_height
)
{
  if (0 == *btn_width_ref) {
    /* First object, just save as button dimensions */
    *btn_width_ref = btn_state_width;
    *btn_height_ref = btn_state_height;
    return 0;
  }

  if (0 == btn_state_width)
    return 0; // No object in button state
  if (*btn_width_ref != btn_state_width || *btn_height_ref != btn_state_height)
    return -1; // Mismatch

  return 0; // Matching dimensions
}

static int _checkBtnNSI(
  const HdmvButtonNormalStateInfoParam normal_state_info,
  uint16_t button_id,
  const HdmvEpochState * epoch_state,
  uint16_t * btn_obj_w_ref,
  uint16_t * btn_obj_h_ref,
  bool no_display_update
)
{
  /* normal_start_object_id_ref */
  uint16_t start_obj_id_ref = normal_state_info.start_object_id_ref;
  /* normal_end_object_id_ref */
  uint16_t end_obj_id_ref   = normal_state_info.end_object_id_ref;

  uint16_t state_obj_w, state_obj_h;
  int ret = _checkBtnStateObj(
    epoch_state,
    start_obj_id_ref,
    end_obj_id_ref,
    &state_obj_w,
    &state_obj_h,
    no_display_update
  );
  if (ret < 0)
    return -1;

  if (no_display_update)
    return 0; // No further tests

  lb_assert(0 <= _checkBtnObjDims(btn_obj_w_ref, btn_obj_h_ref, state_obj_w, state_obj_h)
  );

  if (start_obj_id_ref == end_obj_id_ref) {
    /* normal_repeat_flag */
    if (normal_state_info.repeat_flag)
      LIBBLU_HDMV_CK_WARNING(
        "Repeat animation flag set for normal state, "
        "but the state does not contain animation "
        "('button_id' == 0x%04" PRIX16 ", "
        "'normal_start_object_id_ref' == 0x%04" PRIX16 ", "
        "'normal_end_object_id_ref' == 0x%04" PRIX16 ", "
        "'normal_repeat_flag' == 0b1).\n",
        button_id,
        start_obj_id_ref,
        end_obj_id_ref
      );

    /* normal_complete_flag */
    if (normal_state_info.complete_flag)
      LIBBLU_HDMV_CK_WARNING(
        "Complete animation flag set for normal state, "
        "but the state does not contain animation "
        "('button_id' == 0x%04" PRIX16 ", "
        "'normal_start_object_id_ref' == 0x%04" PRIX16 ", "
        "'normal_end_object_id_ref' == 0x%04" PRIX16 ", "
        "'normal_complete_flag' == 0b1).\n",
        button_id,
        start_obj_id_ref,
        end_obj_id_ref
      );
  }

  if (normal_state_info.complete_flag)
    LIBBLU_HDMV_CK_WARNING(
      "Use of deprecated complete animation flag for normal state "
      "('button_id' == 0x%04" PRIX16 ", "
      "'normal_complete_flag' == 0b1).\n",
      button_id
    );

  return 0;
}

static int _checkBtnSSI(
  const HdmvButtonSelectedStateInfoParam selected_state_info,
  uint16_t button_id,
  const HdmvEpochState * epoch_state,
  uint16_t * btn_obj_w_ref,
  uint16_t * btn_obj_h_ref,
  bool no_display_update
)
{
  /* selected_state_sound_id_ref */
  // TODO: Enable support for sound.bdmv file generation.

  /* selected_start_object_id_ref */
  uint16_t start_obj_id_ref = selected_state_info.start_object_id_ref;
  /* selected_end_object_id_ref */
  uint16_t end_obj_id_ref   = selected_state_info.end_object_id_ref;

  uint16_t state_obj_w, state_obj_h;
  int ret = _checkBtnStateObj(
    epoch_state,
    start_obj_id_ref,
    end_obj_id_ref,
    &state_obj_w,
    &state_obj_h,
    no_display_update
  );
  if (ret < 0)
    return -1;

  if (no_display_update)
    return 0; // No further tests

  if (_checkBtnObjDims(btn_obj_w_ref, btn_obj_h_ref, state_obj_w, state_obj_h) < 0)
    LIBBLU_HDMV_CK_ERROR_RETURN(
      "Mismatch between dimensions of button normal and "
      "selected states used objects ('button_id' == 0x%04" PRIX16 ", "
      "from %" PRIu16 "x%" PRIu16 " to %" PRIu16 "x%" PRIu16 ").\n",
      button_id,
      *btn_obj_w_ref,
      *btn_obj_h_ref,
      state_obj_w,
      state_obj_h
    );

  if (start_obj_id_ref == end_obj_id_ref) {
    /* selected_repeat_flag */
    if (selected_state_info.repeat_flag)
      LIBBLU_HDMV_CK_WARNING(
        "Repeat animation flag set for selected state, "
        "but the state does not contain animation "
        "('button_id' == 0x%04" PRIX16 ", "
        "'normal_start_object_id_ref' == 0x%04" PRIX16 ", "
        "'normal_end_object_id_ref' == 0x%04" PRIX16 ", "
        "'normal_repeat_flag' == 0b1).\n",
        button_id,
        start_obj_id_ref,
        end_obj_id_ref
      );

    /* selected_complete_flag */
    if (selected_state_info.complete_flag)
      LIBBLU_HDMV_CK_WARNING(
        "Complete animation flag set for selected state, "
        "but the state does not contain animation "
        "('button_id' == 0x%04" PRIX16 ", "
        "'normal_start_object_id_ref' == 0x%04" PRIX16 ", "
        "'normal_end_object_id_ref' == 0x%04" PRIX16 ", "
        "'normal_complete_flag' == 0b1).\n",
        button_id,
        start_obj_id_ref,
        end_obj_id_ref
      );
  }

  if (selected_state_info.complete_flag)
    LIBBLU_HDMV_CK_WARNING(
      "Use of deprecated complete animation flag for selected state "
      "('button_id' == 0x%04" PRIX16 ", "
      "'normal_complete_flag' == 0b1).\n",
      button_id
    );

  return 0;
}

static int _checkBtnASI(
  const HdmvButtonActivatedStateInfoParam activated_state_info,
  uint16_t button_id,
  const HdmvEpochState * epoch_state,
  uint16_t * btn_obj_w_ref,
  uint16_t * btn_obj_h_ref,
  bool no_display_update
)
{
  /* activated_state_sound_id_ref */
  // TODO: Enable support for sound.bdmv file generation.

  /* activated_start_object_id_ref */
  uint16_t start_obj_id_ref = activated_state_info.start_object_id_ref;
  /* activated_end_object_id_ref */
  uint16_t end_obj_id_ref   = activated_state_info.end_object_id_ref;

  uint16_t state_obj_w, state_obj_h;
  int ret = _checkBtnStateObj(
    epoch_state,
    start_obj_id_ref,
    end_obj_id_ref,
    &state_obj_w,
    &state_obj_h,
    no_display_update
  );
  if (ret < 0)
    return -1;

  if (no_display_update)
    return 0; // No further tests

  if (_checkBtnObjDims(btn_obj_w_ref, btn_obj_h_ref, state_obj_w, state_obj_h) < 0)
    LIBBLU_HDMV_CK_ERROR_RETURN(
      "Mismatch between dimensions of button normal, selected and "
      "activated states used objects ('button_id' == 0x%04" PRIX16 ", "
      "from %" PRIu16 "x%" PRIu16 " to %" PRIu16 "x%" PRIu16 ").\n",
      button_id,
      *btn_obj_w_ref,
      *btn_obj_h_ref,
      state_obj_w,
      state_obj_h
    );

  return 0;
}

static int _checkHdmvICPage(
  const HdmvICParameters ic,
  uint8_t page_idx,
  const HdmvEpochState * epoch_state,
  bool no_display_update
)
{
  const HdmvPageParameters page = ic.pages[page_idx];

  const HdmvVDParameters video_desc = epoch_state->video_descriptor;

  uint8_t present_button_id_values[HDMV_MAX_NB_BUTTONS];
  MEMSET_ARRAY(present_button_id_values, 0xFF);

  uint16_t present_button_numeric_select_values[HDMV_MAX_BUTTON_NUMERIC_SELECT_VALUE+1];
  MEMSET_ARRAY(present_button_numeric_select_values, 0xFF);

  /* page_id */
  if (page_idx != page.page_id)
    LIBBLU_HDMV_CK_ERROR_RETURN(
      "Unexpected Interactive Composition page non-sequential id value "
      "('page_id' == 0x%02X, expect 0x%02X).\n",
      page.page_id,
      page_idx
    );

  HdmvPageParameters prev_page;
  bool known_prev_page = fetchActiveICPageHdmvEpochState(
    epoch_state, page.page_id, &prev_page
  );

  if (no_display_update && !known_prev_page)
    LIBBLU_HDMV_CK_ERROR_RETURN(
      "Invalid Interactive Composition, "
      "unexpected unknown page ID in a non-Display Update DS "
      "('page_id' == 0x%02" PRIX8 ").\n",
      page.page_id
    );

  bool shall_be_same = false; // Shall be same as previous definition

  /* page_version_number */
  if (!known_prev_page) {
    if (0x00 != page.page_version_number)
      LIBBLU_HDMV_CK_ERROR_RETURN(
        "Invalid Interactive Composition, "
        "a page that has not yet been encountered must be initialized "
        "as version number 0 ('page_version_number' == 0x%02" PRIX8 ").\n",
        page.page_version_number
      );
  }
  else {
    uint8_t prev_ver_nb = prev_page.page_version_number;
    uint8_t cur_ver_nb  = page.page_version_number;
    if (!interpretHdmvVersionNumber(prev_ver_nb, cur_ver_nb, &shall_be_same))
      LIBBLU_HDMV_CK_ERROR_RETURN(
        "Invalid Interactive Composition, "
        "unexpected 'page_version_number' not equal or incremented correctly "
        "compared to previously known page version number "
        "(old: 0x%02X / new: 0x%02X).\n",
        prev_ver_nb,
        cur_ver_nb
      );

    if (no_display_update && !shall_be_same)
      LIBBLU_HDMV_CK_ERROR_RETURN(
        "Invalid Interactive Composition, "
        "unexpected incremented page version number in a "
        "non-Display Update DS ('page_id' == 0x%02" PRIX8 ").\n",
        page.page_id
      );

    if (shall_be_same) {
      if (!compareICPageEpochState(epoch_state, page))
        LIBBLU_HDMV_CK_ERROR_RETURN(
          "Invalid Interactive Composition, "
          "non-Display Update page content is different than previous one "
          "('page_id' == 0x%02" PRIX8 ", "
          "'page_version_number' == 0x%02" PRIX8 ").\n",
          page.page_id,
          page.page_version_number
        );
      if (!no_display_update)
        return 0; // No further verification (otherwise check pal/obj)
    }
  }

  /* in_effects */
  if (_checkEffectSequence(page.in_effects, epoch_state, no_display_update) < 0)
    LIBBLU_HDMV_CK_ERROR_RETURN(
      "Invalid Interactive Composition page 0x%02X "
      "in-effects effect sequence.\n",
      page_idx
    );

  /* out_effects */
  if (_checkEffectSequence(page.in_effects, epoch_state, no_display_update) < 0)
    LIBBLU_HDMV_CK_ERROR_RETURN(
      "Invalid Interactive Composition page 0x%02X "
      "out-effects effect sequence.\n",
      page_idx
    );

  // animation_frame_rate_code
  // default_selected_button_id_ref
  // palette_id_ref
  // Checked after (only if !shall_be_same)

  /* palette_id_ref */
  bool updated_palette;
  if (!fetchPaletteHdmvEpochState(epoch_state, page.palette_id_ref, true, NULL, &updated_palette))
    LIBBLU_HDMV_CK_ERROR_RETURN(
      "Palette ID referenced by Page is unknown "
      "('palette_id_ref' == 0x%02" PRIX8 " does not match any 'palette_id' "
      "or has not been supplied since last Acquistion Point).\n",
      page.palette_id_ref
    );

  if (no_display_update && updated_palette)
    LIBBLU_HDMV_CK_ERROR_RETURN(
      "Palette referenced by Page has been updated although the composition "
      "does not describe a Display Update "
      "('palette_id_ref' == 0x%02" PRIX8 ").",
      page.palette_id_ref
    );

  if (shall_be_same) {
    assert(no_display_update); // Only no-Display Update IC need these checks

    for (uint8_t bog_idx = 0; bog_idx < page.number_of_BOGs; bog_idx++) {
      LIBBLU_HDMV_CK_IC_DEBUG(" Check BOG %" PRIu8 ".\n", bog_idx);
      HdmvButtonOverlapGroupParameters * bog = &page.bogs[bog_idx];

      for (uint8_t btn_idx = 0; btn_idx < bog->number_of_buttons; btn_idx++) {
        LIBBLU_HDMV_CK_IC_DEBUG("  Check button %" PRIu8 ".\n", btn_idx);
        HdmvButtonParam * btn = &bog->buttons[btn_idx];

        /* normal_state_info */
        HdmvButtonNormalStateInfoParam nsi = btn->normal_state_info;
        if (_checkBtnNSI(nsi, btn->button_id, epoch_state, NULL, NULL, no_display_update) < 0)
          return -1;

        /* selected_state_info */
        HdmvButtonSelectedStateInfoParam ssi = btn->selected_state_info;
        if (_checkBtnSSI(ssi, btn->button_id, epoch_state, NULL, NULL, no_display_update) < 0)
          return -1;

        /* activated_state_info */
        HdmvButtonActivatedStateInfoParam asi = btn->activated_state_info;
        if (_checkBtnASI(asi, btn->button_id, epoch_state, NULL, NULL, no_display_update) < 0)
          return -1;
      }
    }
  }

  /* animation_frame_rate_code */
  // TODO: May animation_frame_rate_code be check to not been greater than
  // frame rate.

  /* default_selected_button_id_ref */
  bool def_selected_button_present  =
    (0xFFFF == page.default_selected_button_id_ref)
    // 0xFFFF == Unspecified (treated as present)
  ;

  /* default_activated_button_id_ref */
  bool def_activated_button_present =
    (0xFFFE <= page.default_activated_button_id_ref)
    // 0xFFFE == Current selected button activated
    // 0xFFFF == Unspecified (both treated as present)
  ;

  /* palette_id_ref */
  // Already checked

  /* number_of_BOGs */
  // Nothing to check

  HdmvCollisionTreeNodePtr bogs_collision_tree = NULL;

  for (uint8_t bog_idx = 0; bog_idx < page.number_of_BOGs; bog_idx++) {
    LIBBLU_HDMV_CK_IC_DEBUG(" Check BOG %" PRIu8 ".\n", bog_idx);
    HdmvButtonOverlapGroupParameters * bog = &page.bogs[bog_idx];

    /* default_valid_button_id_ref */
    bool def_valid_button_present =
      (0xFFFF == bog->default_valid_button_id_ref)
      // 0xFFFF == Unspecified (treated as present)
    ;

    if (page.default_selected_button_id_ref == bog->default_valid_button_id_ref)
      def_selected_button_present = true;

    /* number_of_buttons */
    // Already checked at parsing

    HdmvRectangle bog_rect = emptyRectangle();

    for (uint8_t btn_idx = 0; btn_idx < bog->number_of_buttons; btn_idx++) {
      LIBBLU_HDMV_CK_IC_DEBUG("  Check button %" PRIu8 ".\n", btn_idx);
      HdmvButtonParam * btn = &bog->buttons[btn_idx];

      /* button_id */
      // Range check:
      if (HDMV_MAX_NB_BUTTONS <= btn->button_id)
        LIBBLU_HDMV_CK_ERROR_RETURN(
          "Invalid 'button_id' == 0x04%" PRIX16 " value "
          "in BOG %u of 'page_id' == 0x%02" PRIX8 " "
          "(value shall be 0x0000 through 0x%04" PRIX16 ").\n",
          btn->button_id,
          bog_idx,
          page.page_id,
          HDMV_MAX_NB_BUTTONS-1
        );

      // Uniqueness check:
      if (present_button_id_values[btn->button_id] <= bog_idx)
        LIBBLU_HDMV_CK_ERROR_RETURN(
          "Non-unique 'button_id' == 0x%04" PRIX16 " "
          "in BOG %u of 'page_id' == 0x%02" PRIX8 ".\n",
          btn->button_id,
          bog_idx,
          page.page_id
        );
      present_button_id_values[btn->button_id] = bog_idx; // Set

      if (page.default_activated_button_id_ref == btn->button_id)
        def_activated_button_present = true;
      if (bog->default_valid_button_id_ref      == btn->button_id)
        def_valid_button_present     = true;

      /* button_numeric_select_value */
      if (btn->button_numeric_select_value != 0xFFFF) {
        // If not disabled (0xFFFF)
        uint16_t btn_num_value = btn->button_numeric_select_value;

        // Range check:
        if (HDMV_MAX_BUTTON_NUMERIC_SELECT_VALUE < btn_num_value)
          LIBBLU_HDMV_CK_ERROR_RETURN(
            "Invalid 'button_numeric_select_value' == 0x04%" PRIX16 " value "
            "associated with 'button_id' == 0x%04" PRIX16 " of BOG %u "
            "of 'page_id' == 0x%02" PRIX8 " (value shall be 0 through 9999 "
            "or 0xFFFF).\n",
            btn->button_numeric_select_value,
            btn->button_id,
            bog_idx,
            page.page_id
          );

        // Uniqueness check:
        if (present_button_numeric_select_values[btn_num_value] < bog_idx) {
          LIBBLU_HDMV_CK_ERROR_RETURN(
            "Non-unique 'button_numeric_select_value' == 0x04%" PRIX16 " value "
            "associated with 'button_id' == 0x%04" PRIX16 " of BOG %u "
            "of 'page_id' == 0x%02" PRIX8 " shared with another BOG "
            "of the same page.\n",
            btn->button_numeric_select_value,
            btn->button_id,
            bog_idx,
            page.page_id
          );
        }
        present_button_numeric_select_values[btn_num_value] = bog_idx;
      }

      /* auto_action_flag */
      // Button states are checked

      /* button_horizontal_position */
      /* button_vertical_position */
      // This is checked after button states objects verification

      /* neighbor_info */
      // This is checked after verification of all Button() structures

      uint16_t btn_obj_w = 0;
      uint16_t btn_obj_h = 0;

      /* normal_state_info */
      HdmvButtonNormalStateInfoParam nsi = btn->normal_state_info;
      if (_checkBtnNSI(nsi, btn->button_id, epoch_state, &btn_obj_w, &btn_obj_h, false) < 0)
        return -1;

      /* selected_state_info */
      HdmvButtonSelectedStateInfoParam ssi = btn->selected_state_info;
      if (_checkBtnSSI(ssi, btn->button_id, epoch_state, &btn_obj_w, &btn_obj_h, false) < 0)
        return -1;

      /* activated_state_info */
      HdmvButtonActivatedStateInfoParam asi = btn->activated_state_info;
      if (_checkBtnASI(asi, btn->button_id, epoch_state, &btn_obj_w, &btn_obj_h, false) < 0)
        return -1;

      /* button_horizontal_position */
      if (video_desc.video_width < btn->button_horizontal_position + btn_obj_w)
        LIBBLU_HDMV_CK_ERROR_RETURN(
          "Button horizontal position out of video width limits "
          "('video_width' == %" PRIu16 ", "
          "'button_id' == 0x%04" PRIX16 ", "
          "'button_horizontal_position' == %" PRIu16 ", "
          "button states objects width == %" PRIu16 ").\n",
          video_desc.video_width,
          btn->button_id,
          btn->button_horizontal_position,
          btn_obj_w
        );
      btn->btn_obj_width = btn_obj_w;

      /* button_vertical_position */
      if (video_desc.video_height < btn->button_vertical_position + btn_obj_h)
        LIBBLU_HDMV_CK_ERROR_RETURN(
          "Button vertical position out of video height limits "
          "('video_height' == %" PRIu16 ", "
          "'button_id' == 0x%04" PRIX16 ", "
          "'button_vertical_position' == %" PRIu16 ", "
          "button states objects height == %" PRIu16 ").\n",
          video_desc.video_height,
          btn->button_id,
          btn->button_vertical_position,
          btn_obj_h
        );
      btn->btn_obj_height = btn_obj_h;

      LIBBLU_HDMV_CK_IC_DEBUG(
        "  Button dimensions: x=%" PRIu16 " y=%" PRIu16 " w=%" PRIu16 " h=%" PRIu16 ".\n",
        btn->button_horizontal_position,
        btn->button_vertical_position,
        btn_obj_w,
        btn_obj_h
      );

      bog_rect = mergeRectangle(
        bog_rect, (HdmvRectangle) {
          .x = btn->button_horizontal_position,
          .y = btn->button_vertical_position,
          .w = btn_obj_w,
          .h = btn_obj_h
        }
      );

      /* number_of_navigation_commands */
      /* navigation_commands */
      // TODO
    }

    if (!def_valid_button_present) {
      LIBBLU_HDMV_CK_ERROR_RETURN(
        "Invalid 'default_valid_button_id_ref' == 0x%04" PRIX16 " on "
        "BOG %u of 'page_id' == 0x%02" PRIX8 ", "
        "no such 'button_id' in BOG.\n",
        bog->default_valid_button_id_ref,
        bog_idx,
        page.page_id
      );
    }

    LIBBLU_HDMV_CK_IC_DEBUG(
      "  BOG covered zone: x=%" PRIu16 " y=%" PRIu16 " w=%" PRIu16 " h=%" PRIu16 ".\n",
      bog_rect.x, bog_rect.y, bog_rect.w, bog_rect.h
    );

    if (!isEmptyRectangle(bog_rect)) {
      /* Insert BOG rectangle into collision detection tree */
      HdmvRectangle colliding_bog_rect;
      unsigned colliding_bog_idx;
      int ret = insertHdmvCollisionTree(
        &bogs_collision_tree, bog_rect, bog_idx,
        &colliding_bog_rect,
        &colliding_bog_idx
      );
      if (ret < 0)
        return -1; // Error
      if (0 < ret) {
        destroyHdmvCollisionTree(bogs_collision_tree);
        LIBBLU_HDMV_CK_ERROR_RETURN(
          "At least one button from BOG %u overlaps with a button from BOG %u "
          "in 'page_id' == 0x%02" PRIX8 " "
          "(BOG %u bounding box: x=%" PRIu16 " y=%" PRIu16 " w=%" PRIu16 " h=%" PRIu16 ", "
          "BOG %u bounding box: x=%" PRIu16 " y=%" PRIu16 " w=%" PRIu16 " h=%" PRIu16 ").\n",
          bog_idx,
          colliding_bog_idx,
          page.page_id,
          colliding_bog_idx, colliding_bog_rect.x, colliding_bog_rect.y, colliding_bog_rect.w, colliding_bog_rect.h,
          bog_idx, bog_rect.x, bog_rect.y, bog_rect.w, bog_rect.h
        );
      }
    }
  }

  destroyHdmvCollisionTree(bogs_collision_tree);

  if (!def_selected_button_present) {
    LIBBLU_HDMV_CK_ERROR_RETURN(
      "Invalid 'default_selected_button_id_ref' == 0x%04" PRIX16 " on"
      " 'page_id' == 0x%02" PRIX8 ", no BOG with such "
      "'default_valid_button_id_ref' value.\n",
      page.default_selected_button_id_ref,
      page.page_id
    );
  }

  if (!def_activated_button_present) {
    LIBBLU_HDMV_CK_ERROR_RETURN(
      "Invalid 'default_activated_button_id_ref' == 0x%04" PRIX16 " on"
      " 'page_id' == 0x%02" PRIX8 ", no BOG with such "
      "'default_valid_button_id_ref' value.\n",
      page.default_activated_button_id_ref,
      page.page_id
    );
  }

  /* Checking Neighbor_info(): */
  for (unsigned bog_index = 0; bog_index < page.number_of_BOGs; bog_index++) {
    const HdmvButtonOverlapGroupParameters bog = page.bogs[bog_index];

    for (unsigned btn_index = 0; btn_index < bog.number_of_buttons; btn_index++) {
      const HdmvButtonParam btn = bog.buttons[btn_index];

#define CHECK_NEIGHBOR(nghb_fname)                                            \
      if (btn.neighbor_info.nghb_fname != btn.button_id) {                    \
        uint8_t neighbor_bog_idx = present_button_id_values[                  \
          btn.neighbor_info.nghb_fname                                        \
        ];                                                                    \
        if (bog_index == neighbor_bog_idx) {                                  \
          LIBBLU_HDMV_CK_ERROR_RETURN(                                        \
            "Invalid '" #nghb_fname "' == 0x%04" PRIX16 " on "                \
            "'button_id' == 0x%04" PRIX16 " of BOG %u from "                  \
            "'page_id' == 0x%02" PRIX8 ", "                                   \
            "referenced button is from the same BOG.\n"                       \
          );                                                                  \
        }                                                                     \
      }

      CHECK_NEIGHBOR(upper_button_id_ref);
      CHECK_NEIGHBOR(lower_button_id_ref);
      CHECK_NEIGHBOR(left_button_id_ref);
      CHECK_NEIGHBOR(right_button_id_ref);
#undef CHECK_NEIGHBOR
    }
  }

  return 0;
}

static int _checkParametersStreamConstancyIC(
  const HdmvEpochState * epoch_state,
  const HdmvICParameters inter_def
)
{
  HdmvEpochStateIGStream epoch_ic_param = epoch_state->stream_parameters.ig;
  if (!epoch_ic_param.initialized)
    return 0;

  bool invalid = false;

#define ERROR(format, ...)  \
  do {LIBBLU_HDMV_CK_ERROR(format, ##__VA_ARGS__); invalid = true; } while (0)

  if (epoch_ic_param.stream_model != inter_def.stream_model)
    ERROR(
      "Invalid Interactive Composition, "
      "stream model parameter is different from other compositions "
      "('stream_model' == 0b%x, %s, was 0b%x, %s).\n",
      inter_def.stream_model,
      HdmvStreamModelStr(inter_def.stream_model),
      epoch_ic_param.stream_model,
      HdmvStreamModelStr(epoch_ic_param.stream_model)
    );

  if (epoch_ic_param.user_interface_model != inter_def.user_interface_model)
    ERROR(
      "Invalid Interactive Composition, "
      "stream model parameter is different from other compositions "
      "('stream_model' == 0b%x, %s, was 0b%x, %s).\n",
      inter_def.stream_model,
      HdmvStreamModelStr(inter_def.stream_model),
      epoch_ic_param.stream_model,
      HdmvStreamModelStr(epoch_ic_param.stream_model)
    );

#undef ERROR

  return invalid ? -1 : 0;
}

static void _setIGStreamParametersHdmvEpochState(
  HdmvEpochState * epoch_state,
  const HdmvICParameters inter_def
)
{
  epoch_state->stream_parameters.ig = (HdmvEpochStateIGStream) {
    .stream_model         = inter_def.stream_model,
    .user_interface_model = inter_def.user_interface_model,
    .initialized          = true
  };
}

static int _checkParametersEpochConstancyIC(
  const HdmvICParameters prev_inter_def,
  const HdmvICParameters inter_def
)
{
  bool invalid = false;

#define ERROR(format, ...)  \
  do {LIBBLU_HDMV_CK_ERROR(format, ##__VA_ARGS__); invalid = true; } while (0)

  if (prev_inter_def.stream_model != inter_def.stream_model)
    ERROR(
      "Invalid Interactive Composition, "
      "stream model parameter is different from previous composition "
      "('stream_model' == 0b%x, %s, was 0b%x, %s).\n",
      prev_inter_def.stream_model,
      HdmvStreamModelStr(prev_inter_def.stream_model),
      inter_def.stream_model,
      HdmvStreamModelStr(inter_def.stream_model)
    );

  if (prev_inter_def.user_interface_model != inter_def.user_interface_model)
    ERROR(
      "Invalid Interactive Composition, "
      "stream model parameter is different from previous composition "
      "('stream_model' == 0b%x, %s, was 0b%x, %s).\n",
      prev_inter_def.stream_model,
      HdmvStreamModelStr(prev_inter_def.stream_model),
      inter_def.stream_model,
      HdmvStreamModelStr(inter_def.stream_model)
    );

  if (prev_inter_def.composition_time_out_pts != inter_def.composition_time_out_pts)
    ERROR(
      "Invalid Interactive Composition, composition time out PTS parameter "
      "is different from previous composition "
      "('composition_time_out_pts' == %" PRId64 ", was %" PRId64 ").\n",
      inter_def.composition_time_out_pts,
      prev_inter_def.composition_time_out_pts
    );

  if (prev_inter_def.selection_time_out_pts != inter_def.selection_time_out_pts)
    ERROR(
      "Invalid Interactive Composition, selection time out PTS parameter "
      "is different from previous composition "
      "('selection_time_out_pts' == %" PRId64 ", was %" PRId64 ").\n",
      inter_def.selection_time_out_pts,
      prev_inter_def.selection_time_out_pts
    );

  if (prev_inter_def.user_time_out_duration != inter_def.user_time_out_duration)
    ERROR(
      "Invalid Interactive Composition, user time out duration parameter "
      "is different from previous composition "
      "('user_time_out_duration' == %" PRIu32 ", was %" PRIu32 ").\n",
      inter_def.user_time_out_duration,
      prev_inter_def.user_time_out_duration
    );

  if (prev_inter_def.number_of_pages != inter_def.number_of_pages)
    ERROR(
      "Invalid Interactive Composition, "
      "number of pages is different from previous composition "
      "('number_of_pages' == %" PRIu8 ", was %" PRIu8 ").\n",
      inter_def.number_of_pages,
      prev_inter_def.number_of_pages
    );

#undef ERROR

  return invalid ? -1 : 0;
}

static int _checkAndUpdateIC(
  const HdmvICParameters ic,
  const HdmvEpochState * epoch_state,
  bool no_display_update
)
{
  HdmvICParameters prev_ic;
  int64_t prev_ic_pts;
  bool known_prev_ic = fetchActiveICHdmvEpochState(
    epoch_state, &prev_ic, &prev_ic_pts
  );

  LIBBLU_HDMV_CK_IC_DEBUG("Previously known IC: %s.\n", BOOL_STR(known_prev_ic));
  LIBBLU_HDMV_CK_IC_DEBUG("Check common IC parameters.\n");

  /* interactive_composition_length */
  if (HDMV_MAX_IC_LENGTH < ic.interactive_composition_length + 3u)
    LIBBLU_HDMV_CK_ERROR_RETURN(
      "Invalid Interactive Composition, size exceeds maximum buffer size "
      "('interactive_composition_length' == %" PRIu32 " + 3 bytes for field "
      "size exceeds " STR(HDMV_MAX_IC_LENGTH) " bytes).\n"
    );

  /* stream_model */
  if (HDMV_STREAM_MODEL_OOM == ic.stream_model && known_prev_ic)
    LIBBLU_HDMV_CK_ERROR_RETURN(
      "Invalid stream, "
      "streams with Interactive Composition stream model parameter set as "
      "Out-of-Mux shall be composed of a single Epoch with a single "
      "Display Set.\n"
    );

  /* user_interface_model */
  if (
    HDMV_STREAM_MODEL_MULTIPLEXED == ic.stream_model
    && HDMV_UI_MODEL_NORMAL != ic.user_interface_model
  ) {
    LIBBLU_HDMV_CK_ERROR_RETURN(
      "Invalid Interactive Composition, "
      "multiplexed stream model only allows \"Normal/Always-On\" "
      "UI model ('stream_model' == 0b0, 'user_interface_model' == 0b%x, %s).\n",
      ic.user_interface_model,
      HdmvUserInterfaceModelStr(ic.user_interface_model)
    );
  }

  if (HDMV_STREAM_MODEL_MULTIPLEXED == ic.stream_model) {
    /* composition_time_out_pts */
    // TODO: Warning if composition_time_out_pts non aligned on video grid
    // TODO: Warning if initial_delay is non zero as composition_time_out_pts might be changed

    /* selection_time_out_pts */
    // TODO: Warning if selection_time_out_pts non aligned on video grid
    // TODO: Warning if initial_delay is non zero as selection_time_out_pts might be changed

    /* composition_time_out_pts/selection_time_out_pts relationship */
    if (0 < ic.composition_time_out_pts && 0 < ic.selection_time_out_pts) {
      /**
       * selection_time_out_pts <= composition_time_out_pts
       * if composition_time_out_pts is non-zero.
       */
      if (ic.composition_time_out_pts < ic.selection_time_out_pts)
        LIBBLU_HDMV_CK_ERROR_RETURN(
          "Invalid Interactive Composition, "
          "selection time out PTS if non-zero shall be less than "
          "or equal to composition time out PTS, if non-zero too "
          "('selection_time_out_pts' == %" PRId64 ", "
          "'composition_time_out_pts' == %" PRId64 ").\n",
          ic.selection_time_out_pts,
          ic.composition_time_out_pts
        );
    }
  }

  /* user_time_out_duration */
  // Nothing to check

  if (!no_display_update) {
    LIBBLU_HDMV_CK_IC_DEBUG("Check parameters stream consistency.\n");
    if (_checkParametersStreamConstancyIC(epoch_state, ic) < 0)
      return -1;

    if (known_prev_ic) {
      LIBBLU_HDMV_CK_IC_DEBUG("Check parameters Epoch consistency.\n");
      if (_checkParametersEpochConstancyIC(prev_ic, ic) < 0)
        return -1;
    }
  }
  else { // no_display_update
    assert(known_prev_ic);
    LIBBLU_HDMV_CK_IC_DEBUG("Check parameters Epoch consistency.\n");
    if (_checkParametersEpochConstancyIC(prev_ic, ic) < 0)
      return -1;
  }

  for (uint8_t page_idx = 0; page_idx < ic.number_of_pages; page_idx++) {
    LIBBLU_HDMV_CK_IC_DEBUG("Check page %" PRIu8 ":\n", page_idx);
    if (_checkHdmvICPage(ic, page_idx, epoch_state, no_display_update) < 0)
      return -1;
  }

  return 0;
}

static int _setUsageODS(
  HdmvSeqIndexer * ods_indexer,
  uint32_t object_id_ref,
  HdmvODSUsage usage
)
{
  if (0xFFFF == object_id_ref)
    return 0; // No object

  assert(ODS_UNUSED_PAGE0 < usage);

  HdmvSequencePtr seq;
  if (getODSSeqIndexer(ods_indexer, object_id_ref, &seq) < 0)
    LIBBLU_HDMV_CK_ERROR_RETURN("Unable to set object usage.\n");

  seq->ods_usage = MAX(seq->ods_usage, usage); // Usage priority
  return 0;
}

static int _setUsageODSSequence(
  HdmvSeqIndexer * ods_indexer,
  uint16_t start_object_id_ref,
  uint16_t end_object_id_ref,
  HdmvODSUsage usage
)
{
  if (0xFFFF == start_object_id_ref)
    return 0; // No object

  uint16_t start, end;
  if (start_object_id_ref <= end_object_id_ref)
    start = start_object_id_ref, end = end_object_id_ref;
  else
    start = end_object_id_ref, end = start_object_id_ref;

  for (uint16_t object_id_ref = start; object_id_ref <= end; object_id_ref++) {
    if (_setUsageODS(ods_indexer, object_id_ref, usage) < 0)
      return -1;
  }

  return 0;
}

static int _checkICODSOrderDSState(
  HdmvDSState * ds,
  const HdmvICParameters ic
)
{
  if (0 == ic.number_of_pages) {
    LIBBLU_HDMV_CK_DEBUG("    No page.\n");
    return 0;
  }

  /* Index all ODS from the DS by their 'object_id' */
  HdmvSeqIndexer ods_indexer;
  initHdmvSeqIndexer(&ods_indexer);

  for (
    HdmvSequencePtr seq = getFirstODSSequenceHdmvDSState(ds);
    NULL != seq;
    seq = seq->next_sequence
  ) {
    uint16_t object_id = seq->data.od.object_descriptor.object_id;
    if (putHdmvSeqIndexer(&ods_indexer, object_id, seq) < 0)
      goto free_return;
  }

  /* Find usage type of each ODS in first page */
  const HdmvPageParameters page = ic.pages[0];

  /* Mark objects used in in_effects() of page 0 */
  const HdmvEffectSequenceParameters in_effects = page.in_effects;
  for (uint8_t effect_i = 0; effect_i < in_effects.number_of_effects; effect_i++) {
    const HdmvEffectInfoParameters eff = in_effects.effects[effect_i];
    for (uint8_t co_i = 0; co_i < eff.number_of_composition_objects; co_i++) {
      const HdmvCOParameters co = eff.composition_objects[co_i];
      if (_setUsageODS(&ods_indexer, co.object_id_ref, ODS_IN_EFFECT_PAGE0) < 0)
        goto free_return;
    }
  }

  /* Mark objects used in out_effects() of page 0 */
  const HdmvEffectSequenceParameters out_effects = page.out_effects;
  for (uint8_t effect_i = 0; effect_i < out_effects.number_of_effects; effect_i++) {
    const HdmvEffectInfoParameters eff = out_effects.effects[effect_i];
    for (uint8_t co_i = 0; co_i < eff.number_of_composition_objects; co_i++) {
      const HdmvCOParameters co = eff.composition_objects[co_i];
      if (_setUsageODS(&ods_indexer, co.object_id_ref, ODS_USED_OTHER_PAGE0) < 0)
        goto free_return;
    }
  }

  /* Mark objects used in any button() of page 0 */
  for (uint8_t bog_i = 0; bog_i < page.number_of_BOGs; bog_i++) {
    const HdmvButtonOverlapGroupParameters bog = page.bogs[bog_i];
    for (uint8_t btn_i = 0; btn_i < bog.number_of_buttons; btn_i++) {
      const HdmvButtonParam btn = bog.buttons[btn_i];

      bool is_def_valid_btn = (bog.default_valid_button_id_ref     == btn.button_id);
      bool is_def_sel_btn   = (page.default_selected_button_id_ref == btn.button_id);

      uint16_t start_obj_id_ref, end_obj_id_ref;
      HdmvODSUsage usage;

      /* Objects used in normal_state_info */
      start_obj_id_ref = btn.normal_state_info.start_object_id_ref;
      end_obj_id_ref   = btn.normal_state_info.end_object_id_ref;

      usage = (is_def_valid_btn ? ODS_DEFAULT_NORMAL_PAGE0 : ODS_USED_OTHER_PAGE0);

      if (_setUsageODSSequence(&ods_indexer, start_obj_id_ref, end_obj_id_ref, usage) < 0)
        goto free_return;

      /* Objects used in selected_state_info */
      start_obj_id_ref = btn.selected_state_info.start_object_id_ref;
      end_obj_id_ref   = btn.selected_state_info.end_object_id_ref;

      usage = (is_def_valid_btn ? ODS_DEFAULT_SELECTED_PAGE0 : ODS_USED_OTHER_PAGE0);

      if (_setUsageODSSequence(&ods_indexer, start_obj_id_ref, end_obj_id_ref, usage) < 0)
        goto free_return;

      /* First object displayed from the default selected button */
      if (is_def_sel_btn) {
        if (_setUsageODS(&ods_indexer, start_obj_id_ref, ODS_DEFAULT_FIRST_SELECTED_PAGE0) < 0)
          goto free_return;
      }

      /* Objects used in activated_state_info */
      start_obj_id_ref = btn.activated_state_info.start_object_id_ref;
      end_obj_id_ref   = btn.activated_state_info.end_object_id_ref;

      usage = ODS_USED_OTHER_PAGE0;

      if (_setUsageODSSequence(&ods_indexer, start_obj_id_ref, end_obj_id_ref, usage) < 0)
        goto free_return;
    }
  }

  /* Check order */
  HdmvSequencePtr seq = getFirstODSSequenceHdmvDSState(ds);
  HdmvODSUsage expect_max_usage = ODS_IN_EFFECT_PAGE0;

  for (
    unsigned ods_i = 0;
    NULL != seq;
    seq = seq->next_sequence, ods_i++
  ) {
    uint16_t object_id = seq->data.od.object_descriptor.object_id;

    LIBBLU_HDMV_CK_DEBUG(
      "    - ODS #%u ('object_id' == 0x%04" PRIX16 "): %s.\n",
      ods_i, object_id,
      HdmvODSUsageStr(seq->ods_usage)
    );

    if (expect_max_usage < seq->ods_usage)
      LIBBLU_ERROR_FRETURN(
        "Out of order ODS, expect object(s) '%s' to not be after objects "
        "marked as '%s' (problematic ODS #%u with 'object_id' == 0x%04" PRIX16 ").\n",
        HdmvODSUsageStr(seq->ods_usage),
        HdmvODSUsageStr(expect_max_usage),
        expect_max_usage,
        ods_i, object_id
      );

    expect_max_usage = MIN(expect_max_usage, seq->ods_usage);
  }

  cleanHdmvSeqIndexer(ods_indexer);
  return 0;

free_return:
  cleanHdmvSeqIndexer(ods_indexer);
  return -1;
}

static int _checkAndUpdateICHdmvDSState(
  HdmvDSState * ds,
  HdmvEpochState * epoch_state,
  int64_t pres_time
)
{
  LIBBLU_HDMV_CK_DEBUG("   Checking Interactive Composition Segments (ICS).\n");

  HdmvSequencePtr ics_seq = getICSSequenceHdmvDSState(ds);
  if (NULL == ics_seq)
    return -1;

  assert(HDMV_SEGMENT_TYPE_ICS == ics_seq->type);
  HdmvICParameters ic = ics_seq->data.ic;

  if (_checkAndUpdateIC(ic, epoch_state, false) < 0)
    return -1;

  if (setActiveICHdmvEpochState(epoch_state, ic, pres_time) < 0)
    return -1;
  _setIGStreamParametersHdmvEpochState(epoch_state, ic);

  LIBBLU_HDMV_CK_DEBUG("   Checking Display Set ODS order.\n");
  if (_checkICODSOrderDSState(ds, ic) < 0)
    return -1;

  return 0;
}

static int _checkAndUpdateIGHdmvDSState(
  HdmvDSState * ds,
  HdmvEpochState * epoch_state,
  int64_t pres_time
)
{
  /* PDS */
  if (_checkAndUpdatePDHdmvDSState(ds, epoch_state) < 0)
    return -1;

  /* ODS */
  if (_checkAndUpdateODHdmvDSState(ds, epoch_state) < 0)
    return -1;

  /* ICS */
  if (_checkAndUpdateICHdmvDSState(ds, epoch_state, pres_time) < 0)
    return -1;

  return 0;
}

static int _checkWD(
  const HdmvWDParameters window_definition,
  const HdmvVDParameters video_desc
)
{

  /* number_of_windows */ // Already checked

  HdmvRectangle win_rect[HDMV_MAX_NB_WDS_WINDOWS];

  for (
    uint8_t win_idx = 0;
    win_idx < window_definition.number_of_windows;
    win_idx++
  ) {
    /* Window() */
    const HdmvWindowInfoParameters win = window_definition.windows[win_idx];

    /* window_id */
    if (win_idx != win.window_id)
      LIBBLU_HDMV_CK_ERROR_RETURN(
        "Unexpected effect sequence non sequential window id "
        "(got 0x%02" PRIX8 ", expect 0x%02" PRIX8 ").\n",
        win.window_id,
        win_idx
      );

    if (_checkWindowInfo(win, video_desc, win_rect, win_idx) < 0)
      return -1;
  }

  return 0;
}

static int _checkAndUpdateWDHdmvDSState(
  HdmvDSState * ds,
  HdmvEpochState * epoch_state
)
{
  LIBBLU_HDMV_CK_DEBUG("   Checking Window Definition Segment (WDS).\n");

  HdmvSequencePtr wds_seq = getWDSSequenceHdmvDSState(ds);
  if (NULL == wds_seq) {
    // Absence of WDS is only allowed when 'palette_update_flag' of PCS is
    // set to 0b1 (this last condition is checked further in PCS checks)

    if (!isNormalHdmvCDParameters(ds->compo_desc))
      LIBBLU_HDMV_CK_ERROR_RETURN(
        "Unexpected missing Window Definition Segment from a non-Normal "
        "composition state Display Set.\n"
      );

    assert(epoch_state->window_def_present);
    epoch_state->window_def_provided = false;
    return 0;
  }

  assert(HDMV_SEGMENT_TYPE_WDS == wds_seq->type);
  HdmvWDParameters wd = wds_seq->data.wd;

  if (_checkWD(wd, epoch_state->video_descriptor) < 0)
    return -1;

  if (!isEpochStartHdmvCDParameters(ds->compo_desc)) {
    HdmvWDParameters prev_wd;
    lb_assert(fetchCurrentWDHdmvEpochState(epoch_state, false, &prev_wd));
    if (!constantHdmvWDParameters(prev_wd, wd))
      LIBBLU_HDMV_CK_ERROR_RETURN(
        "Invalid Window Definition Segment, "
        "definition shall remain the same accross Epoch.\n"
      );
  }

  setWDHdmvEpochState(epoch_state, wd);
  return 0;
}

static int _checkPC(
  const HdmvPCParameters pc,
  HdmvEpochState * epoch_state,
  bool no_display_update
)
{

  /* palette_update_flag */
  // Checked at DS level

  /* palette_id_ref */
  bool updated_palette;
  if (!fetchPaletteHdmvEpochState(epoch_state, pc.palette_id_ref, false, NULL, &updated_palette))
    LIBBLU_HDMV_CK_ERROR_RETURN(
      "Invalid Presentation Composition, "
      "referenced Palette ID is unknown "
      "('palette_id_ref' == 0x%02" PRIX8 " does not match any 'palette_id' "
      "or has not been supplied since last Acquistion Point).\n",
      pc.palette_id_ref
    );

  if (no_display_update && updated_palette)
    LIBBLU_HDMV_CK_ERROR_RETURN(
      "Invalid non-Display Update Presentation Composition, "
      "the referenced palette has been updated although the composition "
      "does not describe a Display Update "
      "('palette_id_ref' == 0x%02" PRIX8 ").",
      pc.palette_id_ref
    );

  /* number_of_composition_objects */ // Already checked

  HdmvWDParameters wd;
  if (!fetchCurrentWDHdmvEpochState(epoch_state, !pc.palette_update_flag, &wd))
    LIBBLU_HDMV_CK_ERROR_RETURN(
      "Invalid Display Set, "
      "Window Definition Segment must be provided except for palette update "
      "only Display Update.\n"
    );

  const uint8_t nb_win = wd.number_of_windows;
  const HdmvWindowInfoParameters * win = wd.windows;
  HdmvRectangle co_rect[HDMV_MAX_NB_PC_COMPO_OBJ];

  for (unsigned idx = 0; idx < pc.number_of_composition_objects; idx++) {
    /* Composition_object() */
    const HdmvCOParameters co = pc.composition_objects[idx];
    if (_checkCO(co, win, nb_win, epoch_state, false, no_display_update, &co_rect[idx]) < 0)
      return -1;
    if (_checkNonCollidingCO(co_rect, idx) < 0)
      return -1;
  }

  return 0;
}

static int _checkAndUpdatePCHdmvDSState(
  HdmvDSState * ds,
  HdmvEpochState * epoch_state,
  int64_t pres_time
)
{
  LIBBLU_HDMV_CK_DEBUG("   Checking Presentation Composition Segment (PCS).\n");

  HdmvSequencePtr pcs_seq = getPCSSequenceHdmvDSState(ds);
  if (NULL == pcs_seq)
    return -1;

  /* Check number of active Presentation Compositions */
  int64_t oldest_pc_pts;
  if (fetchOldestPCHdmvEpochState(epoch_state, NULL, &oldest_pc_pts)) {
    /**
     * This PCS might cause a disruption to an active Presentation Composition.
     * Up to 8 compositions might be simultaneously active (decoded PCs but
     * whose PTS Display Update timestamp has not yet been reached). This new
     * composition must not overwrite a composition that is still active, so
     * its DTS timestamp must be greater than or equal to the PTS of the 8th
     * oldest composition.
     */
    pcs_seq->earliest_available_dts = oldest_pc_pts;
    pcs_seq->has_earliest_available_dts = true;
  }

  assert(HDMV_SEGMENT_TYPE_PCS == pcs_seq->type);
  HdmvPCParameters pc = pcs_seq->data.pc;

  /* palette_update_flag */
  if (pc.palette_update_flag && !isNormalHdmvCDParameters(ds->compo_desc))
    LIBBLU_HDMV_CK_ERROR_RETURN(
      "Invalid Presentation Composition, "
      "palette only Display Updates are only allowed for DS with "
      "Normal composition state "
      "('palette_update_flag' == 0b1, "
      "'composition_state' == %s).\n",
      HdmvCompositionStateStr(ds->compo_desc.composition_state)
    );

  if (_checkPC(pc, epoch_state, false) < 0)
    return -1;
  setActivePCHdmvEpochState(epoch_state, pc, pres_time);

  return 0;
}

static int _checkAndUpdatePGHdmvDSState(
  HdmvDSState * ds,
  HdmvEpochState * epoch_state,
  int64_t pres_time
)
{
  /* PDS */
  if (_checkAndUpdatePDHdmvDSState(ds, epoch_state) < 0)
    return -1;

  /* ODS */
  if (_checkAndUpdateODHdmvDSState(ds, epoch_state) < 0)
    return -1;

  /* WDS */
  if (_checkAndUpdateWDHdmvDSState(ds, epoch_state) < 0)
    return -1;

  /* PCS */
  if (_checkAndUpdatePCHdmvDSState(ds, epoch_state, pres_time) < 0)
    return -1;

  return 0;
}

int checkAndUpdateHdmvDSState(
  HdmvDSState * ds,
  HdmvEpochState * epoch_state,
  int64_t pres_time
)
{
  LIBBLU_HDMV_CK_DEBUG("  Check and update Display Set state.\n");
  switch (epoch_state->type) {
  case HDMV_STREAM_TYPE_IGS:
    return _checkAndUpdateIGHdmvDSState(ds, epoch_state, pres_time);
  case HDMV_STREAM_TYPE_PGS:
    return _checkAndUpdatePGHdmvDSState(ds, epoch_state, pres_time);
  }

  LIBBLU_TODO_MSG("Unreachable\n");
}

int checkObjectsBufferingHdmvEpochState(
  HdmvEpochState * epoch_state
)
{
  uint32_t decoded_obj_buf_usage = 0u;

  LIBBLU_HDMV_CK_DEBUG("  Checking objects buffer usage:\n");
  LIBBLU_HDMV_CK_DEBUG("     Compressed size => Uncompressed size.\n");

  /* Check all Objects in memory */
  unsigned max_nb_obj = getHdmvMaxNbObj(epoch_state->type);

  for (unsigned obj_id = 0; obj_id < max_nb_obj; obj_id++) {
    HdmvEpochStateObject obj_state = epoch_state->objects[obj_id];
    if (HDMV_DEF_NEVER_PROVIDED == obj_state.state)
      continue;

    HdmvODParameters od = obj_state.def;
    uint32_t obj_coded_size   = od.object_data_length;
    uint32_t obj_decoded_size = od.object_width * od.object_height;

    LIBBLU_HDMV_CK_DEBUG(
      "   - Object %u: %ux%u, %" PRIu32 " byte(s) => %" PRIu32 " byte(s).\n",
      od.object_descriptor.object_id,
      od.object_width,
      od.object_height,
      obj_coded_size,
      obj_decoded_size
    );

    decoded_obj_buf_usage += obj_decoded_size;
  }

  uint32_t dec_obj_buf_size = getHDMVDecodedObjectBufferSize(epoch_state->type);
  LIBBLU_HDMV_CK_DEBUG(
    "    => Decoded Object Buffer (DB) usage: "
    "%" PRIu32 " bytes / %" PRIu32 " bytes.\n",
    decoded_obj_buf_usage,
    dec_obj_buf_size
  );

  if (dec_obj_buf_size < decoded_obj_buf_usage)
    LIBBLU_HDMV_CK_ERROR_RETURN(
      "Decoded Object Buffer (DB) overflows, "
      "%" PRIu32 " bytes used out of buffer size of %" PRIu32 " bytes.\n",
      decoded_obj_buf_usage,
      dec_obj_buf_size
    );

  return 0;
}

int checkDuplicatedDSHdmvDSState(
  const HdmvDSState * prev_ds_state,
  HdmvDSState * cur_ds_state,
  HdmvEpochState * epoch_state
)
{
  {
    HdmvCDParameters prev_cd = prev_ds_state->compo_desc;
    HdmvCDParameters cur_cd  = cur_ds_state->compo_desc;
    assert(prev_cd.composition_number == cur_cd.composition_number);
    assert(HDMV_COMPO_STATE_EPOCH_START != cur_cd.composition_state);
    assert(HDMV_COMPO_STATE_EPOCH_CONTINUE != cur_cd.composition_state);
  }

  /* PDS */
  if (_checkDuplicatedPDHdmvDSState(prev_ds_state, cur_ds_state, epoch_state) < 0)
    return -1;

  /* ODS */
  if (_checkDuplicatedODHdmvDSState(prev_ds_state, cur_ds_state, epoch_state) < 0)
    return -1;

  // TODO

  // FIXME: Does players support non-referenced palettes/objects update in
  // a DS with a non-Display Update composition?
  // See _checkDuplicatedPDHdmvDSState/_checkDuplicatedODHdmvDSState
  return 0;
}
