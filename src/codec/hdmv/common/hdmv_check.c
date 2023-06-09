#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>

#include "hdmv_check.h"

static int _checkAndCollectPDSHdmvEpochState(
  HdmvEpochState * epoch,
  HdmvSequencePtr * dsPdsQueue,
  unsigned dsIdx,
  uint8_t palette_id_ref
)
{

  if (0xFF == palette_id_ref) // No palette attached
    return 0;

  HdmvSequencePtr pds = getSequenceByIdxHdmvEpochState(epoch, HDMV_SEGMENT_TYPE_PDS_IDX);
  for (; NULL != pds; pds = pds->nextSequence) {
    if (pds->data.pd.palette_descriptor.palette_id == palette_id_ref)
      break;
  }

  if (NULL == pds) {
    LIBBLU_HDMV_CK_ERROR_RETURN(
      "Unexpected unknown palette_id_ref 0x%02" PRIX8 ".\n",
      palette_id_ref
    );
  }

  if (pds->displaySetIdx != dsIdx) // PDS is from a previous DS.
    return 0;
  if (NULL != pds->nextSequenceDS) // Palette is already referenced (in list).
    return 0;

  if (NULL == *dsPdsQueue) {
    setDSSequenceByIdxHdmvEpochState(epoch, HDMV_SEGMENT_TYPE_PDS_IDX, pds);
  }
  else {
    if (*dsPdsQueue == pds) // Palette is already referenced (at list queue).
      return 0;
    (*dsPdsQueue)->nextSequenceDS = pds;
  }
  *dsPdsQueue = pds;

  return 0;
}

static void _collectODSEpoch(
  HdmvEpochState * epoch,
  HdmvSequencePtr * listTail,
  const HdmvSequencePtr ods,
  bool orderByValue
)
{

  if (NULL != ods->nextSequenceDS) // Object is already referenced.
    return;

  if (NULL == *listTail) { // Empty queue
    setDSSequenceByIdxHdmvEpochState(epoch, HDMV_SEGMENT_TYPE_ODS_IDX, ods);
    *listTail = ods;
    return;
  }

  if (*listTail == ods) // Object is already referenced (at list tail).
    return;

  if (
    orderByValue
    && (
      ods->data.od.object_descriptor.object_id
      < (*listTail)->data.od.object_descriptor.object_id
    )
  ) {
    // Order by value, only if object_id is greater than list tail.

    HdmvSequencePtr prev, cur;
    prev = NULL;
    cur = getDSSequenceByIdxHdmvEpochState(epoch, HDMV_SEGMENT_TYPE_ODS_IDX);
    for (; NULL != cur; cur = cur->nextSequenceDS) {
      if (ods->data.od.object_descriptor.object_id < cur->data.od.object_descriptor.object_id) {
        break;
      }
      prev = cur;
    }

    if (NULL == prev) {
      // Insert as list head.
      setDSSequenceByIdxHdmvEpochState(epoch, HDMV_SEGMENT_TYPE_ODS_IDX, ods);
    }
    else {
      // Insert in middle.
      prev->nextSequenceDS = ods;
    }

    assert(NULL == ods->nextSequenceDS);

    ods->nextSequenceDS = cur;
    return;
  }

  assert(NULL == ods->nextSequenceDS);

  // Insert as list tail.
  (*listTail)->nextSequenceDS = ods;
  *listTail = ods;
}

/** \~english
 * \brief
 *
 * \param epoch
 * \param dsOdsQueue
 * \param dsIdx
 * \param start_object_id_ref
 * \param end_object_id_ref
 * \param objWidthRet Optional object width check and return.
 * \param objHeightRet Optional object height check and return.
 * \return int
 *
 * If objWidthRet (or respectively objHeightRet) is not NULL, supplied pointer
 * is used to return objects width (respectively height).
 */
static int _checkAndCollectODSHdmvEpochState(
  HdmvEpochState * epoch,
  HdmvSequencePtr * dsOdsQueue,
  unsigned dsIdx,
  uint16_t start_object_id_ref,
  uint16_t end_object_id_ref,
  uint16_t * objWidthRet,
  uint16_t * objHeightRet,
  bool orderByValue
)
{
  if (0xFFFF == start_object_id_ref)  {
    // No object attached
    if (NULL != objWidthRet)
      *objWidthRet = 0;
    if (NULL != objHeightRet)
      *objHeightRet = 0;
    return 0;
  }

  if (0xFFFF == end_object_id_ref) {
    LIBBLU_HDMV_CK_ERROR_RETURN(
      "Invalid 'end_object_id_ref' (= 0x%04" PRIX16 ") "
      "< 'start_object_id_ref' (= 0x%04" PRIX16 ").\n",
      start_object_id_ref,
      end_object_id_ref
    );
  }

  uint16_t start, end;
  if (start_object_id_ref <= end_object_id_ref)
    start = start_object_id_ref, end = end_object_id_ref;
  else
    start = end_object_id_ref, end = start_object_id_ref;

  uint16_t width = 0, height = 0;
  HdmvSequencePtr odsList = getSequenceByIdxHdmvEpochState(epoch, HDMV_SEGMENT_TYPE_ODS_IDX);
  for (uint16_t id = start; id <= end; id++) {
    HdmvSequencePtr ods;

    for (ods = odsList; NULL != ods; ods = ods->nextSequence) {
      if (ods->data.od.object_descriptor.object_id == id)
        break;
    }

    if (NULL == ods) {
      LIBBLU_HDMV_CK_ERROR_RETURN(
        "Unexpected unknown 'object_id' == 0x%04" PRIX16 " "
        "(0x%04" PRIX16 "-0x%04" PRIX16 ").\n",
        id,
        start_object_id_ref,
        end_object_id_ref
      );
    }

    /* Check objects dimensions uniformization */
    if (!width) {
      width = ods->data.od.object_width;
      height = ods->data.od.object_height;
    }
    else {
      const HdmvODParameters objdef = ods->data.od;

      if (width != objdef.object_width || height != objdef.object_height)
        LIBBLU_HDMV_CK_ERROR_RETURN(
          "Objects sequence dimensions mismatch "
          "(from %" PRIu16 "x%" PRIu16 " on 'object_id' 0x%04" PRIX16
          " to %" PRIu16 "x%" PRIu16 " on 'object_id' 0x%04" PRIX16 ").\n"
        );
    }

    if (ods->displaySetIdx != dsIdx) // ODS is from a previous DS.
      continue;

    _collectODSEpoch(epoch, dsOdsQueue, ods, orderByValue);
  }

  if (NULL != objWidthRet)
    *objWidthRet = width;
  if (NULL != objHeightRet)
    *objHeightRet = height;

  return 0;
}

static int _checkAndCollectUniqueODSHdmvEpochState(
  HdmvEpochState * epoch,
  HdmvSequencePtr * dsOdsQueue,
  unsigned dsIdx,
  uint16_t object_id_ref,
  uint16_t * objWidthRet,
  uint16_t * objHeightRet,
  bool orderByValue
)
{
  return _checkAndCollectODSHdmvEpochState(
    epoch,
    dsOdsQueue,
    dsIdx,
    object_id_ref,
    object_id_ref,
    objWidthRet,
    objHeightRet,
    orderByValue
  );
}

static int _getAndCollectPresentationCompositionHdmvEpochState(
  HdmvEpochState * epoch,
  HdmvPCParameters * presentation_composition
)
{
  HdmvSequencePtr sequence;

  sequence = getLastSequenceByIdxHdmvEpochState(epoch, HDMV_SEGMENT_TYPE_PCS_IDX);
  if (NULL == sequence)
    LIBBLU_HDMV_CK_ERROR_RETURN("Unexpected missing PCS.\n");

  setDSSequenceByIdxHdmvEpochState(epoch, HDMV_SEGMENT_TYPE_PCS_IDX, sequence);

  *presentation_composition = sequence->data.pc;
  return 0;
}

static int _getAndCollectWindowDefinitionHdmvEpochState(
  HdmvEpochState * epoch,
  HdmvWDParameters * window_definition,
  unsigned dsIdx
)
{
  HdmvSequencePtr sequence;

  sequence = getLastSequenceByIdxHdmvEpochState(epoch, HDMV_SEGMENT_TYPE_WDS_IDX);
  if (NULL == sequence)
    LIBBLU_HDMV_CK_ERROR_RETURN("Unexpected missing WDS.\n");

  if (dsIdx == sequence->displaySetIdx) {
    // WDS is from current DS.
    setDSSequenceByIdxHdmvEpochState(epoch, HDMV_SEGMENT_TYPE_WDS_IDX, sequence);
  }

  *window_definition = sequence->data.wd;

  return 0;
}

static int _checkHdmvWindowDefinitionParameters(
  HdmvVDParameters video_desc,
  HdmvWDParameters definition
)
{

  for (unsigned i = 0; i < definition.number_of_windows; i++) {
    const HdmvWindowInfoParameters * infos = definition.windows[i];

    /* window_id */
    for (unsigned j = 0; j < i; j++) {
      if (infos->window_id == definition.windows[j]->window_id) {
        LIBBLU_HDMV_CK_ERROR_RETURN(
          "Non-unique window_id == 0x%02" PRIX8 " in WDS.\n",
          infos->window_id
        );
      }
    }

    /* window_horizontal_position/window_width */
    if (video_desc.video_width < infos->window_horizontal_position + infos->window_width) {
      LIBBLU_HDMV_CK_ERROR_RETURN(
        "Out of graphical plane window 0x%02" PRIX8 " horizontal "
        "position/width, video_width = %u pixels, "
        "window horizontal_position (= %u pixels) + width (= %u pixels) "
        "= %u pixels.\n",
        infos->window_id,
        video_desc.video_width,
        infos->window_horizontal_position,
        infos->window_width,
        infos->window_horizontal_position + infos->window_width
      );
    }

    /* window_vertical_position/window_height */
    if (video_desc.video_height < infos->window_vertical_position + infos->window_height) {
      LIBBLU_HDMV_CK_ERROR_RETURN(
        "Out of graphical plane window 0x%02" PRIX8 " vertical "
        "position/height, video_height = %u pixels, "
        "window vertical_position (= %u pixels) + height (= %u pixels) "
        "= %u pixels.\n",
        infos->window_id,
        video_desc.video_width,
        infos->window_vertical_position,
        infos->window_height,
        infos->window_vertical_position + infos->window_height
      );
    }
  }

  return 0;
}

static int _getAndCollectInteractiveCompositionHdmvEpochState(
  HdmvEpochState * epoch,
  HdmvICParameters * interactive_composition
)
{
  HdmvSequencePtr sequence;

  sequence = getLastSequenceByIdxHdmvEpochState(epoch, HDMV_SEGMENT_TYPE_ICS_IDX);
  if (NULL == sequence)
    LIBBLU_HDMV_CK_ERROR_RETURN("Unexpected missing ICS.\n");

  setDSSequenceByIdxHdmvEpochState(epoch, HDMV_SEGMENT_TYPE_ICS_IDX, sequence);

  *interactive_composition = sequence->data.ic;
  return 0;
}

static int _checkAndCollectEndSequenceHdmvEpochState(
  HdmvEpochState * epoch
)
{
  HdmvSequencePtr sequence;

  sequence = getLastSequenceByIdxHdmvEpochState(epoch, HDMV_SEGMENT_TYPE_END_IDX);
  if (NULL == sequence)
    LIBBLU_HDMV_CK_ERROR_RETURN("Unexpected missing END.\n");

  setDSSequenceByIdxHdmvEpochState(epoch, HDMV_SEGMENT_TYPE_END_IDX, sequence);
  return 0;
}

static int _checkHdmvWindowInfoParameters(
  HdmvVDParameters video_desc,
  const HdmvWindowInfoParameters * window
)
{
  /* window_id */ // Already checked

  /* window_horizontal_position */
  if (video_desc.video_width <= window->window_horizontal_position)
    LIBBLU_HDMV_CK_ERROR_RETURN(
      "Effects window (id: 0x%02" PRIX8 ") 'window_horizontal_position' is out "
      "of video dimensions (%u <= %u).\n",
      window->window_id,
      video_desc.video_width,
      window->window_horizontal_position
    );

  /* window_vertical_position */
  if (video_desc.video_height <= window->window_vertical_position)
    LIBBLU_HDMV_CK_ERROR_RETURN(
      "Effects window (id: 0x%02" PRIX8 ") 'window_vertical_position' is out "
      "of video dimensions (%u <= %u).\n",
      window->window_id,
      video_desc.video_height,
      window->window_vertical_position
    );

  /* window_width */
  if (video_desc.video_width - window->window_horizontal_position < window->window_width)
    LIBBLU_HDMV_CK_ERROR_RETURN(
      "Effects window (id: 0x%02" PRIX8 ") 'window_width' is out "
      "of video dimensions (%u < %u).\n",
      window->window_id,
      video_desc.video_width - window->window_horizontal_position,
      window->window_width
    );

  /* window_height */
  if (video_desc.video_height - window->window_vertical_position < window->window_height)
    LIBBLU_HDMV_CK_ERROR_RETURN(
      "Effects window (id: 0x%02" PRIX8 ") 'window_height' is out "
      "of video dimensions (%u < %u).\n",
      window->window_id,
      video_desc.video_height - window->window_vertical_position,
      window->window_height
    );

  return 0;
}

static int _checkWindowIdPresenceHdmvEffectSequenceParameters(
  HdmvEffectSequenceParameters effects_sequence,
  uint8_t window_id_ref,
  HdmvWindowInfoParameters * infos
)
{

  for (unsigned i = 0; i < effects_sequence.number_of_windows; i++) {
    const HdmvWindowInfoParameters * window = effects_sequence.windows[i];

    if (window_id_ref == window->window_id) {
      /* Window id match */
      *infos = *window;
      return 0;
    }
  }

  LIBBLU_HDMV_CK_ERROR_RETURN(
    "Unknown window_id_ref == 0x%02" PRIX8 ", no such id in the epoch WDS.\n",
    window_id_ref
  );
}

static int _checkHdmvEffectInfoParameters(
  HdmvEpochState * epoch,
  HdmvSequencePtr * pdsQueue,
  HdmvSequencePtr * odsQueue,
  HdmvEffectSequenceParameters effects_sequence,
  const HdmvEffectInfoParameters * effect_info,
  unsigned dsIdx,
  bool orderByValue
)
{
  /* effect_duration */ // Unchecked

  /* palette_id_ref */
  if (_checkAndCollectPDSHdmvEpochState(epoch, pdsQueue, dsIdx, effect_info->palette_id_ref) < 0)
    return -1;

  /* composition_objects */
  for (unsigned idx = 0; idx < effect_info->number_of_composition_objects; idx++) {
    const HdmvCompositionObjectParameters * obj = effect_info->composition_objects[idx];

    /* object_id_ref */
    uint16_t object_width, object_height;
    int ret = _checkAndCollectUniqueODSHdmvEpochState(
      epoch, odsQueue, dsIdx,
      obj->object_id_ref,
      &object_width,
      &object_height,
      orderByValue
    );
    if (ret < 0)
      return -1;

    /* window_id_ref */
    HdmvWindowInfoParameters window;
    ret = _checkWindowIdPresenceHdmvEffectSequenceParameters(
      effects_sequence,
      obj->window_id_ref,
      &window
    );
    if (ret < 0)
      return -1;

    /* object_cropped_flag */
    /* forced_on_flag */
    // Nothing to check

    /* composition_object_horizontal_position */
    uint16_t horizontal_position = obj->composition_object_horizontal_position;
    if (horizontal_position < window.window_horizontal_position) {
      LIBBLU_HDMV_CK_ERROR_RETURN(
        "Out of effect window 0x%02" PRIX8 " effect composition %u, "
        "horizontal position of object (%" PRIu16 " pixels) "
        "is smaller than window horizontal position (%" PRIu16 " pixels).\n",
        obj->window_id_ref,
        idx,
        horizontal_position,
        window.window_horizontal_position
      );
    }

    /* composition_object_vertical_position */
    uint16_t vertical_position = obj->composition_object_vertical_position;
    if (vertical_position < window.window_vertical_position) {
      LIBBLU_HDMV_CK_ERROR_RETURN(
        "Out of effect window 0x%02" PRIX8 " effect composition %u, "
        "vertical position of object (%" PRIu16 " pixels) "
        "is smaller than window vertical position (%" PRIu16 " pixels).\n",
        obj->window_id_ref,
        idx,
        vertical_position,
        window.window_vertical_position
      );
    }

    if (obj->object_cropped_flag) {
      bool valid = true;

      /* object_cropping_width */
      if (0 == obj->object_cropping.width) {
        LIBBLU_HDMV_CK_ERROR(
          "Forbidden zero composition object %u cropping width value.\n",
          idx
        );
        valid = false;
      }

      /* object_cropping_height */
      if (0 == obj->object_cropping.height) {
        LIBBLU_HDMV_CK_ERROR(
          "Forbidden zero composition object %u cropping height value.\n",
          idx
        );
        valid = false;
      }

      /* object_cropping_horizontal_position/width */
      unsigned cropping_x_offset =
        obj->object_cropping.horizontal_position
        + obj->object_cropping.width
      ;

      if (cropping_x_offset < object_width) {
        LIBBLU_HDMV_CK_ERROR(
          "Effect composition object %u cropping values "
          "are out of object 0x%" PRIX8 " dimensions, "
          "cropping horizontal_position (%u pixels) and width (%u pixels) "
          "= %u pixels, are greater than object width (%u pixels).\n",
          idx,
          obj->object_id_ref,
          obj->object_cropping.horizontal_position,
          obj->object_cropping.width,
          cropping_x_offset,
          object_width
        );
        valid = false;
      }

      /* object_cropping_horizontal_position/width */
      unsigned cropping_y_offset =
        obj->object_cropping.vertical_position
        + obj->object_cropping.height
      ;

      if (cropping_y_offset < object_height) {
        LIBBLU_HDMV_CK_ERROR(
          "Effect composition object %u cropping values "
          "are out of object 0x%" PRIX8 " dimensions, "
          "cropping vertical_position (%u pixels) and height (%u pixels) "
          "= %u pixels, are greater than object height (%u pixels).\n",
          idx,
          obj->object_id_ref,
          obj->object_cropping.vertical_position,
          obj->object_cropping.height,
          cropping_y_offset,
          object_height
        );
        valid = false;
      }

      if (!valid)
        return -1;

      object_width = obj->object_cropping.width;
      object_height = obj->object_cropping.height;
    }

    bool valid = true;

    if (
      window.window_horizontal_position + window.window_width
      < horizontal_position + object_width
    ) {
      LIBBLU_HDMV_CK_ERROR(
        "Out of effect window 0x%02" PRIX8 " effect composition %u, "
        "horizontal position (%" PRIu16 " pixels) "
        "and width (%" PRIu16 " pixels) = %u pixels, "
        "are bigger than window position (%" PRIu16 " pixels) "
        "and width (%" PRIu16 " pixels) = %u pixels.",
        obj->window_id_ref,
        idx,
        horizontal_position,
        object_width,
        horizontal_position + object_width,
        window.window_horizontal_position,
        window.window_width,
        window.window_horizontal_position + window.window_width
      );
      valid = false;
    }

    if (
      window.window_vertical_position + window.window_height
      < vertical_position + object_height
    ) {
      LIBBLU_HDMV_CK_ERROR(
        "Out of effect window 0x%02" PRIX8 " effect composition %u, "
        "vertical position (%" PRIu16 " pixels) "
        "and height (%" PRIu16 " pixels) = %u pixels, "
        "are bigger than window position (%" PRIu16 " pixels) "
        "and height (%" PRIu16 " pixels) = %u pixels.",
        obj->window_id_ref,
        idx,
        vertical_position,
        object_height,
        vertical_position + object_height,
        window.window_vertical_position,
        window.window_height,
        window.window_vertical_position + window.window_height
      );
      valid = false;
    }

    if (!valid)
      return -1;
  }

  return 0;
}

static int _checkAndBuildDisplaySetHdmvEffectsSequence(
  HdmvEpochState * epoch,
  HdmvSequencePtr * pdsQueue,
  HdmvSequencePtr * odsQueue,
  HdmvEffectSequenceParameters effects_sequence,
  unsigned dsIdx,
  bool orderByValue
)
{

  /* number_of_windows */ // Unchecked

  /* windows */
  for (unsigned idx = 0; idx < effects_sequence.number_of_windows; idx++) {
    const HdmvWindowInfoParameters * win = effects_sequence.windows[idx];

    /* window_id */
    for (unsigned prevIdx = 0; prevIdx < idx; prevIdx++) {
      if (effects_sequence.windows[prevIdx]->window_id == win->window_id) {
        LIBBLU_HDMV_CK_ERROR_RETURN(
          "Effects window id 0x%02" PRIX8 " is not unique.\n",
          win->window_id
        );
      }
    }

    if (_checkHdmvWindowInfoParameters(epoch->video_descriptor, win) < 0)
      return -1;
  }

  /* number_of_effects */
  if (HDMV_MAX_ALLOWED_NB_ICS_EFFECTS < effects_sequence.number_of_effects)
    LIBBLU_HDMV_CK_ERROR_RETURN(
      "Too many animation steps, 'number_of_effects' == %u exceeds %u.\n",
      effects_sequence.number_of_effects,
      HDMV_MAX_ALLOWED_NB_ICS_EFFECTS
    );

  /* effects */
  for (unsigned idx = 0; idx < effects_sequence.number_of_effects; idx++) {
    const HdmvEffectInfoParameters * eff = effects_sequence.effects[idx];

    int ret = _checkHdmvEffectInfoParameters(
      epoch,
      pdsQueue,
      odsQueue,
      effects_sequence,
      eff,
      dsIdx,
      orderByValue
    );
    if (ret < 0)
      return -1;
  }

  return 0;
}

static bool _isUniquePageIdAmongNextPagesHdmvIC(
  HdmvICParameters ic,
  unsigned checked_page_index
)
{
  uint8_t checked_page_id = ic.pages[checked_page_index]->page_id;

  for (unsigned i = checked_page_index + 1; i < ic.number_of_pages; i++) {
    if (ic.pages[i]->page_id == checked_page_id)
      return false;
  }

  return true;
}

static int _checkButtonObjectDimensions(
  uint16_t * buttonWidth,
  uint16_t * buttonHeight,
  uint16_t objectWidth,
  uint16_t objectHeight
)
{
  if (0 == *buttonWidth) {
    /* First object, just save as button dimensions */
    *buttonWidth = objectWidth;
    *buttonHeight = objectHeight;
    return 0;
  }

  if (0 == objectWidth)
    return 0; // No object in button state
  if (*buttonWidth != objectWidth || *buttonHeight != objectHeight)
    return -1; // Mismatch

  return 0; // Matching dimensions
}

/** \~english
 * \brief Check and build Display Set associated with Epoch from supplied
 * Interactive composition.
 *
 * \param video_desc Video descriptor.
 * \param options Parsing options.
 * \param epoch Associated epoch.
 * \param ic Interactive composition to browse.
 * \param dsIdx Builded Display Set number in Epoch.
 * \return int Upon success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
static int _checkAndBuildDisplaySetHdmvIC(
  HdmvParsingOptions options,
  HdmvEpochState * epoch,
  HdmvICParameters ic,
  unsigned dsIdx
)
{
  HdmvSequencePtr pdsQueue = NULL; // PDS Sequences queue.
  HdmvSequencePtr odsQueue = NULL; // ODS Sequences queue.

  const bool orderOdsByValue = options.orderIgsSegByValue;

  for (unsigned page_index = 0; page_index < ic.number_of_pages; page_index++) {
    HdmvPageParameters * page = ic.pages[page_index];

    unsigned presentButtonIdValues[8160];
    MEMSET_ARRAY(presentButtonIdValues, 0xFF);

    unsigned presentButtonNumericValues[10000];
    MEMSET_ARRAY(presentButtonNumericValues, 0xFF);

    /* page_id */
    if (page->page_id == 0xFF)
      LIBBLU_HDMV_CK_ERROR_RETURN("Reserved 'page_id' == 0xFF in use.\n");
    if (!_isUniquePageIdAmongNextPagesHdmvIC(ic, page_index))
      LIBBLU_HDMV_CK_ERROR_RETURN(
        "Duplicated 'page_id' == 0x%02" PRIX8 " in use.\n",
        page->page_id
      );

    /* page_version_id */
    /* Checked when trying to apply update */

    /* in_effects */
    int ret = _checkAndBuildDisplaySetHdmvEffectsSequence(
      epoch,
      &pdsQueue,
      &odsQueue,
      page->in_effects,
      dsIdx,
      orderOdsByValue
    );
    if (ret < 0)
      LIBBLU_HDMV_CK_ERROR_RETURN(
        "Error on 'in_effects' of 'page_id' == 0x%02" PRIX8 ".\n",
        page->page_id
      );

    /* out_effects */
    ret = _checkAndBuildDisplaySetHdmvEffectsSequence(
      epoch,
      &pdsQueue,
      &odsQueue,
      page->out_effects,
      dsIdx,
      orderOdsByValue
    );
    if (ret < 0)
      LIBBLU_HDMV_CK_ERROR_RETURN(
        "Error on 'out_effects' of 'page_id' == 0x%02" PRIX8 ".\n",
        page->page_id
      );

    /* animation_frame_rate_code */
    // TODO: May animation_frame_rate_code be check to not been greater than
    // frame rate.

    /* default_selected_button_id_ref */
    bool defaultSelectedButtonPresent = (0xFFFF == page->default_selected_button_id_ref);
      // 0xFFFF == Unspecified (treated as present).

    /* default_activated_button_id_ref */
    bool defaultActivatedButtonPresent = (0xFFFF == page->default_activated_button_id_ref);
      // 0xFFFF == Unspecified (treated as present).

    /* palette_id_ref */
    if (_checkAndCollectPDSHdmvEpochState(epoch, &pdsQueue, dsIdx, page->palette_id_ref) < 0)
      LIBBLU_HDMV_CK_ERROR_RETURN(
        "Error on 'page_id' == 0x%02" PRIX8 ".\n",
        page->page_id
      );

    for (unsigned bog_index = 0; bog_index < page->number_of_BOGs; bog_index++) {
      HdmvButtonOverlapGroupParameters * bog = page->bogs[bog_index];

      /* default_valid_button_id_ref */
      uint16_t dflValidBtnId = bog->default_valid_button_id_ref;
      bool defaultValidButtonPresent = (0xFFFF == dflValidBtnId);
      defaultSelectedButtonPresent  |= (page->default_selected_button_id_ref == dflValidBtnId);
      defaultActivatedButtonPresent |= (page->default_activated_button_id_ref == dflValidBtnId);

      for (unsigned btn_index = 0; btn_index < bog->number_of_buttons; btn_index++) {
        HdmvButtonParam * btn = bog->buttons[btn_index];
        int ret;

        uint16_t btnObjWidth = 0;  // Button objects size sharing check.
        uint16_t btnObjHeight = 0;
        uint16_t objWidth, objHeight;

        /* button_id */
        // Range check:
        if (0x1FDF < btn->button_id)
          LIBBLU_HDMV_CK_ERROR_RETURN(
            "Invalid 'button_id' == 0x04%" PRIX16 " value "
            "in BOG %u of 'page_id' == 0x%02" PRIX8 " "
            "(value shall be 0x0000 through 0x1FDF).\n",
            btn->button_id,
            bog_index,
            page->page_id
          );
        // Unique check:
        if (presentButtonIdValues[btn->button_id] <= bog_index)
          LIBBLU_HDMV_CK_ERROR_RETURN(
            "Non-unique 'button_id' == 0x%04" PRIX16 " "
            "in BOG %u of 'page_id' == 0x%02" PRIX8 ".\n",
            btn->button_id,
            bog_index,
            page->page_id
          );
        presentButtonIdValues[btn->button_id] = bog_index;
        // BOG default_valid_button_id_ref value presence set:
        defaultValidButtonPresent |= (btn->button_id == dflValidBtnId);

        /* button_numeric_select_value */
        if (btn->button_numeric_select_value != 0xFFFF) {
          // If not disabled (0xFFFF)
          uint16_t numericValue = btn->button_numeric_select_value;

          // Range check:
          if (9999 < numericValue)
            LIBBLU_HDMV_CK_ERROR_RETURN(
              "Invalid 'button_numeric_select_value' == 0x04%" PRIX16 " value "
              "associated with 'button_id' == 0x%04" PRIX16 " of BOG %u "
              "of 'page_id' == 0x%02" PRIX8 " (value shall be 0 through 9999 "
              "or 0xFFFF).\n",
              btn->button_numeric_select_value,
              btn->button_id,
              bog_index,
              page->page_id
            );
          // Unique check:
          if (presentButtonNumericValues[numericValue] < bog_index) {
            LIBBLU_HDMV_CK_ERROR_RETURN(
              "Non-unique 'button_numeric_select_value' == 0x04%" PRIX16 " value "
              "associated with 'button_id' == 0x%04" PRIX16 " of BOG %u "
              "of 'page_id' == 0x%02" PRIX8 " shared with another BOG "
              "of the same page.\n",
              btn->button_numeric_select_value,
              btn->button_id,
              bog_index,
              page->page_id
            );
          }
          presentButtonNumericValues[numericValue] = bog_index;
        }

        /* button_horizontal_position */
        /* button_vertical_position */
        // This is checked after button associated objects collection.

        /* neighbor_info */
        // This is checked after page browsing and associated button_id
        // values collection.

        /* normal_state_info */
        ret = _checkAndCollectODSHdmvEpochState(
          epoch, &odsQueue, dsIdx,
          btn->normal_state_info.start_object_id_ref,
          btn->normal_state_info.end_object_id_ref,
          &objWidth,
          &objHeight,
          orderOdsByValue
        );
        if (ret < 0)
          LIBBLU_HDMV_CK_ERROR_RETURN(
            "Error on 'normal_state_info' of 'button_id' == 0x%04" PRIX16 " of "
            "'page_id' == 0x%02" PRIX8 ".\n",
            btn->button_id,
            page->page_id
          );

        ret = _checkButtonObjectDimensions(
          &btnObjWidth, &btnObjHeight,
          objWidth, objHeight
        );
        assert(0 <= ret); // First value, cannot fail.

        /* selected_state_info */
        ret = _checkAndCollectODSHdmvEpochState(
          epoch, &odsQueue, dsIdx,
          btn->selected_state_info.start_object_id_ref,
          btn->selected_state_info.end_object_id_ref,
          &objWidth,
          &objHeight,
          orderOdsByValue
        );
        if (ret < 0)
          LIBBLU_HDMV_CK_ERROR_RETURN(
            "Error on 'selected_state_info' of 'button_id' == 0x%04" PRIX16 " of "
            "'page_id' == 0x%02" PRIX8 ".\n",
            btn->button_id,
            page->page_id
          );

        ret = _checkButtonObjectDimensions(
          &btnObjWidth, &btnObjHeight,
          objWidth, objHeight
        );
        if (ret < 0) {
          LIBBLU_HDMV_CK_ERROR_RETURN(
            "Inconstant button dimensions between 'normal_state_info' "
            "and 'selected_state_info' of 'button_id' == 0x%04" PRIX16 " of "
            "'page_id' == 0x%02" PRIX8 " (from %" PRIu16 "x%" PRIu16 " to "
            "%" PRIu16 "x%" PRIu16 ").\n",
            btn->button_id,
            page->page_id,
            btnObjWidth,
            btnObjHeight,
            objWidth,
            objHeight
          );
        }

        // Set button initial presentation maximum graphical object size:
        btn->max_initial_width = btnObjWidth;
        btn->max_initial_height = btnObjHeight;

        /* activated_state_info */
        ret = _checkAndCollectODSHdmvEpochState(
          epoch, &odsQueue, dsIdx,
          btn->activated_state_info.start_object_id_ref,
          btn->activated_state_info.end_object_id_ref,
          &objWidth,
          &objHeight,
          orderOdsByValue
        );
        if (ret < 0)
          LIBBLU_HDMV_CK_ERROR_RETURN(
            "Error on 'activated_state_info' of 'button_id' == 0x%04" PRIX16 " of "
            "'page_id' == 0x%02" PRIX8 ".\n",
            btn->button_id,
            page->page_id
          );

        ret = _checkButtonObjectDimensions(
          &btnObjWidth, &btnObjHeight,
          objWidth, objHeight
        );
        if (ret < 0) {
          LIBBLU_HDMV_CK_ERROR_RETURN(
            "Inconstant button dimensions between 'normal_state_info' "
            "and 'activated_state_info' of 'button_id' == 0x%04" PRIX16 " of "
            "'page_id' == 0x%02" PRIX8 " (from %" PRIu16 "x%" PRIu16 " to "
            "%" PRIu16 "x%" PRIu16 ").\n",
            btn->button_id,
            page->page_id,
            btnObjWidth,
            btnObjHeight,
            objWidth,
            objHeight
          );
        }

        /* button_horizontal_position */
        /* button_vertical_position */
        // TODO

        /* navigation_commands */
        // TODO
      }

      if (!defaultValidButtonPresent) {
        LIBBLU_HDMV_CK_ERROR_RETURN(
          "Invalid 'default_valid_button_id_ref' == 0x%04" PRIX16 " on "
          "BOG %u of 'page_id' == 0x%02" PRIX8 ", "
          "no such 'button_id' in BOG.\n",
          bog->default_valid_button_id_ref,
          bog_index,
          page->page_id
        );
      }
    }

    if (!defaultSelectedButtonPresent) {
      LIBBLU_HDMV_CK_ERROR_RETURN(
        "Invalid 'default_selected_button_id_ref' == 0x%04" PRIX16 " on"
        " 'page_id' == 0x%02" PRIX8 ", no BOG with such "
        "'default_valid_button_id_ref' value.\n",
        page->default_selected_button_id_ref,
        page->page_id
      );
    }

    if (!defaultActivatedButtonPresent) {
      LIBBLU_HDMV_CK_ERROR_RETURN(
        "Invalid 'default_activated_button_id_ref' == 0x%04" PRIX16 " on"
        " 'page_id' == 0x%02" PRIX8 ", no BOG with such "
        "'default_valid_button_id_ref' value.\n",
        page->default_activated_button_id_ref,
        page->page_id
      );
    }

    for (unsigned bog_index = 0; bog_index < page->number_of_BOGs; bog_index++) {
      HdmvButtonOverlapGroupParameters * bog = page->bogs[bog_index];

      for (unsigned btn_index = 0; btn_index < bog->number_of_buttons; btn_index++) {
        HdmvButtonParam * btn = bog->buttons[btn_index];

#define CHECK_NEIGHBOR(nghb_fname)                                            \
        if (btn->neighbor_info.nghb_fname != btn->button_id) {                \
          unsigned neighborBogIndex = presentButtonIdValues[                  \
            btn->neighbor_info.nghb_fname                                     \
          ];                                                                  \
          if (bog_index == neighborBogIndex) {                                \
            LIBBLU_HDMV_CK_ERROR_RETURN(                                      \
              "Invalid '" #nghb_fname "' == 0x%04" PRIX16 " on "              \
              "'button_id' == 0x%04" PRIX16 " of BOG %u from "                \
              "'page_id' == 0x%02" PRIX8 ", "                                 \
              "referenced button is from the same BOG.\n"                     \
            );                                                                \
          }                                                                   \
        }

        CHECK_NEIGHBOR(upper_button_id_ref);
        CHECK_NEIGHBOR(lower_button_id_ref);
        CHECK_NEIGHBOR(left_button_id_ref);
        CHECK_NEIGHBOR(right_button_id_ref);
#undef CHECK_NEIGHBOR
      }
    }
  }

  // TODO: Check no BOG overlap.

  return 0;
}

static int _checkAndBuildIgsDisplaySetHdmvEpochState(
  HdmvParsingOptions options,
  HdmvEpochState * epoch,
  unsigned dsIdx
)
{
  HdmvICParameters ic;

  /* ICS */
  if (_getAndCollectInteractiveCompositionHdmvEpochState(epoch, &ic) < 0)
    return -1;

  /* PDS/ODS */
  if (_checkAndBuildDisplaySetHdmvIC(options, epoch, ic, dsIdx) < 0)
    return -1;

  /* END */
  if (_checkAndCollectEndSequenceHdmvEpochState(epoch) < 0)
    return -1;

  return 0;
}

static int _checkIdPresenceHdmvWindowDefinitionParameters(
  HdmvWDParameters window_definition,
  uint8_t window_id_ref,
  HdmvWindowInfoParameters * infos
)
{
  for (unsigned i = 0; i < window_definition.number_of_windows; i++) {
    const HdmvWindowInfoParameters * window = window_definition.windows[i];

    if (window_id_ref == window->window_id) {
      /* Window id match */
      *infos = *window;
      return 0;
    }
  }

  LIBBLU_HDMV_CK_ERROR_RETURN(
    "Unknown window_id_ref == 0x%02" PRIX8 ", no such id in the epoch WDS.\n",
    window_id_ref
  );
}

static int _checkAndBuildDisplaySetHdmvPC(
  HdmvParsingOptions options,
  HdmvEpochState * epoch,
  HdmvPCParameters pc,
  HdmvWDParameters wd,
  unsigned dsIdx
)
{
  HdmvSequencePtr pdsQueue = NULL; // PDS Sequences queue.
  HdmvSequencePtr odsQueue = NULL; // ODS Sequences queue.

  const bool orderOdsByValue = options.orderPgsSegByValue;

  /* palette_update_flag */
  // TODO

  /* palette_id_ref */
  if (_checkAndCollectPDSHdmvEpochState(epoch, &pdsQueue, dsIdx, pc.palette_id_ref) < 0)
    return -1;

  for (unsigned i = 0; i < pc.number_of_composition_objects; i++) {
    const HdmvCompositionObjectParameters * obj = pc.composition_objects[i];
    int ret;

    /* object_id_ref */
    uint16_t obj_width, obj_height;
    ret = _checkAndCollectUniqueODSHdmvEpochState(
      epoch, &odsQueue, dsIdx,
      obj->object_id_ref,
      &obj_width,
      &obj_height,
      orderOdsByValue
    );
    if (ret < 0)
      LIBBLU_HDMV_CK_ERROR_RETURN("Error on composition object %u.\n", i);

    /* window_id_ref */
    HdmvWindowInfoParameters window;
    if (_checkIdPresenceHdmvWindowDefinitionParameters(wd, obj->window_id_ref, &window) < 0)
      LIBBLU_HDMV_CK_ERROR_RETURN("Error on composition object %u.\n", i);

    /* Check object position/dimensions */
    bool valid = true;

    /* composition_object_horizontal_position */
    uint16_t obj_horizontal_position = obj->composition_object_horizontal_position;
    if (obj_horizontal_position < window.window_horizontal_position) {
      LIBBLU_HDMV_CK_ERROR(
        "Out of window composition object horizontal position, "
        "window horizontal position is %u pixels, "
        "object horizontal position is %u pixels.\n",
        window.window_horizontal_position,
        obj_horizontal_position
      );
      valid = false;
    }

    /* composition_object_vertical_position */
    uint16_t obj_vertical_position = obj->composition_object_vertical_position;
    if (obj_vertical_position < window.window_vertical_position) {
      LIBBLU_HDMV_CK_ERROR(
        "Out of window composition object vertical position, "
        "window vertical position is %u pixels, "
        "object vertical position is %u pixels.\n",
        window.window_vertical_position,
        obj_vertical_position
      );
      valid = false;
    }

    if (!valid)
      return -1;

    if (obj->object_cropped_flag) {
      /* object_cropping_width */
      if (0 == obj->object_cropping.width) {
        LIBBLU_HDMV_CK_ERROR(
          "Forbidden object_cropping_width == 0 value.\n"
        );
        valid = false;
      }

      /* object_cropping_height */
      if (0 == obj->object_cropping.height) {
        LIBBLU_HDMV_CK_ERROR(
          "Forbidden object_cropping_height == 0 value.\n"
        );
        valid = false;
      }

      /* object_cropping_horizontal_position/object_cropping_width */
      if (
        obj_width
        < obj->object_cropping.horizontal_position + obj->object_cropping.width
      ) {
        LIBBLU_HDMV_CK_ERROR(
          "Out of object horizontal dimensions cropping, "
          "object width is %u pixels, "
          "cropping horizontal_position (= %u pixels) + width (= %u pixels) "
          "= %u pixels.\n",
          obj_width,
          obj->object_cropping.horizontal_position,
          obj->object_cropping.width,
          obj->object_cropping.horizontal_position + obj->object_cropping.width
        );
        valid = false;
      }

      /* object_cropping_vertical_position/object_cropping_height */
      if (
        obj_height
        < obj->object_cropping.vertical_position + obj->object_cropping.height
      ) {
        LIBBLU_HDMV_CK_ERROR(
          "Out of object vertical dimensions cropping, "
          "object width is %u pixels, "
          "cropping vertical_position (= %u pixels) + height (= %u pixels) "
          "= %u pixels.\n",
          obj_height,
          obj->object_cropping.vertical_position,
          obj->object_cropping.height,
          obj->object_cropping.vertical_position + obj->object_cropping.height
        );
        valid = false;
      }

      if (!valid)
        return -1;

      obj_width = obj->object_cropping.width;
      obj_height = obj->object_cropping.height;
    }

    /* width */
    if (
      window.window_horizontal_position + window.window_width
      < obj_horizontal_position + obj_width
    ) {
      LIBBLU_HDMV_CK_ERROR(
        "Out of window composition object horizontal position, "
        "window horizontal_position (= %u pixels) + width (= %u pixels) "
        "= %u pixels, object horizontal_position (= %u pixels) "
        "+ width (= %u pixels) = %u pixels.\n",
        window.window_horizontal_position,
        window.window_width,
        window.window_horizontal_position + window.window_width,
        obj_horizontal_position,
        obj_width
      );
      valid = false;
    }

    /* height */
    if (
      window.window_vertical_position + window.window_height
      < obj_vertical_position + obj_height
    ) {
      LIBBLU_HDMV_CK_ERROR(
        "Out of window composition object vertical position, "
        "window vertical_position (= %u pixels) + height (= %u pixels) "
        "= %u pixels, object vertical_position (= %u pixels) "
        "+ height (= %u pixels) = %u pixels.\n",
        window.window_vertical_position,
        window.window_height,
        window.window_vertical_position + window.window_height,
        obj_vertical_position,
        obj_height
      );
      valid = false;
    }

    if (!valid)
      LIBBLU_HDMV_CK_ERROR_RETURN("Error on composition object %u.\n", i);
  }

  return 0;
}

static int _checkAndBuildPgsDisplaySetHdmvEpochState(
  HdmvParsingOptions options,
  HdmvEpochState * epoch,
  unsigned dsIdx
)
{

  /* PCS */
  HdmvPCParameters pc;
  if (_getAndCollectPresentationCompositionHdmvEpochState(epoch, &pc) < 0)
    return -1;

  /* WDS */
  HdmvWDParameters wd;
  if (_getAndCollectWindowDefinitionHdmvEpochState(epoch, &wd, dsIdx) < 0)
    return -1;

  if (_checkHdmvWindowDefinitionParameters(epoch->video_descriptor, wd) < 0)
    return -1;

  /* PDS/ODS */
  if (_checkAndBuildDisplaySetHdmvPC(options, epoch, pc, wd, dsIdx) < 0)
    return -1;

  /* END */
  if (_checkAndCollectEndSequenceHdmvEpochState(epoch) < 0)
    return -1;

  return 0;
}

int checkAndBuildDisplaySetHdmvEpochState(
  HdmvParsingOptions options,
  HdmvStreamType type,
  HdmvEpochState * epoch,
  unsigned dsIdx
)
{
  switch (type) {
    case HDMV_STREAM_TYPE_IGS:
      return _checkAndBuildIgsDisplaySetHdmvEpochState(options, epoch, dsIdx);
    case HDMV_STREAM_TYPE_PGS:
      return _checkAndBuildPgsDisplaySetHdmvEpochState(options, epoch, dsIdx);
  }

  LIBBLU_HDMV_CK_ERROR_RETURN(
    "Unreachable %s:%s():%u\n",
    __FILE__, __func__, __LINE__
  );
}

/** \~english
 * \brief Coded Object Buffer (EB) size.
 *
 * \return size_t
 *
 * \todo Use rather in T-STD buffer verifier.
 */
static size_t _getCodedObjectBufferSize(
  HdmvStreamType type
)
{
  static const size_t sizes[] = {
    4 << 20, /**< Interactive Graphics  - 4MiB  */
    1 << 20, /**< Presentation Graphics - 1MiB  */
  };

  return sizes[type];
}

/** \~english
 * \brief Decoded Object Buffer (DB) size.
 *
 * \param type
 * \return size_t
 */
static size_t _getDecodedObjectBufferSize(
  HdmvStreamType type
)
{
  static const size_t sizes[] = {
    16 << 20, /**< Interactive Graphics  - 16MiB  */
    4  << 20, /**< Presentation Graphics -  4MiB  */
  };

  return sizes[type];
}

int checkObjectsBufferingHdmvEpochState(
  HdmvEpochState * epoch,
  HdmvStreamType type
)
{
  HdmvSequencePtr seq;

  size_t codedObjBufferUsage = 0;
  size_t decodedObjBufferUsage = 0;

  LIBBLU_HDMV_CK_DEBUG("  Checking objects buffer usage:\n");
  LIBBLU_HDMV_CK_DEBUG("     Compressed size => Uncompressed size.\n");

  /* Check all ODS in memory */
  seq = getSequenceByIdxHdmvEpochState(epoch, HDMV_SEGMENT_TYPE_ODS_IDX);
  for (; NULL != seq; seq = seq->nextSequence) {
    HdmvODParameters object_data = seq->data.od;
    size_t objCodedSize = object_data.object_data_length;
    size_t objDecodedSize = object_data.object_width * object_data.object_height;

    LIBBLU_HDMV_CK_DEBUG(
      "   - Object %u: %ux%u, %zu byte(s) => %zu byte(s).\n",
      object_data.object_descriptor.object_id,
      object_data.object_width,
      object_data.object_height,
      objCodedSize,
      objDecodedSize
    );

    codedObjBufferUsage += objCodedSize;
    decodedObjBufferUsage += objDecodedSize;
  }

#if 0
  size_t codedObjBufferSize = _getCodedObjectBufferSize(type);
  LIBBLU_HDMV_CK_DEBUG(
    "    => Coded Object Buffer (EB) usage: %zu bytes / %zu bytes.\n",
    codedObjBufferUsage,
    codedObjBufferSize
  );

  if (codedObjBufferSize < codedObjBufferUsage)
    LIBBLU_HDMV_CK_WARNING(
      "Coded Object Buffer (EB) overflows, "
      "%zu bytes used out of buffer size of %zu bytes.\n",
      codedObjBufferUsage,
      codedObjBufferSize
    );
#endif

  LIBBLU_HDMV_CK_DEBUG(
    "    => Coded objects size: %zu bytes.\n",
    codedObjBufferUsage
  );

  size_t decodedObjBufferSize = _getDecodedObjectBufferSize(type);
  LIBBLU_HDMV_CK_DEBUG(
    "    => Decoded Object Buffer (DB) usage: %zu bytes / %zu bytes.\n",
    decodedObjBufferUsage,
    decodedObjBufferSize
  );

  if (decodedObjBufferSize < decodedObjBufferUsage)
    LIBBLU_HDMV_CK_ERROR_RETURN(
      "Decoded Object Buffer (DB) overflows, "
      "%zu bytes used out of buffer size of %zu bytes.\n",
      decodedObjBufferUsage,
      decodedObjBufferSize
    );

  return 0;
}

int checkDuplicatedDSHdmvEpochState(
  HdmvEpochState * epoch,
  unsigned lastDSIdx
)
{

  for (hdmv_segtype_idx idx = 0; idx < HDMV_NB_SEGMENT_TYPES; idx++) {
    HdmvSequencePtr lastDSSeq = NULL;

    for (
      HdmvSequencePtr seq = getSequenceByIdxHdmvEpochState(epoch, idx);
      NULL != seq;
      seq = seq->nextSequence
    ) {
      if (isFromIdxDisplaySetHdmvSequence(seq, lastDSIdx)) {
        /* Presence of a non-refreshed sequence from previous DS. */
        LIBBLU_HDMV_COM_ERROR_RETURN(
          "Missing segments from previous duplicated DS.\n"
        );
      }

      if (NULL == lastDSSeq)
        setDSSequenceByIdxHdmvEpochState(epoch, idx, seq);
      else
        lastDSSeq->nextSequenceDS = seq;
      lastDSSeq = seq;
    }
  }

  return 0;
}