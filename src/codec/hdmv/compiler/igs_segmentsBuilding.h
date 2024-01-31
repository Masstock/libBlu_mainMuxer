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
#include "../common/hdmv_palette.h"
#include "igs_compiler_data.h"

/* ### HDMV Segments building utilities : ################################## */

/** \~english
 * \brief HDMV Segments Building buffer context.
 *
 * This structure is used to create HDMV stream.
 */
typedef struct {
  FILE * out_fd;  /**< Output file descriptor.                               */

  uint8_t * data;           /**< HDMV segments construction buffer.          */
  uint32_t allocated_size;  /**< Buffer allocated length (in bytes).         */
  uint32_t used_size;       /**< Buffer used data length (in bytes).         */
} HdmvSegmentsBuildingContext;

int initHdmvSegmentsBuildingContext(
  HdmvSegmentsBuildingContext * dst,
  const lbc * out_filepath
);

/** \~english
 * \brief Release memory allocation done by
 * #createIgsCompilerSegmentsBuldingContext().
 *
 * \param ctx Context to free.
 */
static inline void cleanHdmvSegmentsBuildingContext(
  HdmvSegmentsBuildingContext ctx
)
{
  free(ctx.data);
}

/* ### HDMV segments : ##################################################### */

/* ###### Palette Definition Segment (0x14) : ############################## */

/* ###### Object Definition Segment (0x15) : ############################### */

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
 * segment #HDMV_SIZE_OD_SEGMENT_HEADER.
 *
 * Object Definition segments header is composed of:
 *  - header prefix #HDMV_SIZE_OBJECT_DESCRIPTOR.
 *  - sequence_descriptor() #HDMV_SIZE_SEQUENCE_DESCRIPTOR.
 */
#define HDMV_MAX_SIZE_OBJECT_DEFINITION_FRAGMENT                              \
  (HDMV_MAX_SIZE_SEGMENT_PAYLOAD - HDMV_SIZE_OD_SEGMENT_HEADER)

/* ###### Interactive Composition Segment (0x18) : ######################### */

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
 * header in each segment #HDMV_SIZE_IC_SEGMENT_HEADER.
 *
 * Interactive Compostion segments header is composed of:
 *  - video_descriptor() #HDMV_SIZE_VIDEO_DESCRIPTOR;
 *  - composition_descriptor() #HDMV_SIZE_VIDEO_DESCRIPTOR;
 *  - sequence_descriptor() #HDMV_SIZE_SEQUENCE_DESCRIPTOR.
 */
#define HDMV_MAX_SIZE_INTERACTIVE_COMPOSITION_FRAGMENT                        \
  (HDMV_MAX_SIZE_SEGMENT_PAYLOAD - HDMV_SIZE_IC_SEGMENT_HEADER)

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
