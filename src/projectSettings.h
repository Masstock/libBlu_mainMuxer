
#ifndef __LIBBLU_MUXER__PROJECT_SETTINGS_H__
#define __LIBBLU_MUXER__PROJECT_SETTINGS_H__

#include "muxingSettings.h"
#include "bd/bd_clpi.h"

typedef enum {
  LIBBLU_SINGLE_MUX,
  LIBBLU_DISC_PROJECT
} LibbluProjectType;



// typedef struct {
//   union {
//     BDCLPI prefilled_CLPI;
//     struct {
//       uint8_t application_type;

//       uint8_t *following_clips[5];
//       uint8_t nb_following_clips;
//     };
//   };

//   LibbluMuxingSettings mux_settings;
// } LibbluClipSettings;

typedef struct {
  // TODO

  BDCLPI *clip_information_file;
  LibbluMuxingSettings *clip_mux_settings;
  unsigned nb_clips;

  LibbluMuxingOptions shared_mux_options;
} LibbluDiscProjectSettings;

static inline void cleanLibbluDiscProjectSettings(
  LibbluDiscProjectSettings disc_project_settings
)
{
  for (unsigned i = 0; i < disc_project_settings.nb_clips; i++) {
    cleanBDCLPI(disc_project_settings.clip_information_file[i]);
    cleanLibbluMuxingSettings(disc_project_settings.clip_mux_settings[i]);
  }
  free(disc_project_settings.clip_information_file);
  free(disc_project_settings.clip_mux_settings);
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