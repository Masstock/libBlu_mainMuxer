#include <stdio.h>
#include <stdlib.h>

#include "ac3_util.h"

int initAc3Context(
  Ac3Context *ctx,
  LibbluESParsingSettings *settings
)
{
  const LibbluESSettingsOptions options = settings->options;
  BitstreamReaderPtr es_bs = NULL;
  BitstreamWriterPtr script_bs = NULL;
  EsmsHandlerPtr script = NULL;
  uint8_t src_file_idx;

  /* ES input */
  es_bs = createBitstreamReaderDefBuf(settings->esFilepath);
  if (NULL == es_bs)
    LIBBLU_AC3_ERROR_FRETURN("Unable to open input ES.\n");

  /* Script output */
  script_bs = createBitstreamWriterDefBuf(settings->scriptFilepath);
  if (NULL == script_bs)
    LIBBLU_AC3_ERROR_FRETURN("Unable to create output script file.\n");

  /* Prepare ESMS output file */
  script = createEsmsHandler(ES_AUDIO, options, FMT_SPEC_INFOS_AC3);
  if (NULL == script)
    LIBBLU_AC3_ERROR_FRETURN("Unable to create output script handler.\n");

  if (appendSourceFileEsmsHandler(script, settings->esFilepath, &src_file_idx) < 0)
    LIBBLU_AC3_ERROR_FRETURN("Unable to add source file to script handler.\n");

  /* Write script header */
  if (writeEsmsHeader(script_bs) < 0)
    return -1;

  /* Save in context */
  *ctx = (Ac3Context) {
    .src_file_idx = src_file_idx,
    .bs = es_bs,
    .script_bs = script_bs,
    .script = script,
    .script_fp = settings->scriptFilepath,

    .skip_ext = options.extract_core
  };

  return 0;

free_return:
  closeBitstreamWriter(script_bs);
  destroyEsmsHandler(script);

  return -1;
}

static uint8_t _getSyncFrameBsid(
  BitstreamReaderPtr bs
)
{
  uint64_t value = nextUint64(bs);

  /* [v40 *variousBits*] [u5 bsid] ... */
  return (value >> 19) & 0x1F;
}


/** \~english
 * \brief Return 'format_sync' field from 'major_sync_info()' if present.
 *
 * \param input
 * \return uint32_t
 */
static uint32_t _getMlpMajorSyncFormatSyncWord(
  BitstreamReaderPtr input
)
{
  /* [v4 check_nibble] [u12 access_unit_length] [u16 input_timing] */
  /* [v32 format_sync] */
  return (nextUint64(input) & 0xFFFFFFFF);
}


static bool _isPresentMlpMajorSyncFormatSyncWord(
  BitstreamReaderPtr input
)
{
  uint32_t format_sync = _getMlpMajorSyncFormatSyncWord(input) >> 8;

  return (MLP_SYNCWORD_PREFIX == format_sync);
}


Ac3ContentType initNextFrameAc3Context(
  Ac3Context *ctx
)
{
  Ac3ContentType type;

  if (nextUint16(ctx->bs) == AC3_SYNCWORD) {
    /* AC-3 Core / E-AC-3 */
    uint8_t bsid = _getSyncFrameBsid(ctx->bs);

    LIBBLU_AC3_DEBUG_UTIL(
      "Next frame syncword 0x%04X with bsid %u.\n",
      AC3_SYNCWORD, bsid
    );

    if (bsid <= 8) {
      /* AC-3 bit stream syntax */
      type = AC3_CORE;
    }

    else if (11 <= bsid && bsid <= 16) {
      /* Enhanced AC-3 bit stream syntax */
      type = AC3_EAC3;
    }

    else {
      /* Invalid/Unsupported bsid */
      LIBBLU_AC3_ERROR_RETURN(
        "Invalid/Unsupported 'bsid' value %u in AC-3 sync.\n",
        bsid
      );
    }
  }

  else if (ctx->contains_mlp || _isPresentMlpMajorSyncFormatSyncWord(ctx->bs)) {
    if (!ctx->contains_mlp) {
      LIBBLU_AC3_DEBUG_UTIL("Detection of MLP major sync word.\n");
      ctx->contains_mlp = true;
    }

    /* Expect MLP/TrueHD Access Unit */
    uint64_t next_words = nextUint64(ctx->bs);

    LIBBLU_AC3_DEBUG_UTIL(
      "Next frame words 0x%016X.\n",
      next_words
    );

    if (!ctx->mlp.nb_frames) {
      /* Expect first Access Unit to contain major sync */
      if (((next_words >> 8) & 0xFFFFFF) == MLP_SYNCWORD_PREFIX) {
        type = AC3_TRUEHD;
      }
      else {
        // TODO: Add support for Dolby_TrueHD_file syntax described in [3].
        LIBBLU_TODO_MSG(
          "Dolby_TrueHD_file defined in "
          "'TrueHD - Dolby TrueHD (MLP) - High-level bitstream description' "
          "is not yet supported.\n"
        );
      }
    }
    else {
      /* Access Unit may contain minor sync, meaning no magic word */
      type = AC3_TRUEHD;
    }
  }

  else {
    LIBBLU_AC3_ERROR_RETURN(
      "Unknown sync word 0x%016" PRIX64 " at 0x%" PRIX64 ".\n",
      nextUint64(ctx->bs),
      tellPos(ctx->bs)
    );
  }

  LIBBLU_AC3_DEBUG_UTIL(
    "Next frame initialized with %s content detected.\n",
    Ac3ContentTypeStr(type)
  );

  ctx->cur_au = (Ac3AccessUnit) {
    .type = type,
    .start_offset = tellPos(ctx->bs)
  };

  return type;
}


static int _reallocBitReaderBuffer(
  Ac3Context *ctx,
  size_t new_size
)
{
  /* Reallocate backing buffer */
  uint8_t *new_buf = realloc(ctx->buffer, new_size);
  if (NULL == new_buf)
    LIBBLU_ERROR_RETURN("Memory allocation error.\n");

  ctx->buffer = new_buf;
  ctx->buffer_size = new_size;

  return 0;
}


int fillAc3BitReaderAc3Context(
  Ac3Context *ctx,
  const Ac3SyncInfoParameters *si
)
{
  // TODO: Check minimal frame_size value.
  unsigned frame_size = framesize_frmsizecod(si->frmsizecod, si->fscod) << 1;
  unsigned frame_data_size = frame_size - 5; // minus syncinfo size.

  if (ctx->buffer_size < frame_data_size) {
    /* Reallocate backing buffer */
    if (_reallocBitReaderBuffer(ctx, frame_data_size) < 0)
      return -1;
  }

  /* Read Sync frame data */
  if (readBytes(ctx->bs, ctx->buffer, frame_data_size) < 0)
    LIBBLU_AC3_ERROR_RETURN("Unable to read sync frame from source file.\n");
  initLibbluBitReader(&ctx->br, ctx->buffer, frame_data_size << 3);

  ctx->cur_au.size = frame_size;
  return 0;
}


static unsigned _getFrmsizEac3(
  BitstreamReaderPtr bs
)
{
  uint32_t value = nextUint32(bs);

  /* [v21 *variousBits*] [u11 frmsiz] */
  return (value & 0x7FF);
}


int fillEac3BitReaderAc3Context(
  Ac3Context *ctx
)
{
  // TODO: Check minimal frame_size value.
  unsigned frame_size = (_getFrmsizEac3(ctx->bs) + 1) << 1; // 16-bit words.

  if (ctx->buffer_size < frame_size) {
    /* Reallocate backing buffer */
    if (_reallocBitReaderBuffer(ctx, frame_size) < 0)
      return -1;
  }

  /* Read Sync frame data */
  if (readBytes(ctx->bs, ctx->buffer, frame_size) < 0)
    LIBBLU_AC3_ERROR_RETURN("Unable to read sync frame from source file.\n");
  initLibbluBitReader(&ctx->br, ctx->buffer, frame_size << 3);

  ctx->cur_au.size = frame_size;
  return 0;
}


int fillMlpBitReaderAc3Context(
  Ac3Context *ctx,
  const MlpSyncHeaderParameters *sh
)
{

  // TODO: Check minimal access_unit_length value.
  unsigned access_unit_size = sh->access_unit_length << 1;
  unsigned access_unit_data_size = access_unit_size - 4;

  if (ctx->buffer_size < access_unit_data_size) {
    /* Reallocate backing buffer */
    if (_reallocBitReaderBuffer(ctx, access_unit_data_size) < 0)
      return -1;
  }

  /* Read Access Unit data */
  if (readBytes(ctx->bs, ctx->buffer, access_unit_data_size) < 0)
    LIBBLU_MLP_ERROR_RETURN("Unable to read access unit from source file.\n");
  initLibbluBitReader(&ctx->br, ctx->buffer, access_unit_data_size << 3);

  ctx->cur_au.size = access_unit_size;
  return 0;
}


int completeFrameAc3Context(
  Ac3Context *ctx
)
{
  const Ac3AccessUnit *au = &ctx->cur_au;
  unsigned sfile_idx = ctx->src_file_idx;

  switch (au->type) {
  case AC3_CORE:
    if (initAudioPesPacketEsmsHandler(ctx->script, false, false, ctx->core.pts, 0) < 0)
      return -1;
    break;

  case AC3_EAC3:
    if (ctx->skip_ext)
      return 0; // Skip extensions.
    if (initAudioPesPacketEsmsHandler(ctx->script, true, false, ctx->eac3.pts, 0) < 0)
      return -1;
    break;

  case AC3_TRUEHD:
    if (ctx->skip_ext)
      return 0; // Skip extensions.
    if (initAudioPesPacketEsmsHandler(ctx->script, true, false, ctx->mlp.pts, 0) < 0)
      return -1;
    break;
  }

  if (appendAddPesPayloadCommandEsmsHandler(ctx->script, sfile_idx, 0x0, au->start_offset, au->size) < 0)
    return -1;

  if (writePesPacketEsmsHandler(ctx->script_bs, ctx->script) < 0)
    return -1;

  return 0;
}


static LibbluStreamCodingType _codingTypeAc3FrameType(
  Ac3ContentType stream_type
)
{
  static LibbluStreamCodingType types[] = {
    STREAM_CODING_TYPE_AC3,
    STREAM_CODING_TYPE_EAC3,
    STREAM_CODING_TYPE_TRUEHD
  };

  return types[stream_type];
}

static double _peakBitrateAc3FrameType(
  Ac3ContentType stream_type
)
{
  static double bitrates[] = {
    64000.,     // AC-3
    4736000.,   // EAC-3 as primary audio
    18640000.,  // TrueHD
    256000.     // EAC-3 as secondary audio.
  };

  return bitrates[stream_type];
}

static int _detectStreamType(
  Ac3Context *ctx,
  Ac3ContentType *type
)
{

  if (!ctx->core.nb_frames) // TODO: Support secondary audio
    LIBBLU_AC3_ERROR_RETURN("Missing mandatory AC-3 core.\n");

  if (!ctx->skip_ext && ctx->mlp.nb_frames)
    *type = AC3_TRUEHD; // TrueHD
  else if (!ctx->skip_ext && ctx->eac3.nb_frames)
    *type = AC3_EAC3; // EAC-3
  else
    *type = AC3_CORE;

  return 0;
}

static uint8_t _detectAudioFormat(
  Ac3Context *ctx,
  Ac3ContentType stream_type
)
{
  unsigned ext_nb_channels = 0;

  if (stream_type == AC3_TRUEHD) {
    const MlpMajorSyncInfoParameters *msi = &ctx->mlp.mlp_sync_header.major_sync_info;

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
  if (ctx->core.bsi.acmod == AC3_ACMOD_C)
    audio_format = 0x01; // Mono
  else if (ctx->core.bsi.acmod == AC3_ACMOD_L_R && !ctx->core.bsi.lfeon)
    audio_format = 0x03; // Stereo
  else
    audio_format = 0x06; // Multichannel

  if (audio_format <= 0x03 && 2 < ext_nb_channels)
    return 0x0C; // Stereo core + Multichannel extension.
  return audio_format;
}

static SampleRateCode _detectSampleRateCode(
  Ac3Context *ctx,
  Ac3ContentType stream_type
)
{
  unsigned ext_audio_freq = 0;

  if (stream_type == AC3_TRUEHD) {
    const MlpMajorSyncInfoParameters *msi = &ctx->mlp.mlp_sync_header.major_sync_info;
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
  Ac3Context *ctx,
  LibbluESProperties *es_prop,
  Ac3ContentType stream_type
)
{
  *es_prop = (LibbluESProperties) {
    .type         = ES_AUDIO,
    .coding_type  = _codingTypeAc3FrameType(stream_type),

    .audio_format = _detectAudioFormat(ctx, stream_type),
    .sample_rate  = _detectSampleRateCode(ctx, stream_type),
    .bit_depth    = BIT_DEPTH_16_BITS // Always 16 bit core
  };
}

static void _setLibbluESAc3SpecProperties(
  Ac3Context *ctx,
  LibbluESAc3SpecProperties *ac3_prop
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
  Ac3Context *ctx,
  Ac3ContentType stream_type
)
{
  EsmsHandlerPtr script = ctx->script;

  assert(NULL != script->fmt_prop.ac3);

  _setLibbluESProperties(ctx, &script->prop, stream_type);
  _setLibbluESAc3SpecProperties(ctx, script->fmt_prop.ac3);

  script->PTS_final = ctx->core.pts;
  script->bitrate = _peakBitrateAc3FrameType(stream_type);
}

static const char * _streamFormatStr(
  const Ac3Context *ctx,
  Ac3ContentType stream_type
)
{
  switch (stream_type) {
  case AC3_CORE:
    return "Audio/AC-3";
  case AC3_TRUEHD:
    if (ctx->mlp.info.atmos)
      return "Audio/AC-3 (+ TrueHD/Dolby Atmos Extensions)";
    return "Audio/AC-3 (+ TrueHD Extension)";
  case AC3_EAC3:
    if (ctx->eac3.info.atmos)
      return "Audio/AC-3 (+ E-AC-3/Dolby Atmos Extensions)";
    return "Audio/AC-3 (+ E-AC-3 Extension)";
  }

  return "Audio/Unknown";
}

static void _printStreamInfos(
  const Ac3Context *ctx,
  Ac3ContentType stream_type
)
{
  /* Display infos : */
  lbc_printf(
    lbc_str("== Stream Infos =======================================================================\n")
  );

  lbc_printf(
    lbc_str("Codec: %s, %s (%u channels), Sample rate: %u Hz, Bits per sample: 16bits.\n"),
    _streamFormatStr(ctx, stream_type),
    AudioFormatCodeStr(ctx->script->prop.audio_format),
    ctx->core.bsi.nbChannels,
    valueSampleRateCode(ctx->script->prop.sample_rate)
  );

  if (ctx->contains_mlp) {
    const MlpInformations *mi = &ctx->mlp.info;
    lbc_printf(lbc_str("MLP max output bit-depth: %u bits.\n"), mi->observed_bit_depth);
  }

  printStreamDurationEsmsHandler(ctx->script);
  lbc_printf(
    lbc_str("=======================================================================================\n")
  );
}

int completeAc3Context(
  Ac3Context *ctx
)
{

  Ac3ContentType stream_type;
  if (_detectStreamType(ctx, &stream_type) < 0)
    return -1;
  _setScriptProperties(ctx, stream_type);
  _printStreamInfos(ctx, stream_type);

  if (completePesCuttingScriptEsmsHandler(ctx->script_bs, ctx->script) < 0)
    return -1;
  closeBitstreamWriter(ctx->script_bs);
  ctx->script_bs = NULL;

  if (updateEsmsFile(ctx->script_fp, ctx->script) < 0)
    return -1;

  cleanAc3Context(ctx);
  return 0;
}

void cleanAc3Context(
  Ac3Context *ctx
)
{
  if (NULL == ctx)
    return;
  closeBitstreamReader(ctx->bs);
  closeBitstreamWriter(ctx->script_bs);
  destroyEsmsHandler(ctx->script);
  free(ctx->buffer);
}