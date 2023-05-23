#include <stdio.h>
#include <stdlib.h>

#include "ac3_util.h"

int initAc3Context(
  Ac3Context * ctx,
  LibbluESParsingSettings * settings
)
{
  const LibbluESSettingsOptions options = settings->options;
  BitstreamReaderPtr es_bs = NULL;
  BitstreamWriterPtr script_bs = NULL;
  EsmsFileHeaderPtr script = NULL;
  unsigned src_file_idx;

  /* ES input */
  es_bs = createBitstreamReaderDefBuf(settings->esFilepath);
  if (NULL == es_bs)
    LIBBLU_AC3_ERROR_FRETURN("Unable to open input ES.\n");

  /* Script output */
  script_bs = createBitstreamWriterDefBuf(settings->scriptFilepath);
  if (NULL == script_bs)
    LIBBLU_AC3_ERROR_FRETURN("Unable to create output script file.\n");

  /* Prepare ESMS output file */
  script = createEsmsFileHandler(ES_AUDIO, options, FMT_SPEC_INFOS_AC3);
  if (NULL == script)
    LIBBLU_AC3_ERROR_FRETURN("Unable to create output script handler.\n");

  if (appendSourceFileEsms(script, settings->esFilepath, &src_file_idx) < 0)
    LIBBLU_AC3_ERROR_FRETURN("Unable to add source file to script handler.\n");

  /* Save in context */
  *ctx = (Ac3Context) {
    .src_file_idx = src_file_idx,
    .bs = es_bs,
    .script_bs = script_bs,
    .script = script,
    .script_fp = settings->scriptFilepath,

    .skip_ext = options.extractCore
  };

  return 0;

free_return:
  closeBitstreamWriter(script_bs);
  destroyEsmsFileHandler(script);

  return -1;
}

typedef enum {
  AC3,
  TRUEHD,
  EAC3
} Ac3StreamType;

static LibbluStreamCodingType _codingTypeAc3StreamType(
  Ac3StreamType stream_type
)
{
  static LibbluStreamCodingType types[] = {
    STREAM_CODING_TYPE_AC3,
    STREAM_CODING_TYPE_TRUEHD,
    STREAM_CODING_TYPE_EAC3
  };

  return types[stream_type];
}

static double _peakBitrateAc3StreamType(
  Ac3StreamType stream_type
)
{
  static double bitrates[] = {
    64000.,     // AC-3
    18640000.,  // TrueHD
    4736000.,   // EAC-3 as primary audio
    256000.     // EAC-3 as secondary audio.
  };

  return bitrates[stream_type];
}

static int _detectStreamType(
  Ac3Context * ctx,
  Ac3StreamType * type
)
{

  if (!ctx->core.nb_frames) // TODO: Support secondary audio
    LIBBLU_AC3_ERROR_RETURN("Missing mandatory AC-3 core.\n");

  if (ctx->mlp.nb_frames && !ctx->skip_ext)
    *type = TRUEHD; // TrueHD
  else
    LIBBLU_TODO();

  return 0;
}

static int _writeScriptEndMarker(
  Ac3Context * ctx
)
{
  /* [v8 endMarker] */
  if (writeByte(ctx->script_bs, ESMS_SCRIPT_END_MARKER) < 0)
    LIBBLU_AC3_ERROR_RETURN("Unable to write script end marker.\n");
  return 0;
}

static uint8_t _detectAudioFormat(
  Ac3Context * ctx,
  Ac3StreamType stream_type
)
{
  unsigned ext_nb_channels = 0;

  if (stream_type == TRUEHD) {
    const MlpMajorSyncInfoParameters * msi = &ctx->mlp.mlp_sync_header.major_sync_info;

    ext_nb_channels = MAX(ext_nb_channels, getNbChannels6ChPresentationAssignment(
      msi->format_info.u6ch_presentation_channel_assignment
    ));
    ext_nb_channels = MAX(ext_nb_channels, getNbChannels8ChPresentationAssignment(
      msi->format_info.u8ch_presentation_channel_assignment,
      msi->flags.fmt_info_alter_8ch_asgmt_syntax
    ));
  }

  /* Core : */
  uint8_t audio_format;
  switch (ctx->core.bsi.acmod) {
    case AC3_ACMOD_CH1_CH2:
      audio_format = 0x02; break;
    case AC3_ACMOD_C:
      audio_format = 0x01; break;
    case AC3_ACMOD_L_R:
    case AC3_ACMOD_L_C_R: // FIXME: Is L/C/R stereo?
      audio_format = 0x03; break;
    default:
      audio_format = 0x06; // Multichannel
  }

  if (audio_format <= 0x03 && 2 < ext_nb_channels)
    return 0x0C; // Stereo core + Multichannel extension.
  return audio_format;
}

static SampleRateCode _detectSampleRateCode(
  Ac3Context * ctx,
  Ac3StreamType stream_type
)
{
  unsigned ext_audio_freq = 0;

  if (stream_type == TRUEHD) {
    const MlpMajorSyncInfoParameters * msi = &ctx->mlp.mlp_sync_header.major_sync_info;
    ext_audio_freq = sampleRateMlpAudioSamplingFrequency(msi->format_info.audio_sampling_frequency);
  }

  switch (ext_audio_freq) {
    case 96000:
      return 0x04; // 96 kHz
    case 192000:
      return 0x0E; // 192 kHz + 48 kHz core
    default:
      return 0x01; // 48 kHz
  }
}

static void _setLibbluESProperties(
  Ac3Context * ctx,
  LibbluESProperties * es_prop,
  Ac3StreamType stream_type
)
{
  *es_prop = (LibbluESProperties) {
    .type = ES_AUDIO,
    .codingType  = _codingTypeAc3StreamType(stream_type),

    .audioFormat = _detectAudioFormat(ctx, stream_type),
    .sampleRate  = _detectSampleRateCode(ctx, stream_type),
    .bitDepth    = 16, // Always 16 bit core

    .bitrate     = _peakBitrateAc3StreamType(stream_type)
  };
}

static void _setLibbluESAc3SpecProperties(
  Ac3Context * ctx,
  LibbluESAc3SpecProperties * ac3_prop
)
{
  *ac3_prop = (LibbluESAc3SpecProperties) {
    .sample_rate_code = get_sample_rate_code_ac3_descriptor(ctx->core.syncinfo.fscod),
    .bsid             = ctx->core.bsi.bsid,
    .bit_rate_code    = get_bit_rate_code_ac3_descriptor(ctx->core.syncinfo.frmsizecod),
    .surround_mode    = ctx->core.bsi.dsurmod,
    .bsmod            = ctx->core.bsi.bsmod,
    .num_channels     = get_num_channels_ac3_descriptor(ctx->core.bsi.acmod),
    .full_svc         = false // FIXME
  };
}

static void _setScriptProperties(
  Ac3Context * ctx,
  Ac3StreamType stream_type
)
{
  EsmsFileHeaderPtr script = ctx->script;

  assert(NULL != script->fmtSpecProp.ac3);

  _setLibbluESProperties(ctx, &script->prop, stream_type);
  _setLibbluESAc3SpecProperties(ctx, script->fmtSpecProp.ac3);

  script->endPts = ctx->core.pts;
}

int completeAc3Context(
  Ac3Context * ctx
)
{
  Ac3StreamType stream_type;

  if (_detectStreamType(ctx, &stream_type) < 0)
    return -1;

  if (_writeScriptEndMarker(ctx) < 0)
    return -1;

  _setScriptProperties(ctx, stream_type);

  if (addEsmsFileEnd(ctx->script_bs, ctx->script) < 0)
    return -1;
  closeBitstreamWriter(ctx->script_bs);
  ctx->script_bs = NULL;

  if (updateEsmsHeader(ctx->script_fp, ctx->script) < 0)
    return -1;

  cleanAc3Context(ctx);
  return 0;
}

void cleanAc3Context(
  Ac3Context * ctx
)
{
  closeBitstreamReader(ctx->bs);
  closeBitstreamWriter(ctx->script_bs);
  destroyEsmsFileHandler(ctx->script);

  cleanMlpParsingContext(&ctx->mlp);
}