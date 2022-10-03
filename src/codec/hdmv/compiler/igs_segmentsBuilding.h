/** \~english
 * \file igs_segmentsBuilding.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief HDMV IGS Compiler segments bitstream building module.
 */

#ifndef __LIBBLU_MUXER__CODECS__IGS_COMPILER__SEGMENTS_BUILDING_H__
#define __LIBBLU_MUXER__CODECS__IGS_COMPILER__SEGMENTS_BUILDING_H__

#include "../common/hdmv_error.h"
#include "../common/hdmv_pictures_common.h"
#include "../common/hdmv_palette_def.h"
#include "igs_compiler_data.h"

/* ### HDMV Segments building utilities : ################################## */

/** \~english
 * \brief HDMV Segments Building buffer context.
 *
 * This structure is used to create HDMV stream.
 */
typedef struct {
  uint8_t * data;        /**< HDMV segments construction buffer.             */
  size_t allocatedData;  /**< Buffer allocated length (in bytes).            */
  size_t usedData;       /**< Buffer used data length (in bytes).            */
} HdmvSegmentsBuildingContext, *HdmvSegmentsBuildingContextPtr;

/** \~english
 * \brief Create a #HdmvSegmentsBuildingContext object.
 *
 * \return HdmvSegmentsBuildingContextPtr On success,
 * created context (NULL pointer otherwise).
 *
 * Allocated context must be freed using
 * #destroyIgsCompilerSegmentsBuldingContext().
 */
HdmvSegmentsBuildingContextPtr
createIgsCompilerSegmentsBuldingContext(
  void
);

/** \~english
 * \brief Release memory allocation done by
 * #createIgsCompilerSegmentsBuldingContext().
 *
 * \param ctx Context to free.
 */
void destroyIgsCompilerSegmentsBuldingContext(
  HdmvSegmentsBuildingContextPtr ctx
);

static inline size_t remainingBufferSizeHdmvSegmentsBuildingContext(
  const HdmvSegmentsBuildingContextPtr ctx
)
{
  assert(NULL != ctx);

  return ctx->allocatedData - ctx->usedData;
}

/** \~english
 * \brief Return true if no more data is writable in context.
 *
 * \param ctx Context to check.
 * \return true No more data can be written
 *  (used data length reached allocated, no allocated data).
 * \return false Space remains in buffer.
 *
 * This function can be used fir debugging to check if originaly
 * allocated data size match with result size.
 */
static inline bool isFullIgsCompilerSegmentsBuldingContext(
  const HdmvSegmentsBuildingContextPtr ctx
)
{
  return (0 == remainingBufferSizeHdmvSegmentsBuildingContext(ctx));
}

/** \~english
 * \brief Request a minimum remaining buffer size for segments building.
 *
 * \param ctx Context to edit.
 * \param requestedSize Requested buffer minimum size in bytes.
 * \return int Upon success, a zero value is returned. Otherwise, a negative
 * value is returned, indicating a memory allocation error.
 *
 * This function must be called before any writing using context.
 * If requested value is smaller than already allocated buffer, nothing
 * is done.
 */
int requestBufferCompilerSegmentsBuldingContext(
  HdmvSegmentsBuildingContextPtr ctx,
  size_t requiredSize
);

/** \~english
 * \brief Write supplied amount of bytes in reserved building buffer.
 *
 * \param ctx Destination building context.
 * \param data Data array of bytes.
 * \param size Size of the data array in bytes.
 * \return int Upon success, a zero value is returned. Otherwise, a negative
 * value is returned, indicating reserved buffer size is not large enough.
 *
 * Prior to any writing, #requestBufferCompilerSegmentsBuldingContext() shall
 * be used to reserve enough space.
 */
int writeBytesHdmvSegmentsBuildingContext(
  HdmvSegmentsBuildingContextPtr ctx,
  uint8_t * data,
  size_t size
);

/** \~english
 * \brief Write filled building buffer to supplied FILE pointer.
 *
 * \param ctx Source building context.
 * \param output Destination FILE handle.
 * \return int Upon success, a zero value is returned. Otherwise, a negative
 * value is returned, data writing failed.
 *
 * If the building buffer is empty, nothing is done.
 *
 * \note Building context is reset after writing.
 */
int writeBufferOnOutputIgsCompilerSegmentsBuldingContext(
  const HdmvSegmentsBuildingContextPtr ctx,
  FILE * output
);

/* ### HDMV segments : ##################################################### */

/** \~english
 * \brief Size required by segment header in bytes.
 *
 * Composed of:
 *  - u8  : segment_type;
 *  - u16 : segment_length.
 *
 * => 3 bytes.
 */
#define HDMV_SIZE_SEGMENT_HEADER  3

int writeSegmentHeader(
  HdmvSegmentsBuildingContextPtr ctx,
  uint8_t type,
  size_t length
);

/** \~english
 * \brief Maximum payload size of a segment in bytes.
 *
 * Payload is considered as all segment data content except
 * header.
 * Size is defined as the segment_length max possible value.
 */
#define HDMV_MAX_SIZE_SEGMENT_PAYLOAD  0xFFEF

/** \~english
 * \brief Maximum segment size in bytes.
 *
 * Size is defined as the segment_length max possible value
 * plus segment header length.
 */
#define HDMV_MAX_SIZE_SEGMENT                                                 \
  (HDMV_MAX_SIZE_SEGMENT_PAYLOAD + HDMV_SIZE_SEGMENT_HEADER)

/** \~english
 * \brief Size required by video_descriptor() structure in bytes.
 *
 * Composed of:
 *  - u16 : video_width;
 *  - u16 : video_height;
 *  - u4  : frame_rate_id;
 *  - v4  : reserved.
 *
 * => 5 bytes.
 */
#define HDMV_SIZE_VIDEO_DESCRIPTOR  5

int writeVideoDescriptorIgsCompiler(
  HdmvSegmentsBuildingContextPtr ctx,
  const HdmvVDParameters param
);

/** \~english
 * \brief Size required by composition_descriptor() structure in bytes.
 *
 * Composed of:
 *  - u16 : composition_number;
 *  - u8  : composition_state.
 *
 * => 3 bytes.
 */
#define HDMV_SIZE_COMPOSITION_DESCRIPTOR  3

int writeCompositionDescriptorIgsCompiler(
  HdmvSegmentsBuildingContextPtr ctx,
  const HdmvCDParameters param
);

/** \~english
 * \brief Size required by sequence_descriptor() structure in bytes.
 *
 * Composed of:
 *  - b1  : first_in_sequence;
 *  - b1  : last_in_sequence;
 *  - v6  : reserved.
 *
 * => 1 byte.
 */
#define HDMV_SIZE_SEQUENCE_DESCRIPTOR  1

int writeSequenceDescriptorIgsCompiler(
  HdmvSegmentsBuildingContextPtr ctx,
  bool first_in_sequence,
  bool last_in_sequence
);

/* ###### Palette Definition Segment (0x14) : ############################## */

/** \~english
 * \brief Size of Palette Definition Segment header prefix.
 *
 * Composed of:
 *  - u8  : palette_id;
 *  - u8  : palette_version_number.
 *
 * => 2 bytes.
 */
#define HDMV_SIZE_PALETTE_DEFINITION_HEADER_PREFIX  2

/** \~english
 * \brief Size required by Palette_entry() structure in bytes.
 *
 * Composed of:
 *  - u8  : palette_entry_id;
 *  - u8  : Y_value;
 *  - u8  : Cr_value;
 *  - u8  : Cb_value;
 *  - u8  : T_value.
 *
 * => 5 bytes.
 */
#define HDMV_SIZE_PALETTE_DEFINITION_ENTRY  5

static inline size_t computeSizeHdmvPaletteEntries(
  const HdmvPaletteDefinitionPtr pal
)
{
  return
    getNbEntriesHdmvPaletteDefinition(pal)
    * HDMV_SIZE_PALETTE_DEFINITION_ENTRY
  ;
}

static inline size_t computeSizeHdmvPaletteDefinitionSegments(
  const HdmvPaletteDefinitionPtr * palettes,
  unsigned nbPalettes
)
{
  size_t size;
  unsigned i;

  assert(NULL != palettes);

  size = 0;
  for (i = 0; i < nbPalettes; i++) {
    size +=
      HDMV_SIZE_SEGMENT_HEADER
      + HDMV_SIZE_PALETTE_DEFINITION_HEADER_PREFIX
      + computeSizeHdmvPaletteEntries(palettes[i])
    ;
  }

  return size;
}

uint8_t * buildPaletteDefinitionEntriesIgsCompiler(
  const HdmvPaletteDefinitionPtr pal,
  size_t * size
);

int writePaletteDefinitionSegHeaderIgsCompiler(
  HdmvSegmentsBuildingContextPtr ctx,
  uint8_t id,
  uint8_t versionNb
);

int writePaletteDefinitionSegmentsIgsCompiler(
  HdmvSegmentsBuildingContextPtr ctx,
  const IgsCompilerCompositionPtr compo
);

/* ###### Object Definition Segment (0x15) : ############################### */

/** \~english
 * \brief Length of Object Definition Segment header prefix.
 *
 * Composed of:
 *  - u16 : object_id;
 *  - u8  : object_version_number;
 *
 * => 3 bytes.
 */
#define HDMV_SIZE_OBJECT_DEFINITION_HEADER_PREFIX  3

/** \~english
 * \brief Length of Object Definition Segment header.
 *
 * Composed of:
 *  - header prefix #HDMV_SIZE_OBJECT_DEFINITION_HEADER_PREFIX;
 *  - sequence_descriptor() #HDMV_SIZE_SEQUENCE_DESCRIPTOR.
 */
#define HDMV_SIZE_OBJECT_DEFINITION_SEGMENT_HEADER                            \
  (HDMV_SIZE_OBJECT_DEFINITION_HEADER_PREFIX + HDMV_SIZE_SEQUENCE_DESCRIPTOR)

/** \~english
 * \brief Maximum object_data_fragment() structure size in bytes.
 *
 * An object_data() structure is slitted in object_data_fragment() structures
 * carried by continuous ODS (Object Definition Segment) segments.
 * If the structure is smaller than this value, it can be
 * carried in only one segment, otherwise, this value
 * must be used to compute how many segments are required.
 *
 * Size is defined as the value #HDMV_MAX_SIZE_SEGMENT_PAYLOAD
 * minus required object definition segments extra header in each
 * segment #HDMV_SIZE_OBJECT_DEFINITION_SEGMENT_HEADER.
 *
 * Object Definition segments header is composed of:
 *  - header prefix #HDMV_SIZE_OBJECT_DEFINITION_HEADER_PREFIX.
 *  - sequence_descriptor() #HDMV_SIZE_SEQUENCE_DESCRIPTOR.
 */
#define HDMV_MAX_SIZE_OBJECT_DEFINITION_FRAGMENT                              \
  (                                                                           \
    HDMV_MAX_SIZE_SEGMENT_PAYLOAD                                             \
    - HDMV_SIZE_OBJECT_DEFINITION_SEGMENT_HEADER                              \
  )

static inline size_t computeSizeHdmvObjectDefinitionSegments(
  const HdmvPicturePtr * objects,
  unsigned nbObjects
)
{
  size_t size;
  unsigned i;

  assert(NULL != objects);

  size = 0;
  for (i = 0; i < nbObjects; i++) {
    HdmvPicturePtr obj;
    HdmvODataParameters objParam;
    size_t objDefSize, nbSegments, extraPayload;

    obj = objects[i];
    objParam = (HdmvODataParameters) {
      .object_data_length = getRleSizeHdmvPicture(obj) + 4
    };

    objDefSize = computeSizeHdmvObjectData(objParam);
    nbSegments = objDefSize / HDMV_MAX_SIZE_OBJECT_DEFINITION_FRAGMENT;
    extraPayload = objDefSize % HDMV_MAX_SIZE_OBJECT_DEFINITION_FRAGMENT;

    size += nbSegments * HDMV_MAX_SIZE_SEGMENT;
    if (0 < extraPayload)
      size += (
        HDMV_SIZE_OBJECT_DEFINITION_SEGMENT_HEADER
        + HDMV_SIZE_SEGMENT_HEADER
        + extraPayload
      );
  }

  return size;
}

uint8_t * buildObjectDataIgsCompiler(
  const HdmvPicturePtr param,
  size_t * size
);

int writeObjectDefinitionSegHeaderIgsCompiler(
  HdmvSegmentsBuildingContextPtr ctx,
  uint16_t id,
  uint8_t versionNb
);

int writeObjectDefinitionSegmentsIgsCompiler(
  HdmvSegmentsBuildingContextPtr ctx,
  const IgsCompilerCompositionPtr compo
);

/* ###### Interactive Composition Segment (0x18) : ######################### */

/** \~english
 * \brief Size of Interactive Composition Segment header in bytes.
 *
 * Interactive Compostion segments header is composed of:
 *  - video_descriptor() #HDMV_SIZE_VIDEO_DESCRIPTOR;
 *  - composition_descriptor() #HDMV_SIZE_VIDEO_DESCRIPTOR;
 *  - sequence_descriptor() #HDMV_SIZE_SEQUENCE_DESCRIPTOR.
 */
#define HDMV_SIZE_INTERACTIVE_COMPOSITION_SEGMENT_HEADER                      \
  (                                                                           \
    HDMV_SIZE_VIDEO_DESCRIPTOR                                                \
    + HDMV_SIZE_COMPOSITION_DESCRIPTOR                                        \
    + HDMV_SIZE_SEQUENCE_DESCRIPTOR                                           \
  )

/** \~english
 * \brief Maximum interactive_composition_fragment() structure
 * size in bytes.
 *
 * An interactive_composition() structure is slitted in
 * interactive_composition_fragment() structures carried by
 * continuous segments.
 * If the structure is smaller than this value, it can be
 * carried in only one segment, otherwise, this value
 * must be used to compute how many segments are required.
 *
 * Size is defined as the value #HDMV_MAX_SIZE_SEGMENT_PAYLOAD
 * minus required interactive compostion segments extra
 * header in each segment #HDMV_SIZE_INTERACTIVE_COMPOSITION_SEGMENT_HEADER.
 *
 * Interactive Compostion segments header is composed of:
 *  - video_descriptor() #HDMV_SIZE_VIDEO_DESCRIPTOR;
 *  - composition_descriptor() #HDMV_SIZE_VIDEO_DESCRIPTOR;
 *  - sequence_descriptor() #HDMV_SIZE_SEQUENCE_DESCRIPTOR.
 */
#define HDMV_MAX_SIZE_INTERACTIVE_COMPOSITION_FRAGMENT                        \
  (                                                                           \
    HDMV_MAX_SIZE_SEGMENT_PAYLOAD                                             \
    - HDMV_SIZE_INTERACTIVE_COMPOSITION_SEGMENT_HEADER                        \
  )

/** \~english
 * \brief Computes and return size required by
 * interactive composition segments.
 *
 * \param param Structure parameters, if interaComposSize is non zero, this
 *  parameter can be ignored.
 * \param interaComposSize Optionnal pre-computed
 *  interactive_composition() structure length. If this value is zero,
 *  it is computed.
 * \return size_t Number of bytes required to store builded structure.
 *
 * Used to define how many bytes are required to store generated
 * Interactive Composition Segments.
 * A interactive_composition() may be splitted (because of its size)
 * between multiple ICS, this function return this global
 * size requirement.
 */
static inline size_t computeSizeHdmvInteractiveCompositionSegments(
  const HdmvICParameters * param,
  size_t interaComposSize
)
{
  size_t size, nbSegments, extraPayload;

  if (!interaComposSize) {
    assert(NULL != param);
    interaComposSize = computeSizeHdmvInteractiveComposition(param);
  }

  nbSegments =
    interaComposSize
    / HDMV_MAX_SIZE_INTERACTIVE_COMPOSITION_FRAGMENT
  ;
  extraPayload =
    interaComposSize
    % HDMV_MAX_SIZE_INTERACTIVE_COMPOSITION_FRAGMENT
  ;

  size = nbSegments * HDMV_MAX_SIZE_SEGMENT;

  if (0 < extraPayload)
    size += (
      HDMV_SIZE_SEGMENT_HEADER
      + HDMV_SIZE_INTERACTIVE_COMPOSITION_SEGMENT_HEADER
      + extraPayload
    );

  return size;
}

size_t appendButtonIgsCompiler(
  const HdmvButtonParam * param,
  uint8_t * arr,
  size_t off
);

size_t appendButtonOverlapGroupIgsCompiler(
  const HdmvButtonOverlapGroupParameters * param,
  uint8_t * arr,
  size_t off
);

size_t appendEffectSequenceIgsCompiler(
  const HdmvEffectSequenceParameters * param,
  uint8_t * arr,
  size_t off
);

size_t appendPageIgsCompiler(
  const HdmvPageParameters * param,
  uint8_t * arr,
  size_t off
);

uint8_t * buildInteractiveCompositionIgsCompiler(
  const HdmvICParameters * param,
  size_t * compositionSize
);

int writeInteractiveCompositionSegmentsIgsCompiler(
  HdmvSegmentsBuildingContextPtr ctx,
  const IgsCompilerCompositionPtr compo
);

/* ###### End Of Display Set Segment (0x80) : ############################## */

int writeEndOfDisplaySetSegment(
  HdmvSegmentsBuildingContextPtr ctx
);

/* ### Entry point : ####################################################### */

/** \~english
 * \brief Build IGS file from parameters.
 *
 * \param param IGS parameters.
 * \param outputFilename Output created file path.
 * \return int Zero on success, and negative value
 *  on memory allocation error.
 */
int buildIgsCompiler(
  IgsCompilerData * param,
  const lbc * outputFilename
);

#endif
