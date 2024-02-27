/** \~english
 * \file hdmv_common.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief HDMV bitstreams (IGS, PGS) common handling module.
 */

#ifndef __LIBBLU_MUXER__CODECS__HDMV__COMMON_H__
#define __LIBBLU_MUXER__CODECS__HDMV__COMMON_H__

#include "hdmv_error.h"
#include "hdmv_data.h"
#include "../../../util.h"
#include "../../../esms/scriptCreation.h"

typedef struct {
  float igs_od_decode_delay_factor;  /**< Optional additional decoding delay
    for IGS used to smooth out bitrate spikes related to object decoding.
    This value is a factor relating to the time required to decode an object,
    0 means no delay after decoding an object, 10 means that a pause of
    10 times the time required to decode the object will be used before
    decode the next object.                                                  */
  bool igs_use_earlier_transfer;     /**< Use the alternative timestamp
    calculation algorithm for IGS which begins the transfer of objects as
    soon as all the objects necessary for the first composition are present. */

  bool enable_timestamps_debug;
} HdmvParsingOptions;

/* ### Timestamps utilities : ############################################## */

/* ### Segments types indexes : ############################################ */

typedef int hdmv_segtype_idx;

typedef enum {
  HDMV_SEGMENT_TYPE_ICS_IDX,
  HDMV_SEGMENT_TYPE_PCS_IDX,
  HDMV_SEGMENT_TYPE_WDS_IDX,
  HDMV_SEGMENT_TYPE_PDS_IDX,
  HDMV_SEGMENT_TYPE_ODS_IDX,
  HDMV_SEGMENT_TYPE_END_IDX,

  HDMV_NB_SEGMENT_TYPES
} hdmv_segtype_idx_value;

static const HdmvSegmentType hdmv_segtype_indexes[] = {
  HDMV_SEGMENT_TYPE_ICS,
  HDMV_SEGMENT_TYPE_PCS,
  HDMV_SEGMENT_TYPE_WDS,
  HDMV_SEGMENT_TYPE_PDS,
  HDMV_SEGMENT_TYPE_ODS,
  HDMV_SEGMENT_TYPE_END
};

static inline bool isValidSegmentTypeIndexHdmvContext(
  hdmv_segtype_idx idx
)
{
  return 0 <= idx && idx < HDMV_NB_SEGMENT_TYPES;
}

static inline hdmv_segtype_idx segmentTypeIndexHdmvContext(
  HdmvSegmentType type
)
{
  unsigned i;

  assert(HDMV_NB_SEGMENT_TYPES == ARRAY_SIZE(hdmv_segtype_indexes));

  for (i = 0; i < ARRAY_SIZE(hdmv_segtype_indexes); i++) {
    if (hdmv_segtype_indexes[i] == type)
      return (int) i;
  }

  LIBBLU_HDMV_COM_ERROR_RETURN("Unexpected segment type 0x%02X.\n", type);
}

#define SEGMENT_TYPE_IDX_STR_SIZE  4

static inline const char * segmentTypeIndexStr(
  hdmv_segtype_idx idx
)
{
  static const char strings[][SEGMENT_TYPE_IDX_STR_SIZE] = {
    "ICS",
    "PCS",
    "WDS",
    "PDS",
    "ODS",
    "END"
  }; /**< Sizes are restricted to SEGMENT_TYPE_IDX_STR_SIZE characters
    (including NUL character) */

  assert(0 <= idx && idx < (int) ARRAY_SIZE(strings));

  return strings[idx];
}

/* ###  */

typedef struct {
  unsigned types[HDMV_NB_SEGMENT_TYPES];
  unsigned total;
} HdmvContextSegmentTypesCounter;

static inline void incByIdxHdmvContextSegmentTypesCounter(
  HdmvContextSegmentTypesCounter * counter,
  hdmv_segtype_idx idx
)
{
  assert(isValidSegmentTypeIndexHdmvContext(idx));

  counter->types[idx]++;
  counter->total++;
}

static inline unsigned getTotalHdmvContextSegmentTypesCounter(
  HdmvContextSegmentTypesCounter counter
)
{
  return counter.total;
}

static inline unsigned getByIdxHdmvContextSegmentTypesCounter(
  const HdmvContextSegmentTypesCounter counter,
  hdmv_segtype_idx idx
)
{
  assert(isValidSegmentTypeIndexHdmvContext(idx));

  return counter.types[idx];
}

static inline void resetHdmvContextSegmentTypesCounter(
  HdmvContextSegmentTypesCounter * counter
)
{
  *counter = (HdmvContextSegmentTypesCounter) {
    0
  };
}

static inline void printContentHdmvContextSegmentTypesCounter(
  const HdmvContextSegmentTypesCounter cnt,
  HdmvStreamType type
)
{
#define _G(_i)  getByIdxHdmvContextSegmentTypesCounter(cnt, _i)
#define _P(_n)  lbc_printf(lbc_str(" - "#_n": %u.\n"), _G(HDMV_SEGMENT_TYPE_##_n##_IDX))

  if (HDMV_STREAM_TYPE_IGS == type) {
    _P(ICS);
  }
  else { /* HDMV_STREAM_TYPE_PGS == type */
    _P(PCS);
    _P(WDS);
  }

  _P(PDS);
  _P(ODS);
  _P(END);

#undef _P
#undef _G

  lbc_printf(
    lbc_str(" TOTAL: %u.\n"),
    getTotalHdmvContextSegmentTypesCounter(cnt)
  );
}

/* ### HDMV Segment : ###################################################### */

typedef struct HdmvSegment {
  struct HdmvSegment * next_segment;
  HdmvSegmentParameters param;
} HdmvSegment, *HdmvSegmentPtr;

static inline HdmvSegmentPtr createHdmvSegment(
  const HdmvSegmentParameters param
)
{
  HdmvSegmentPtr seg = calloc(1, sizeof(HdmvSegment));
  if (NULL == seg)
    LIBBLU_HDMV_COM_ERROR_NRETURN("Memory allocation error.\n");
  seg->param = param;
  return seg;
}

static inline void destroyHdmvSegmentList(
  HdmvSegmentPtr head
)
{
  HdmvSegmentPtr next = NULL;
  for (HdmvSegmentPtr seg = head; NULL != seg; seg = next) {
    next = seg->next_segment;
    free(seg);
  }
}

/* ### HDMV Sequence : ##################################################### */

typedef struct HdmvSequence {
  struct HdmvSequence * prev_sequence;
  struct HdmvSequence * next_sequence;
  unsigned idx;  /**< Original bitstream index of the segment sequence among
    other sequences of the same type.                                        */

  HdmvSegmentType type;
  HdmvSegmentPtr segments;
  HdmvSegmentPtr last_segment;

  uint8_t * fragments;
  uint32_t fragments_allocated_len;
  uint32_t fragments_used_len;
  uint32_t fragments_parsed_off;

  union {
    HdmvPDSegmentParameters pd;  /**< Palette Definition.                    */
    HdmvODParameters        od;  /**< Object Definition.                     */
    HdmvPCParameters        pc;  /**< Presentation Composition.              */
    HdmvWDParameters        wd;  /**< Window Definition.                     */
    HdmvICParameters        ic;  /**< Interactive Composition.               */
  } data;

  int64_t earliest_available_pts;    /**< Earliest possible PTS value for a
    definition update carried by this segment sequence without causing
    disruption to a currently valid/active composition requiring the state
    of the definition prior to this update.                                  */
  bool has_earliest_available_pts;   /**< This segment sequence carries a
    definition update (ODS or PDS) and #earliest_available_pts applies.      */

  int64_t earliest_available_dts;
  bool has_earliest_available_dts;

  HdmvODSUsage ods_usage;

  int64_t pts;  /**< Presentation Time Stamp plus referential time code,
    in 90kHz clock ticks.                                                    */
  int64_t dts;  /**< Decoding Time Stamp plus referential time code,
    in 90kHz clock ticks.                                                    */
} HdmvSequence, *HdmvSequencePtr;

static inline HdmvSequencePtr createHdmvSequence(
  void
)
{
  HdmvSequencePtr seq = calloc(1, sizeof(HdmvSequence));
  if (NULL == seq)
    LIBBLU_HDMV_COM_ERROR_NRETURN("Memory allocation error.\n");
  return seq;
}

static inline void destroyHdmvSequence(
  HdmvSequencePtr seq
)
{
  destroyHdmvSegmentList(seq->segments);
  free(seq->fragments);
  if (HDMV_SEGMENT_TYPE_ICS == seq->type)
    cleanHdmvICParameters(seq->data.ic);
  free(seq);
}

static inline void destroyHdmvSequenceList(
  HdmvSequencePtr head
)
{
  HdmvSequencePtr next = NULL;
  for (HdmvSequencePtr seq = head; NULL != seq; seq = next) {
    next = seq->next_sequence;
    destroyHdmvSequence(seq);
  }
}

static inline bool isUniqueInDisplaySetHdmvSequence(
  const HdmvSequencePtr sequence
)
{
  return NULL != sequence && NULL == sequence->next_sequence;
}

static inline bool atMostOneInDisplaySetHdmvSequence(
  const HdmvSequencePtr sequence
)
{
  return NULL == sequence || NULL == sequence->next_sequence;
}

/* ###### Fragments fetching : ############################################# */

int parseFragmentHdmvSequence(
  HdmvSequencePtr dst,
  BitstreamReaderPtr input,
  uint16_t fragment_length
);

/* ###### Fragments unpacking : ############################################ */

static inline uint32_t getRemSizeFragmentHdmvSequence(
  const HdmvSequencePtr sequence
)
{
  return sequence->fragments_used_len - sequence->fragments_parsed_off;
}

static inline const uint8_t * getRemDataFragmentHdmvSequence(
  HdmvSequencePtr sequence,
  uint32_t * remaining_size_ret
)
{
  assert(NULL != sequence);
  assert(NULL != sequence->fragments);

  if (NULL != remaining_size_ret)
    *remaining_size_ret = sequence->fragments_used_len - sequence->fragments_parsed_off;

  return &sequence->fragments[
    sequence->fragments_parsed_off
  ];
}

static inline int readValueFragmentHdmvSequence(
  HdmvSequencePtr sequence,
  uint32_t size,
  uint64_t * value_ret
)
{
  assert(NULL != sequence);
  assert(NULL != sequence->fragments);
  assert(size <= 8);

  uint64_t value = 0;
  while (size--) {
    if (sequence->fragments_used_len <= sequence->fragments_parsed_off)
      LIBBLU_HDMV_COM_ERROR_RETURN("Prematurate end of sequence data.\n");

    uint64_t byte = sequence->fragments[sequence->fragments_parsed_off++];
    value |= byte << (size << 3);
  }

  if (NULL != value_ret)
    *value_ret = value;
  return 0;
}

/* ### HDMV Display Set : ################################################## */

typedef enum {
  HDMV_DS_NON_INITIALIZED,
  HDMV_DS_COMPLETED,
  HDMV_DS_INITIALIZED
} HdmvDisplaySetUsageState;

/** \~english
 * \brief Display Set (DS) memory content.
 *
 */
typedef struct {
  HdmvDisplaySetUsageState init_usage;
  HdmvCDParameters compo_desc;
  unsigned epoch_idx;  /**< Index of the DS associated Epoch in whole stream. */
  unsigned ds_idx;  /**< Index of the Display Set in whole stream. */

  struct {
    HdmvSequencePtr pending;    /**< Pending decoded segment sequence.     */

    HdmvSequencePtr ds_head;
    HdmvSequencePtr ds_last;
    unsigned ds_nb_seq;
  } sequences[HDMV_NB_SEGMENT_TYPES];

  unsigned total_nb_seq;
} HdmvDSState;

static inline void cleanHdmvDSState(
  HdmvDSState ds_state
)
{
  for (hdmv_segtype_idx i = 0; i < HDMV_NB_SEGMENT_TYPES; i++) {
    destroyHdmvSequenceList(ds_state.sequences[i].pending);
    destroyHdmvSequenceList(ds_state.sequences[i].ds_head);
  }
}

static inline void addDSSequenceByIdxHdmvDSState(
  HdmvDSState * ds_state,
  hdmv_segtype_idx seg_type_idx,
  HdmvSequencePtr sequence
)
{
  if (NULL == ds_state->sequences[seg_type_idx].ds_head)
    ds_state->sequences[seg_type_idx].ds_head = sequence;
  else {
    ds_state->sequences[seg_type_idx].ds_last->next_sequence = sequence;
    sequence->prev_sequence = ds_state->sequences[seg_type_idx].ds_last;
  }

  ds_state->sequences[seg_type_idx].ds_last = sequence;

  // Update Sequence counters (and set index in list)
  sequence->idx = ds_state->sequences[seg_type_idx].ds_nb_seq++;
  ds_state->total_nb_seq++;
}

/** \~english
 * \brief Retrieve the first sequence of segments of the specified type in the
 * Display Set.
 *
 * \param ds_state Display Set.
 * \param idx Type index.
 * \return HdmvSequencePtr Sequence of segments of specified type
 * (or NULL if absent).
 */
HdmvSequencePtr getFirstSequenceByIdxHdmvDSState(
  const HdmvDSState * ds_state,
  hdmv_segtype_idx idx
);

/** \~english
 * \brief Retrieve the (Interactive or Presentation) Compostion Segment from
 * Display Set.
 *
 * \param ds_state Display Set.
 * \return HdmvSequencePtr Sequence of ICS/PCS segments.
 */
HdmvSequencePtr getICSPCSSequenceHdmvDSState(
  const HdmvDSState * ds_state
);

/** \~english
 * \brief Retrieve the Interactive Compostion Segment from Display Set.
 *
 * \param ds_state Display Set.
 * \return HdmvSequencePtr Sequence of ICS segments.
 */
HdmvSequencePtr getICSSequenceHdmvDSState(
  const HdmvDSState * ds_state
);

/** \~english
 * \brief Retrieve the Presentation Compostion Segment from Display Set.
 *
 * \param ds_state Display Set.
 * \return HdmvSequencePtr Sequence of PCS segments.
 */
HdmvSequencePtr getPCSSequenceHdmvDSState(
  const HdmvDSState * ds_state
);

HdmvSequencePtr getWDSSequenceHdmvDSState(
  const HdmvDSState * ds_state
);

HdmvSequencePtr getFirstPDSSequenceHdmvDSState(
  const HdmvDSState * ds_state
);

HdmvSequencePtr getLastPDSSequenceHdmvDSState(
  const HdmvDSState * ds_state
);

HdmvSequencePtr getFirstODSSequenceHdmvDSState(
  const HdmvDSState * ds_state
);

HdmvSequencePtr getLastODSSequenceHdmvDSState(
  const HdmvDSState * ds_state
);

HdmvSequencePtr getENDSequenceHdmvDSState(
  const HdmvDSState * ds_state
);

static inline bool isCompoStateHdmvDSState(
  const HdmvDSState * ds_state,
  HdmvCompositionState expected
)
{
  return (expected == ds_state->compo_desc.composition_state);
}

/* ### HDMV Epoch memory state : ########################################### */

typedef enum {
  HDMV_DEF_NEVER_PROVIDED = 0x0,  /**< Never introduced in Epoch. */
  HDMV_DEF_NON_REFRESHED  = 0x1,  /**< Introduced in Epoch, but not refreshed
    since last Acquisition Point. */
  HDMV_DEF_PROVIDED       = 0x3,  /**< Introduced in Epoch, available for
    use. */
  HDMV_DEF_JUST_PROVIDED  = 0x7,  /**< Introduced in Epoch, already updated
    by current DS. Used to detect duplicated definitions. */
  HDMV_DEF_JUST_UPDATED   = 0xF   /**< Introduced in Epoch, already updated
    by current DS with a new version. */
} HdmvEpochStateDefinitionState;

typedef struct {
  HdmvPDSegmentParameters def;
  HdmvEpochStateDefinitionState state;
} HdmvEpochStatePalette;

typedef struct {
  HdmvODParameters def;
  HdmvEpochStateDefinitionState state;
} HdmvEpochStateObject;

typedef struct {
  HdmvStreamModel stream_model;
  HdmvUserInterfaceModel user_interface_model;
  bool initialized;
} HdmvEpochStateIGStream;

typedef struct {
  bool initialized;
} HdmvEpochStatePGStream;

typedef struct {
  HdmvStreamType type;
  HdmvVDParameters video_descriptor;

  HdmvPDSegmentParameters last_PDS[HDMV_MAX_NB_PAL];
    // Store as palettes update process is specific.
  HdmvEpochStatePalette palettes[HDMV_MAX_NB_PAL];

  HdmvEpochStateObject objects[HDMV_MAX_NB_OBJ];

  HdmvPCParameters active_pc[HDMV_MAX_NB_ACTIVE_PC];
  int64_t active_pc_pts[HDMV_MAX_NB_ACTIVE_PC];
  unsigned cur_active_pc;
  unsigned nb_active_pc;

  HdmvWDParameters window_def;
  bool window_def_present;
  bool window_def_provided;

  HdmvICParameters active_ic;
  int64_t active_ic_pts;
  bool active_ic_present;

  union {
    HdmvEpochStateIGStream ig;
    HdmvEpochStatePGStream pg;
  } stream_parameters;
} HdmvEpochState;

static inline void cleanHdmvEpochState(
  HdmvEpochState epoch_state
)
{
  cleanHdmvICParameters(epoch_state.active_ic);
}

static inline void resetEpochStartHdmvEpochState(
  HdmvEpochState * epoch_state
)
{
  memset(epoch_state->palettes, 0x00, sizeof(epoch_state->palettes));
  memset(epoch_state->objects,  0x00, sizeof(epoch_state->objects));
  epoch_state->nb_active_pc = epoch_state->cur_active_pc = 0;
  resetHdmvICParameters(&epoch_state->active_ic);
  epoch_state->active_ic_present = false;
}

static inline void resetAcquisitionPointHdmvEpochState(
  HdmvEpochState * epoch_state
)
{
  for (unsigned i = 0; i < HDMV_MAX_NB_PAL; i++)
    epoch_state->palettes[i].state &= HDMV_DEF_PROVIDED;
  for (unsigned i = 0; i < HDMV_MAX_NB_OBJ; i++)
    epoch_state->objects[i].state  &= HDMV_DEF_PROVIDED;
}

static inline void resetNormalHdmvEpochState(
  HdmvEpochState * epoch_state
)
{
  for (unsigned i = 0; i < HDMV_MAX_NB_PAL; i++)
    epoch_state->palettes[i].state &= HDMV_DEF_NON_REFRESHED;
  for (unsigned i = 0; i < HDMV_MAX_NB_OBJ; i++)
    epoch_state->objects[i].state  &= HDMV_DEF_NON_REFRESHED;
}

/* ### Palette Definitions : ############################################### */

bool fetchPaletteHdmvEpochState(
  const HdmvEpochState * epoch_state,
  uint8_t palette_id,
  bool must_be_in_current_DS,
  HdmvPDSegmentParameters * pd_ret,
  bool * has_been_updated_ret
);

static inline bool justProvidedPaletteHdmvEpochState(
  const HdmvEpochState * epoch_state,
  uint8_t palette_id
)
{
  assert(UINT8_MAX <= HDMV_MAX_NB_PAL); // Avoid type limited range warning
  return (HDMV_DEF_JUST_PROVIDED <= epoch_state->palettes[palette_id].state);
}

static inline bool comparePDSEpochState(
  const HdmvEpochState * epoch_state,
  const HdmvPDSegmentParameters pal_def
)
{
  assert(
    fetchPaletteHdmvEpochState(
      epoch_state,
      pal_def.palette_descriptor.palette_id,
      false,
      NULL,
      NULL
    )
  );

  return constantHdmvPDSegmentParameters(
    epoch_state->last_PDS[pal_def.palette_descriptor.palette_id],
    pal_def
  );
}

void setPaletteHdmvEpochState(
  HdmvEpochState * epoch_state,
  const HdmvPDSegmentParameters pal_def
);

/* ### Object Definitions : ################################################ */

static inline bool fetchObjectHdmvEpochState(
  const HdmvEpochState * epoch_state,
  uint16_t object_id,
  bool must_be_in_current_DS,
  HdmvODParameters * od_ret,
  bool * has_been_updated_ret
)
{
  if (HDMV_MAX_NB_OBJ <= object_id)
    return false; // Invalid object_id
  HdmvEpochStateObject obj = epoch_state->objects[object_id];
  if (HDMV_DEF_NEVER_PROVIDED == obj.state)
    return false; // Never transmitted
  if (HDMV_DEF_NON_REFRESHED == obj.state && must_be_in_current_DS)
    return false; // Not refreshed

  if (NULL != od_ret)
    *od_ret = obj.def;
  if (NULL != has_been_updated_ret)
    *has_been_updated_ret = (HDMV_DEF_JUST_UPDATED == obj.state);
  return true;
}

/** \~english
 * \brief
 *
 * \param epoch_state
 * \param od_ret
 * \param object_id
 * \return true The last parsed DS contains one ODS with the supplied
 * 'object_id'.
 * \return false
 */
static inline bool existsObjectHdmvEpochState(
  const HdmvEpochState * epoch_state,
  uint16_t object_id,
  HdmvODParameters * od_ret
)
{
  assert(object_id < HDMV_MAX_NB_OBJ);
  HdmvEpochStateObject obj = epoch_state->objects[object_id];
  assert(HDMV_DEF_NEVER_PROVIDED != obj.state);

  if (obj.state < HDMV_DEF_JUST_PROVIDED)
    return false; // Not just provided

  if (NULL != od_ret)
    *od_ret = obj.def;
  return true;
}

static inline bool justProvidedObjectHdmvEpochState(
  const HdmvEpochState * epoch_state,
  uint16_t object_id
)
{
  assert(object_id < HDMV_MAX_NB_OBJ); // Avoid type limited range warning
  return (HDMV_DEF_JUST_PROVIDED <= epoch_state->objects[object_id].state);
}

static inline bool compareODSEpochState(
  const HdmvEpochState * epoch_state,
  const HdmvODParameters obj_def
)
{
  assert(
    fetchObjectHdmvEpochState(
      epoch_state,
      obj_def.object_descriptor.object_id,
      false,
      NULL,
      NULL
    )
  );

  return lb_data_equal(
    &epoch_state->objects[obj_def.object_descriptor.object_id].def,
    &obj_def,
    sizeof(HdmvODSegmentParameters)
  );
}

void setObjectHdmvEpochState(
  HdmvEpochState * epoch_state,
  const HdmvODParameters obj_def
);

/* ### Presentation Compositions : ########################################## */

static inline bool fetchPCHdmvEpochState(
  const HdmvEpochState * epoch_state,
  unsigned neg_offset,
  HdmvPCParameters * pc_ret,
  int64_t * pc_pts_ret
)
{
  if (epoch_state->nb_active_pc <= neg_offset)
    return false;

  unsigned offset =
    (HDMV_MAX_NB_ACTIVE_PC + epoch_state->cur_active_pc - neg_offset)
    % HDMV_MAX_NB_ACTIVE_PC
  ;

  if (NULL != pc_ret)
    *pc_ret     = epoch_state->active_pc[offset];
  if (NULL != pc_pts_ret)
    *pc_pts_ret = epoch_state->active_pc_pts[offset];
  return true;
}

static inline bool fetchOldestPCHdmvEpochState(
  const HdmvEpochState * epoch_state,
  HdmvPCParameters * pc_ret,
  int64_t * pc_pts_ret
)
{
  return fetchPCHdmvEpochState(
    epoch_state, HDMV_MAX_NB_ACTIVE_PC-1, pc_ret, pc_pts_ret
  );
}

static inline bool fetchCurrentPCHdmvEpochState(
  const HdmvEpochState * epoch_state,
  HdmvPCParameters * pc_ret,
  int64_t * pc_pts_ret
)
{
  return fetchPCHdmvEpochState(
    epoch_state, 0, pc_ret, pc_pts_ret
  );
}

void setActivePCHdmvEpochState(
  HdmvEpochState * epoch_state,
  HdmvPCParameters pc,
  int64_t pc_pts
);

/* ### Window Definitions : ################################################ */

static inline bool fetchCurrentWDHdmvEpochState(
  const HdmvEpochState * epoch_state,
  bool must_be_in_current_DS,
  HdmvWDParameters * wd_ret
)
{
  if (!epoch_state->window_def_present)
    return false;
  if (!epoch_state->window_def_provided && must_be_in_current_DS)
    return false;
  if (NULL != wd_ret)
    *wd_ret = epoch_state->window_def;
  return true;
}

void setWDHdmvEpochState(
  HdmvEpochState * epoch_state,
  const HdmvWDParameters wd
);

/* ### Interactive Compositions : ########################################## */

static inline bool fetchActiveICHdmvEpochState(
  const HdmvEpochState * epoch_state,
  HdmvICParameters * ic_ret,
  int64_t * ic_pts_ret
)
{
  if (!epoch_state->active_ic_present)
    return false;
  if (NULL != ic_ret)
    *ic_ret     = epoch_state->active_ic;
  if (NULL != ic_pts_ret)
    *ic_pts_ret = epoch_state->active_ic_pts;
  return true;
}

static inline bool fetchActiveICPageHdmvEpochState(
  const HdmvEpochState * epoch_state,
  uint8_t page_id,
  HdmvPageParameters * page_ret
)
{
  HdmvICParameters ic;
  if (!fetchActiveICHdmvEpochState(epoch_state, &ic, NULL))
    return false;
  if (ic.number_of_pages <= page_id)
    return false;
  if (NULL != page_ret)
    *page_ret = ic.pages[page_id];
  return true;
}

static inline bool compareICPageEpochState(
  const HdmvEpochState * epoch_state,
  const HdmvPageParameters page
)
{
  HdmvPageParameters prev_page;
  lb_cannot_fail(fetchActiveICPageHdmvEpochState(epoch_state, page.page_id, &prev_page));
  return constantHdmvPageParameters(prev_page, page);
}

int setActiveICHdmvEpochState(
  HdmvEpochState * epoch_state,
  HdmvICParameters ic,
  int64_t ic_pts
);

/* ### HDMV Segments Inventory Pool : ###################################### */

/** \~english
 * \brief HDMV elements inventory allocation pool initial number of segments.
 *
 */
#define HDMV_SEG_INV_POOL_DEFAULT_SEG_NB  4

/** \~english
 * \brief HDMV elements inventory allocation pool first segment size.
 *
 */
#define HDMV_SEG_INV_POOL_DEFAULT_SEG_SIZE  8

/** \~english
 * \brief HDMV elements inventory allocation pool.
 *
 * This structure is used to provide efficient memory allocation of used HDMV
 * structures. It is used to optimize allocation of an unpredictable number
 * of reusable structures. In HDMV context, structures can be reused at an
 * epoch start since the memory is supposed to be wiped out.
 *
 * elem_segments is a list of incrementally sized lists (called 'segments')
 * containing slots of size elem_size. First elem_segments list index (0) is a
 * segment of #HDMV_SEG_INV_POOL_DEFAULT_SEG_SIZE slots, each of elem_size
 * bytes (so a array of HDMV_SEG_INV_POOL_DEFAULT_SEG_SIZE * elem_size bytes).
 * Each following segment (next index of elem_segments array) size is the double of
 * the previous one.
 *
 * This is a illustration of the elem_segments 2-D array structure:<br />
 * [[0, 1, 2, 3, 4, 5, 6, 7, 8], [9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
 * 20, 21, 22, 23, 24], ...]
 */
typedef struct {
  void ** elem_segments;  /**< 2-D array of allocated elements. */
  size_t elem_size;   /**< Size of one allocated element in bytes. */
  size_t nb_allocated_elem_segments;  /**< Total elem_segments top array allocated size. */
  size_t nb_used_elem_segments;  /**< Current number of segments used in elem_segments. */
  size_t nb_rem_elem_in_seg;  /**< Number of remaining elements in currently used segment. */
} HdmvSegmentsInventoryPool;

static inline void initHdmvSegmentsInventoryPool(
  HdmvSegmentsInventoryPool * dst,
  size_t elem_size
)
{
  *dst = (HdmvSegmentsInventoryPool) {
    .elem_size = elem_size
  };
}

static inline void cleanHdmvSegmentsInventoryPool(
  HdmvSegmentsInventoryPool pool
)
{
  size_t i;

  for (i = 0; i < pool.nb_allocated_elem_segments; i++)
    free(pool.elem_segments[i]);
  free(pool.elem_segments);
}

static inline void resetHdmvSegmentsInventoryPool(
  HdmvSegmentsInventoryPool * pool
)
{
  pool->nb_used_elem_segments = 0;
  pool->nb_rem_elem_in_seg = 0;
}

#if 0
/* ### HDMV Segments Inventory : ########################################### */

typedef struct {
  HdmvSegmentsInventoryPool sequences;
  HdmvSegmentsInventoryPool segments;
} HdmvSegmentsInventory;

void initHdmvSegmentsInventory(
  HdmvSegmentsInventory * dst
);

#define CLEAN_POOL_ELEMENTS(p, t, c)                                          \
  do {                                                                        \
    size_t _i, _seg_sz, _j;                                                   \
    for (_i = 0; _i < (p).nb_allocated_elem_segments; _i++) {                 \
      if (NULL != (p).elem_segments[_i]) {                                    \
        _seg_sz = HDMV_SEG_INV_POOL_DEFAULT_SEG_SIZE << _i;                   \
        for (_j = 0; _j < _seg_sz; _j++)                                      \
          c(((t *) ((p).elem_segments[_i]))[_j]);                             \
      }                                                                       \
    }                                                                         \
  } while (0)

static inline void cleanHdmvSegmentsInventory(
  HdmvSegmentsInventory inv
)
{
  CLEAN_POOL_ELEMENTS(inv.sequences, HdmvSequence, cleanHdmvSequence);
  cleanHdmvSegmentsInventoryPool(inv.sequences);
  cleanHdmvSegmentsInventoryPool(inv.segments);
}

static inline void resetHdmvSegmentsInventory(
  HdmvSegmentsInventory * inv
)
{
  resetHdmvSegmentsInventoryPool(&inv->sequences);
  resetHdmvSegmentsInventoryPool(&inv->segments);
}

/* ###### Inventory content fetching functions : ########################### */

HdmvSequencePtr getHdmvSequenceHdmvSegmentsInventory(
  HdmvSegmentsInventory * inv
);

HdmvSegmentPtr getHdmvSegmentHdmvSegmentsInventory(
  HdmvSegmentsInventory * inv
);
#else

static inline HdmvSequencePtr getHdmvSequenceHdmvSegmentsInventory(
  void
)
{
  return malloc(sizeof(HdmvSequence));
}

static inline HdmvSegmentPtr getHdmvSegmentHdmvSegmentsInventory(
  void
)
{
  return malloc(sizeof(HdmvSegment));
}

#endif

#endif