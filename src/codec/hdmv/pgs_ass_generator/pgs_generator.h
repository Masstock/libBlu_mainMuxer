

#ifndef __LIBBLU_MUXER__CODECS__PGS_GENERATOR_H__
#define __LIBBLU_MUXER__CODECS__PGS_GENERATOR_H__

#include "../../../util.h"
#include "../common/hdmv_rectangle.h"
#include "../common/hdmv_bitmap_list.h"
#include "../common/hdmv_palette_gen.h"
#include "../common/hdmv_paletized_bitmap.h"

#include "pgs_merging_tree.h"
#include "pgs_frame.h"
#include "pgs_error.h"

int processPgsGenerator(
  const lbc *ass_filepath
);

#endif