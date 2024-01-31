#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include "stream.h"

LibbluStreamPtr createLibbluStream(
  StreamType type,
  uint16_t pid
)
{

  LibbluStreamPtr stream;
  if (NULL == (stream = (LibbluStreamPtr) calloc(1, sizeof(LibbluStream))))
    LIBBLU_ERROR_NRETURN("Memory allocation error.\n");

  stream->type = type;
  stream->pid = pid;

  return stream;
}

void destroyLibbluStream(
  LibbluStreamPtr stream
)
{
  if (NULL == stream)
    return;

  if (isESLibbluStream(stream))
    cleanLibbluES(stream->es);
  else
    cleanLibbluSystemStream(stream->sys);

  free(stream);
}

int requestESPIDLibbluStream(
  LibbluPIDValues * pid_vals,
  uint16_t * pid_ret,
  LibbluStreamPtr stream
)
{
  assert(NULL != pid_vals);
  assert(NULL != pid_ret);
  assert(NULL != stream);
  assert(isESLibbluStream(stream));

  const LibbluESSettings * es_sets = stream->es.settings_ref;
  const LibbluESProperties * es_prop = &stream->es.prop;

  /* Get the base PID and the current number of ES of stream type */
  uint16_t sel_es_type_base_pid;
  unsigned * sel_es_type_cur_nb_ptr, sel_es_type_limit;
  const char * sel_es_type_str;

  if (es_prop->type == ES_VIDEO) {
    if (!es_sets->options.secondaryStream) {
      sel_es_type_base_pid   = PRIM_VIDEO_PID;
      sel_es_type_cur_nb_ptr = &pid_vals->nb_streams.primaryVideo;
      sel_es_type_limit      = pid_vals->limits.primaryVideo;
      sel_es_type_str        = "Primary Video";
    }
    else {
      sel_es_type_base_pid   = SEC_VIDEO_PID;
      sel_es_type_cur_nb_ptr = &pid_vals->nb_streams.secondaryVideo;
      sel_es_type_limit      = pid_vals->limits.secondaryVideo;
      sel_es_type_str        = "Secondary Video";
    }
  }
  else if (es_prop->type == ES_AUDIO) {
    if (!es_sets->options.secondaryStream) {
      sel_es_type_base_pid   = PRIM_AUDIO_PID;
      sel_es_type_cur_nb_ptr = &pid_vals->nb_streams.primaryAudio;
      sel_es_type_limit      = pid_vals->limits.primaryAudio;
      sel_es_type_str        = "Primary Audio";
    }
    else {
      sel_es_type_base_pid   = SEC_AUDIO_PID;
      sel_es_type_cur_nb_ptr = &pid_vals->nb_streams.secondaryAudio;
      sel_es_type_limit      = pid_vals->limits.secondaryAudio;
      sel_es_type_str        = "Secondary Audio";
    }
  }
  else if (es_prop->coding_type == STREAM_CODING_TYPE_PG) {
    sel_es_type_base_pid     = PG_PID;
    sel_es_type_cur_nb_ptr   = &pid_vals->nb_streams.pg;
    sel_es_type_limit        = pid_vals->limits.pg;
    sel_es_type_str          = "Presentation Graphics (PG) subtitles";
  }
  else if (es_prop->coding_type == STREAM_CODING_TYPE_IG) {
    sel_es_type_base_pid     = IG_PID;
    sel_es_type_cur_nb_ptr   = &pid_vals->nb_streams.ig;
    sel_es_type_limit        = pid_vals->limits.ig;
    sel_es_type_str          = "Interactive Graphics (IG) menus";
  }
  else if (es_prop->coding_type == STREAM_CODING_TYPE_TEXT) {
    sel_es_type_base_pid     = TXT_SUB_PID;
    sel_es_type_cur_nb_ptr   = &pid_vals->nb_streams.txtSubtitles;
    sel_es_type_limit        = pid_vals->limits.txtSubtitles;
    sel_es_type_str          = "Text Subtitles (TextST)";
  }
  else
    LIBBLU_ERROR_RETURN(
      "Unknown stream coding type, unable to select a PID value.\n"
    );

  /* Check number of already registered ES */
  if (sel_es_type_limit <= *sel_es_type_cur_nb_ptr)
    LIBBLU_ERROR_RETURN(
      "Too many %s streams.\n",
      sel_es_type_str
    );

  uint16_t sel_pid = sel_es_type_base_pid;
  if (es_sets->pid != 0x0000) {
    /* Custom requested PID */
    bool valid = true;

    sel_pid = es_sets->pid;

    if (isReservedPIDValue(sel_pid)) {
      if (pid_vals->fail_on_invalid_request)
        LIBBLU_ERROR_RETURN(
          "Unable to use PID value 0x%04" PRIX16 ", this value is reserved.\n",
          es_sets->pid
        );
      sel_pid = 0x0000;
    }

    if (sel_pid < sel_es_type_base_pid || sel_es_type_base_pid + sel_es_type_limit <= sel_pid) {
      /* Out of range PID request */
      if (pid_vals->fail_on_invalid_request)
        LIBBLU_ERROR_RETURN(
          "Unable to use PID value 0x%04" PRIX16 ", it is not in the allowed "
          "range for a %s stream.\n",
          es_sets->pid,
          sel_es_type_str
        );

      sel_pid = sel_es_type_base_pid;
      valid = false;
    }

    if (!valid)
      LIBBLU_INFO(
        "Unable to use PID value 0x%04" PRIX16 ", "
        ", selection of a replacement available value.\n",
        es_sets->pid,
        sel_pid
      );
  }

  bool is_valid_sel_pid = false;
  while (!is_valid_sel_pid) {
    /* Check PID value range */
    if (sel_es_type_base_pid + sel_es_type_limit <= sel_pid)
      LIBBLU_ERROR_RETURN(
        "Unable to select a valid PID value for a %s stream.\n",
        sel_es_type_str
      );

    /* Try to insert value */
    switch (insertLibbluRegisteredPIDValues(&pid_vals->reg_values, sel_pid)) {
    case 0: /* Unable to insert value */
      sel_pid++;
      break;

    case 1: /* Insertion successfull */
      is_valid_sel_pid = true;
      break;

    default: /* Error */
      return -1;
    }
  }

  if (NULL != pid_ret)
    *pid_ret = sel_pid;
  (*sel_es_type_cur_nb_ptr)++;

  return 0;
}