

#ifndef __LIBBLU_MUXER__CODECS__HDMV__COMMON__BUILDER_H__
#define __LIBBLU_MUXER__CODECS__HDMV__COMMON__BUILDER_H__

#include "hdmv_builder_data.h"

/** \~english
 * \brief HDMV Segments Building buffer context.
 *
 * This structure is used to create HDMV stream.
 */
typedef struct {
  lbc *out_fp;   /**< Output file name.                                     */
  FILE *out_fd;  /**< Output file descriptor.                               */

  uint8_t *data;           /**< HDMV segments construction buffer.          */
  uint32_t allocated_size;  /**< Buffer allocated length (in bytes).         */
  uint32_t used_size;       /**< Buffer used data length (in bytes).         */
} HdmvBuilderContext;

int initHdmvBuilderContext(
  HdmvBuilderContext *dst,
  const lbc *out_filepath
);

int cleanHdmvBuilderContext(
  HdmvBuilderContext builder_ctx
);

int buildIGSDisplaySet(
  HdmvBuilderContext *builder_ctx,
  const HdmvBuilderIGSDSData ds_data
);

int buildPGSDisplaySet(
  HdmvBuilderContext *builder_ctx,
  const HdmvBuilderPGSDSData ds_data
);

#endif