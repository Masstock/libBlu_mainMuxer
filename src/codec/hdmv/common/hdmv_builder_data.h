

#ifndef __LIBBLU_MUXER__CODECS__HDMV__COMMON__BUILDER_DATA_H__
#define __LIBBLU_MUXER__CODECS__HDMV__COMMON__BUILDER_DATA_H__

#include "hdmv_data.h"
#include "hdmv_object.h"
#include "hdmv_palette.h"

typedef struct {
  HdmvVDParameters video_descriptor;
  HdmvCDParameters composition_descriptor;
  HdmvICParameters interactive_composition;

  const HdmvPalette *palettes;
  unsigned nb_palettes;

  const HdmvObject *objects;
  unsigned nb_objects;
} HdmvBuilderIGSDSData;

typedef struct {
  HdmvVDParameters video_descriptor;
  HdmvCDParameters composition_descriptor;
  HdmvPCParameters presentation_composition;

  HdmvWDParameters window_definition;
  bool window_definition_present;

  const HdmvPalette *palettes;
  unsigned nb_palettes;

  const HdmvObject *objects;
  unsigned nb_objects;
} HdmvBuilderPGSDSData;

#endif