
#ifndef __LIBBLU_MUXER__PROJECT_SETTINGS_H__
#define __LIBBLU_MUXER__PROJECT_SETTINGS_H__

#include "muxingSettings.h"
#include "bd/bd_clpi.h"

typedef enum {
  LIBBLU_SINGLE_MUX,
  LIBBLU_DISC_PROJECT
} LibbluProjectType;

typedef struct {
  lbc *output_path;
  BDClip_information_file_name output_stem;

  bool has_prefilled_CLPI;
  union {
    BDCLPI prefilled_CLPI;
    struct {
      uint8_t application_type;  /**< Type of clip usage. One of
        BD_APPLICATION_TYPE__*. */

      BDClip_information_file_name *following_clips;  /**< Stem of clips
        following this one and intended for a seamless connection (for which
        an ATC delta must be computed). */
      uint8_t nb_following_clips;   /**< Number of clips following this one
        and connected seamlessly. */
    };
  };

  LibbluMuxingSettings mux_settings;
} LibbluClipSettings;

static inline void cleanLibbluClipSettings(
  LibbluClipSettings clip_settings
)
{
  free(clip_settings.output_path);
  if (clip_settings.has_prefilled_CLPI)
    cleanBDCLPI(clip_settings.prefilled_CLPI);
  else {
    free(clip_settings.following_clips);
  }
  cleanLibbluMuxingSettings(clip_settings.mux_settings);
}

typedef struct {
  // TODO

  LibbluClipSettings *clips;
  unsigned nb_clips;

  LibbluMuxingOptions shared_mux_options;
} LibbluDiscProjectSettings;

static inline void cleanLibbluDiscProjectSettings(
  LibbluDiscProjectSettings disc_project_settings
)
{
  for (unsigned i = 0; i < disc_project_settings.nb_clips; i++) {
    cleanLibbluClipSettings(disc_project_settings.clips[i]);
  }
  free(disc_project_settings.clips);
}

typedef struct {
  LibbluProjectType type;

  union {
    LibbluMuxingSettings single_mux_settings;
    LibbluDiscProjectSettings disc_project_settings;
  };
} LibbluProjectSettings;

static inline void cleanLibbluProjectSettings(
  LibbluProjectSettings project_settings
)
{
  if (LIBBLU_SINGLE_MUX == project_settings.type)
    cleanLibbluMuxingSettings(project_settings.single_mux_settings);
  else {
    assert(LIBBLU_DISC_PROJECT == project_settings.type);
    cleanLibbluDiscProjectSettings(project_settings.disc_project_settings);
  }
}

#endif