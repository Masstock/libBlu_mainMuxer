#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "mlp_check.h"

#define MLP_6CH_CHASSIGN_STR_BUFFSIZE  48

static void _buildMlp6ChPresChAssignStr(
  char dst[static MLP_6CH_CHASSIGN_STR_BUFFSIZE],
  uint8_t u6ch_presentation_channel_assignment
)
{
  static const char * channels[] = {
    "L, R",
    "C",
    "LFE",
    "Ls, Rs",
    "Tfl, Tfr"
  };

  char * sep = "";
  for (unsigned i = 0; i < 5; i++) {
    if (u6ch_presentation_channel_assignment & (1 << i)) {
      lb_str_cat(&dst, sep);
      lb_str_cat(&dst, channels[i]);
      sep = ", ";
    }
  }
}

#define MLP_8CH_CHASSIGN_STR_BUFFSIZE  130

static void _buildMlp8ChPresChAssignStr(
  char dst[static MLP_8CH_CHASSIGN_STR_BUFFSIZE],
  uint8_t u8ch_presentation_channel_assignment,
  bool alternateSyntax
)
{
  static const char * channels[] = {
    "L, R",
    "C",
    "LFE",
    "Ls, Rs",
    "Tfl, Tfr",
    "Lsc, Rsc",
    "Lb, Rb",
    "Cb",
    "Tc",
    "Lsd, Rsd",
    "Lw, Rw",
    "Tfc",
    "LFE2"
  };

  char * sep = "";
  for (unsigned i = 0; i < ((alternateSyntax) ? 4 : 13); i++) {
    if (u8ch_presentation_channel_assignment & (1 << i)) {
      lb_str_cat(&dst, sep);
      lb_str_cat(&dst, channels[i]);
      sep = ", ";
    }
  }
  if (alternateSyntax && (u8ch_presentation_channel_assignment & (1 << 4))) {
    lb_str_cat(&dst, sep);
    lb_str_cat(&dst, "Tsl, Tsr");
  }
}

static int _checkMlpMajorSyncFormatInfo(
  const MlpMajorSyncFormatInfo * fi,
  const MlpMajorSyncFlags * flags,
  MlpInformations * info
)
{

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    Sampling frequency (audio_sampling_frequency): 0x%" PRIX8 ".\n",
    fi->audio_sampling_frequency
  );
  unsigned sampling_frequency = sampleRateMlpAudioSamplingFrequency(
    fi->audio_sampling_frequency
  );
  info->sampling_frequency = sampling_frequency;
  LIBBLU_MLP_DEBUG_PARSING_MS(
    "     -> %u Hz.\n",
    sampling_frequency
  );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    6-ch multichannel type (6ch_multichannel_type): 0b%x.\n",
    fi->u6ch_multichannel_type
  );
  LIBBLU_MLP_DEBUG_PARSING_MS(
    "     -> %s.\n",
    Mlp6ChMultichannelTypeStr(fi->u6ch_multichannel_type)
  );
  if (fi->u6ch_multichannel_type == MLP_6CH_MULTICHANNEL_TYPE_RES) {
    LIBBLU_MLP_ERROR_RETURN(
      "Reserved value in use (6ch_multichannel_type == 0x%x).",
      fi->u6ch_multichannel_type
    );
  }

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    8-ch multichannel type (8ch_multichannel_type): 0b%x.\n",
    fi->u8ch_multichannel_type
  );
  LIBBLU_MLP_DEBUG_PARSING_MS(
    "     -> %s.\n",
    Mlp8ChMultichannelTypeStr(fi->u8ch_multichannel_type)
  );
  if (fi->u8ch_multichannel_type == MLP_8CH_MULTICHANNEL_TYPE_RES) {
    LIBBLU_MLP_ERROR_RETURN(
      "Reserved value in use (8ch_multichannel_type == 0x%x).",
      fi->u8ch_multichannel_type
    );
  }

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    2-ch presentation channels assignment/content type "
    "(2ch_presentation_channel_modifier): 0x%x.\n",
    fi->u2ch_presentation_channel_modifier
  );
  LIBBLU_MLP_DEBUG_PARSING_MS(
    "     -> %s.\n",
    Mlp2ChPresentationChannelModifierStr(
      fi->u2ch_presentation_channel_modifier
    )
  );

  Mlp2ChPresentationChannelModifier u2ch_pres_ch_mod =
    fi->u2ch_presentation_channel_modifier;
  if (MLP_2CH_PRES_CH_MOD_LT_RT == u2ch_pres_ch_mod)
    info->matrix_surround = MLP_INFO_MS_ENCODED;
  if (MLP_2CH_PRES_CH_MOD_LBIN_RBIN == u2ch_pres_ch_mod)
    info->binaural_audio = true;
  if (MLP_2CH_PRES_CH_MOD_MONO == u2ch_pres_ch_mod)
    info->mono_audio = true;

  unsigned u6chNbCh = getNbChannels6ChPresentationAssignment(
    fi->u6ch_presentation_channel_assignment
  );
  bool b6chLsRsPres = lsRsPresent6ChPresentationAssignment(
    fi->u6ch_presentation_channel_assignment
  );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    6-ch presentation channels assignment/content type "
    "(6ch_presentation_channel_modifier): 0x%x.\n",
    fi->u6ch_presentation_channel_modifier
  );
  LIBBLU_MLP_DEBUG_PARSING_MS(
    "     -> %s.\n",
    Mlp6ChPresentationChannelModifierStr(
      fi->u6ch_presentation_channel_modifier,
      u6chNbCh,
      b6chLsRsPres
    )
  );

  if (
    2 != u6chNbCh
    && !b6chLsRsPres
    && 0 < fi->u6ch_presentation_channel_modifier
  ) {
    LIBBLU_MLP_WARNING(
      "Unexpected '6ch_presentation_channel_modifier' == 0x%" PRIX8 ", "
      "for a %u channels presentation without surround channels.\n",
      u6chNbCh
    );
  }

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    6-ch presentation channels assignment "
    "(6ch_presentation_channel_assignment): 0x%x.\n",
    fi->u6ch_presentation_channel_assignment
  );

  char s6ch_channels[MLP_6CH_CHASSIGN_STR_BUFFSIZE] = {'\0'};
  _buildMlp6ChPresChAssignStr(
    s6ch_channels,
    fi->u6ch_presentation_channel_assignment
  );
  LIBBLU_MLP_DEBUG_PARSING_MS(
    "     -> Channels: %s (%u channels).\n",
    s6ch_channels,
    u6chNbCh
  );

  if (0 == u6chNbCh || 6 < u6chNbCh) {
    LIBBLU_MLP_ERROR_RETURN(
      "Invalid number of channels for 6ch presentation (%u).\n",
      u6chNbCh
    );
  }
  info->nb_channels = u6chNbCh;

  unsigned u8chNbCh = getNbChannels8ChPresentationAssignment(
    fi->u8ch_presentation_channel_assignment,
    flags->fmt_info_alter_8ch_asgmt_syntax
  );
  bool b8chMainLRPres = mainLRPresent8ChPresentationAssignment(
    fi->u8ch_presentation_channel_assignment
  );
  bool b8chOnlyLsRsPres = onlyLsRsPresent8ChPresentationAssignment(
    fi->u8ch_presentation_channel_assignment
  );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    8-ch presentation channels assignment/content type "
    "(8ch_presentation_channel_modifier): 0x%x.\n",
    fi->u8ch_presentation_channel_modifier
  );
  LIBBLU_MLP_DEBUG_PARSING_MS(
    "     -> %s.\n",
    Mlp8ChPresentationChannelModifierStr(
      fi->u8ch_presentation_channel_modifier,
      u8chNbCh,
      b8chMainLRPres,
      b8chOnlyLsRsPres
    )
  );

  if (
    !((2 == u8chNbCh && b8chMainLRPres) || b8chOnlyLsRsPres)
    && 0 < fi->u6ch_presentation_channel_modifier
  ) {
    LIBBLU_MLP_WARNING(
      "Unexpected '8ch_presentation_channel_modifier' == 0x%" PRIX8 ", "
      "for a %u channels presentation without surround channels nor "
      "Main L/R channels.\n",
      u8chNbCh
    );
  }

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    8-ch presentation channels assignment "
    "(8ch_presentation_channel_assignment): 0x%x.\n",
    fi->u8ch_presentation_channel_assignment
  );

  char s8ch_channels[MLP_8CH_CHASSIGN_STR_BUFFSIZE] = {'\0'};
  _buildMlp8ChPresChAssignStr(
    s8ch_channels,
    fi->u8ch_presentation_channel_assignment,
    flags->fmt_info_alter_8ch_asgmt_syntax
  );
  LIBBLU_MLP_DEBUG_PARSING_MS(
    "     -> Channels: %s (%u channels).\n",
    s8ch_channels,
    u8chNbCh
  );

  if (8 < u8chNbCh) {
    LIBBLU_MLP_ERROR_RETURN(
      "Invalid number of channels for 8-ch presentation (%u).\n",
      u8chNbCh
    );
  }

  if (0 < u8chNbCh)
    info->nb_channels = u8chNbCh;
  else
    LIBBLU_MLP_WARNING(
      "Missing 8-ch presentation channels assignment mask.\n"
    );

  return 0;
}

static int _checkMlpSubstreamInfo(
  const MlpSubstreamInfo * si
)
{
  LIBBLU_MLP_DEBUG_PARSING_MS(
    "   Substreams carriage informations (substream_info): 0x%02" PRIX8 ".\n",
    si->value
  );

  if (0x0 != si->reserved_field)
    LIBBLU_MLP_DEBUG_PARSING_MS(
      "    WARNING Unexpected non-zero reserved field (reserved): 0x%x.\n",
      si->reserved_field
    );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    2-ch presentation carriage (*inferred*): Substream 0.\n"
  );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    6-ch presentation carriage (6ch_presentation): %s (0x%x).\n",
    MlpSubstreamInfo6ChPresentationStr(si->u6ch_presentation),
    si->u6ch_presentation
  );

  if (MLP_SS_INFO_6CH_PRES_RESERVED == si->u6ch_presentation)
    LIBBLU_MLP_ERROR_RETURN(
      "Reserved value in use, 'substream_info' bits 2-3 == 0x%x.\n",
      si->u6ch_presentation
    );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    8-ch presentation carriage (8ch_presentation): %s (0x%x).\n",
    MlpSubstreamInfo8ChPresentationStr(si->u8ch_presentation),
    si->u8ch_presentation
  );

  if (MLP_SS_INFO_8CH_PRES_RESERVED == si->u8ch_presentation)
    LIBBLU_MLP_ERROR_RETURN(
      "Reserved value in use, 'substream_info' bits 6-4 == 0x%x.\n",
      si->u8ch_presentation
    );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    16-ch presentation presence (16ch_presentation_present): %s (0x%x).\n",
    BOOL_STR(si->b16ch_presentation_present),
    si->b16ch_presentation_present
  );

  return 0;
}

static int _checkMlpSubstreamInfoCombination(
  const MlpSubstreamInfo * si,
  MlpExtendedSubstreamInfo esi
)
{
  static const unsigned valid_ranges[][4] = {
  // A  B  C  D      8ch_pres min A, max B / 6ch_pres min C, max D
    {4, 7, 1, 3}, // 16-ch pres in Substream 3
    {4, 4, 2, 3}, // 16-ch pres in Substreams 2 and 3
    {6, 6, 2, 2}, // 16-ch pres in Substreams 1, 2 and 3
    {7, 7, 3, 3}  // 16-ch pres in Substreams 0, 1, 2 and 3
  };

  int ret = 0;

  unsigned u8ch_p = si->u8ch_presentation;
  if (u8ch_p < valid_ranges[esi][0] || valid_ranges[esi][1] < u8ch_p) {
    LIBBLU_MLP_ERROR(
      "Invalid substream carriage information combination "
      "for 8-ch presentation.\n"
    );
    ret = -1;
  }

  unsigned u6ch_p = si->u6ch_presentation;
  if (u6ch_p < valid_ranges[esi][2] || valid_ranges[esi][3] < u6ch_p) {
    LIBBLU_MLP_ERROR(
      "Invalid substream carriage information combination "
      "for 6-ch presentation.\n"
    );
    ret = -1;
  }

  return ret;
}

static int _checkMlp16ChChannelMeaning(
  const Mlp16ChChannelMeaning * cm
)
{

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "      16-ch dialogue norm (16ch_dialogue_norm): "
    "-%" PRIu8 " LKFS (0x%02" PRIX8 ").\n",
    (0 < cm->u16ch_dialogue_norm) ? cm->u16ch_dialogue_norm : 31,
    cm->u16ch_dialogue_norm
  );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "      16-ch mix level (16ch_mix_level): %u dB (0x%02" PRIX8 ").\n",
    (unsigned) cm->u16ch_mix_level + 70,
    cm->u16ch_mix_level
  );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "      16-ch presentation number of channels (16ch_channel_count): "
    "%u channel(s) (0x%02" PRIX8 ").\n",
    cm->u16ch_channel_count + 1,
    cm->u16ch_channel_count
  );

  if (16 < cm->u16ch_channel_count + 1) {
    LIBBLU_MLP_ERROR_RETURN(
      "Number of channels for the 16-ch presentation exceed 16 channels (%u).",
      cm->u16ch_channel_count + 1
    );
  }

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "      Dynamic objects only 16-ch presentation (dyn_object_only): %s (0b%x).\n",
    BOOL_STR(cm->dyn_object_only),
    cm->dyn_object_only
  );

  if (cm->dyn_object_only) {
    LIBBLU_MLP_DEBUG_PARSING_MS(
      "      First channel contains LFE (lfe_present): %s (0b%x).\n",
      BOOL_STR(cm->lfe_present),
      cm->lfe_present
    );
  }
  else {
    LIBBLU_TODO(); // TODO
  }

  return 0;
}

static int _checkMlpExtraChannelMeaningData(
  const MlpExtraChannelMeaningData * ecmd
)
{

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "     Extra channel meaning data, extra_channel_meaning_data()\n"
  );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "      -> Identified type: %s.\n",
    MlpExtraChannelMeaningContentStr(ecmd->type)
  );

  switch (ecmd->type) {
    case MLP_EXTRA_CH_MEANING_CONTENT_UNKNOWN:
      break;
    case MLP_EXTRA_CH_MEANING_CONTENT_16CH_MEANING:
      if (_checkMlp16ChChannelMeaning(&ecmd->content.v16ch_channel_meaning) < 0)
        return -1;
  }

  char reserved_field[32*5] = {0};
  for (unsigned i = 0; i < ecmd->reserved_size; i += 8) {
    sprintf(&reserved_field[i*5], " 0x%02X", ecmd->reserved[i >> 3]);
  }

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "      Reserved field (reserved):%s.\n",
    reserved_field
  );

  return 0;
}

static int _checkMlpChannelMeaning(
  const MlpChannelMeaning * cm
)
{

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "   Presentation channels metadata, channel_meaning()\n"
  );

  if (0x0 != cm->reserved_field) {
    LIBBLU_MLP_DEBUG_PARSING_MS(
      "    WARNING Unexpected non-zero reserved field (reserved): "
      "0x%02" PRIX8 ".\n",
      cm->reserved_field
    );
  }

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    Enabled 2-ch control gain (2ch_control_enabled): %s (0b%x).\n",
    BOOL_STR(cm->b2ch_control_enabled),
    cm->b2ch_control_enabled
  );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    Enabled 6-ch control gain (6ch_control_enabled): %s (0b%x).\n",
    BOOL_STR(cm->b6ch_control_enabled),
    cm->b6ch_control_enabled
  );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    Enabled 8-ch control gain (8ch_control_enabled): %s (0b%x).\n",
    BOOL_STR(cm->b8ch_control_enabled),
    cm->b8ch_control_enabled
  );

  if (cm->reserved_bool_1) {
    LIBBLU_MLP_DEBUG_PARSING_MS(
      "    WARNING Unexpected non-zero reserved field (reserved): 0b%x.\n",
      cm->reserved_bool_1
    );
  }

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    DRC startup gain (drc_start_up_gain): %d (0x%02" PRIX8 ").\n",
    lb_sign_extend(cm->drc_start_up_gain, 7),
    cm->drc_start_up_gain
  );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    2-ch dialogue norm (2ch_dialogue_norm): "
    "-%" PRIu8 " LKFS (0x%02" PRIX8 ").\n",
    (0 < cm->u2ch_dialogue_norm) ? cm->u2ch_dialogue_norm : 31,
    cm->u2ch_dialogue_norm
  );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    2-ch mix level (2ch_mix_level): %u dB (0x%02" PRIX8 ").\n",
    (unsigned) cm->u2ch_mix_level + 70,
    cm->u2ch_mix_level
  );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    6-ch dialogue norm (6ch_dialogue_norm): "
    "-%" PRIu8 " LKFS (0x%02" PRIX8 ").\n",
    (0 < cm->u6ch_dialogue_norm) ? cm->u6ch_dialogue_norm : 31,
    cm->u6ch_dialogue_norm
  );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    6-ch mix level (6ch_mix_level): %u dB (0x%02" PRIX8 ").\n",
    (unsigned) cm->u6ch_mix_level + 70,
    cm->u6ch_mix_level
  );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    6-ch hierarchical source information (6ch_source_format): "
    "%u (0x%02" PRIX8 ").\n",
    cm->u6ch_source_format,
    cm->u6ch_source_format
  );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    8-ch dialogue norm (8ch_dialogue_norm): "
    "-%" PRIu8 " LKFS (0x%02" PRIX8 ").\n",
    (0 < cm->u8ch_dialogue_norm) ? cm->u8ch_dialogue_norm : 31,
    cm->u8ch_dialogue_norm
  );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    8-ch mix level (8ch_mix_level): %u dB (0x%02" PRIX8 ").\n",
    (unsigned) cm->u8ch_mix_level + 70,
    cm->u8ch_mix_level
  );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    8-ch hierarchical source information (8ch_source_format): "
    "%u (0x%02" PRIX8 ").\n",
    cm->u8ch_source_format,
    cm->u8ch_source_format
  );

  if (cm->reserved_bool_2) {
    LIBBLU_MLP_DEBUG_PARSING_MS(
      "    WARNING Unexpected non-zero reserved field (reserved): 0b%x.\n",
      cm->reserved_bool_2
    );
  }

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    Extra channel meaning (extra_channel_meaning_present): %s (0b%x).\n",
    BOOL_PRESENCE(cm->extra_channel_meaning_present),
    cm->extra_channel_meaning_present
  );

  if (cm->extra_channel_meaning_present) {
    LIBBLU_MLP_DEBUG_PARSING_MS(
      "     Extra channel meaning data length (extra_channel_meaning_length): "
      "%u words (0x%" PRIX8 ").\n",
      cm->extra_channel_meaning_length + 1,
      cm->extra_channel_meaning_length
    );

    if (_checkMlpExtraChannelMeaningData(&cm->extra_channel_meaning_data) < 0)
      return -1;
  }

  return 0;
}

static int _checkMlpMajorSyncInfo(
  const MlpMajorSyncInfoParameters * msi,
  MlpInformations * info
)
{

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "   Format synchronization word (format_sync): 0x%08" PRIX32 ".\n",
    msi->format_sync
  );
  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    -> %s.\n",
    MlpFormatSyncStr(msi->format_sync)
  );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "   Bitstream format informations (format_info): 0x%08" PRIX32 ".\n",
    msi->format_info.value
  );

  int ret = _checkMlpMajorSyncFormatInfo(
    &msi->format_info,
    &msi->flags,
    info
  );
  if (ret < 0)
    return -1;

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "   Format signature (signature): 0x%04" PRIX16 ".\n",
    msi->signature
  );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "   Informations flags (flags): 0x%04" PRIX16 ".\n",
    msi->flags.value
  );
  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    Constant FIFO delay: %s.\n",
    BOOL_STR(msi->flags.constant_fifo_buf_delay)
  );
  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    8ch presentation channel assignment modifier: %s.\n",
    BOOL_STR(msi->flags.fmt_info_alter_8ch_asgmt_syntax)
  );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "   Variable bitrate mode (variable_bitrate): %s (0b%x).\n",
    BOOL_STR(msi->variable_bitrate),
    msi->variable_bitrate
  );

  if (msi->variable_bitrate) {
    LIBBLU_MLP_DEBUG_PARSING_MS(
      "   Peak data-rate (peak_data_rate): 0x%04" PRIX16 ".\n",
      msi->peak_data_rate
    );
  }
  else {
    LIBBLU_MLP_DEBUG_PARSING_MS(
      "   Constant data-rate (peak_data_rate): 0x%04" PRIX16 ".\n",
      msi->peak_data_rate
    );
  }

  info->peak_data_rate = (msi->peak_data_rate * info->sampling_frequency) >> 4;

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "    -> %u bps, %u kbps.\n",
    info->peak_data_rate,
    info->peak_data_rate / 1000
  );

  if (MLP_MAX_PEAK_DATA_RATE < info->peak_data_rate)
    LIBBLU_MLP_ERROR_RETURN(
      "Too high peak data rate %u bps (shall not exceed %u bps).\n",
      info->peak_data_rate,
      MLP_MAX_PEAK_DATA_RATE
    );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "   Number of substreams (substreams): %u.\n",
    msi->substreams
  );

  LIBBLU_MLP_DEBUG_PARSING_MS(
    "   16-ch presentation carriage (extended_substream_info): %s (0x%x).\n",
    MlpExtendedSubstreamInfoStr(
      msi->extended_substream_info,
      msi->substream_info.b16ch_presentation_present
    ),
    msi->extended_substream_info
  );

  const MlpExtendedSubstreamInfo esi = msi->extended_substream_info;

  if (msi->substream_info.b16ch_presentation_present && 0 != esi) {
    LIBBLU_MLP_WARNING(
      "Unexpected field in use, 'extended_substream_info' == 0x%x.\n",
      esi
    );
  }

  if (_checkMlpSubstreamInfo(&msi->substream_info) < 0)
    return -1;

  if (3 < msi->substreams) {
    if (_checkMlpSubstreamInfoCombination(&msi->substream_info, esi) < 0)
      LIBBLU_ERROR_RETURN(
        "Invalid 'substream_info'/'extended_substream_info' fields.\n"
      );

    if (msi->substream_info.b16ch_presentation_present)
      info->atmos = true;
  }

  if (_checkMlpChannelMeaning(&msi->channel_meaning) < 0)
    return -1;

  return 0;
}


int checkMlpSyncHeader(
  const MlpSyncHeaderParameters * sync,
  MlpInformations * info,
  bool firstAU
)
{

  LIBBLU_MLP_DEBUG_PARSING_HDR(
    "  Minor Sync check sum (check_nibble): 0x%" PRIX8 ".\n",
    sync->check_nibble
  );

  LIBBLU_MLP_DEBUG_PARSING_HDR(
    "  Access Unit length (access_unit_length): %u words (0x%03" PRIX16 ").\n",
    sync->access_unit_length,
    sync->access_unit_length
  );
  LIBBLU_MLP_DEBUG_PARSING_HDR(
    "   -> %u bytes.\n",
    2 * sync->access_unit_length
  );

  LIBBLU_MLP_DEBUG_PARSING_HDR(
    "  Decoding timestamp (input_timing): %u samples (0x%02" PRIX16 ").\n",
    sync->input_timing,
    sync->input_timing
  );

  if (firstAU && !sync->major_sync) {
    LIBBLU_MLP_ERROR_RETURN(
      "Invalid first Access Unit, missing Major Sync.\n"
    );
  }

  if (sync->major_sync) {
    LIBBLU_MLP_DEBUG_PARSING_HDR(
      "  Major Sync present.\n"
    );

    if (_checkMlpMajorSyncInfo(&sync->major_sync_info, info) < 0)
      return -1;
  }

  return 0;
}

/** \~english
 * \brief Check and compute substreams sizes from substream_directory().
 *
 * \param directory substream_directory() syntax object.
 * \param au_ss_length Access unit remaining size in 16-bits words.
 * \param is_major_sync Is the access unit a major sync.
 * \return int Upon success, a zero value is returned. Otherwise, a negative
 * value is returned.
 */
int checkAndComputeSSSizesMlpSubstreamDirectory(
  MlpSubstreamDirectoryEntry * directory,
  const MlpMajorSyncInfoParameters * msi,
  unsigned au_ss_length,
  bool is_major_sync
)
{

  unsigned substream_start = 0;
  for (unsigned i = 0; i < msi->substreams; i++) {
    MlpSubstreamDirectoryEntry * entry = &directory[i];

    LIBBLU_MLP_DEBUG_PARSING_HDR("   Substream %u entry:\n", i);
    LIBBLU_MLP_DEBUG_PARSING_HDR(
      "    Extra 16-bit word (extra_substream_word): %s (0b%x).\n",
      BOOL_PRESENCE(entry->extra_substream_word),
      entry->extra_substream_word
    );
    LIBBLU_MLP_DEBUG_PARSING_HDR(
      "    Restart header (restart_nonexistent): %s (0b%x).\n",
      BOOL_PRESENCE(!entry->restart_nonexistent),
      entry->restart_nonexistent
    );
    LIBBLU_MLP_DEBUG_PARSING_HDR(
      "    CRC and parity check (crc_present): %s (0b%x).\n",
      BOOL_PRESENCE(entry->crc_present),
      entry->crc_present
    );
    if (entry->reserved_field_1) {
      LIBBLU_MLP_DEBUG_PARSING_HDR(
        "    WARNING Unexpected non-zero reserved field (reserved): 0b1.\n"
      );
    }
    LIBBLU_MLP_DEBUG_PARSING_HDR(
      "    Substream end pointer offset (substream_end_ptr): "
      "%u (0x%03" PRIX16 ").\n",
      entry->substream_end_ptr,
      entry->substream_end_ptr
    );

    if (entry->extra_substream_word) {
      LIBBLU_MLP_DEBUG_PARSING_HDR(
        "    Dynamic range control, dynamic_range_control\n"
      );
      LIBBLU_MLP_DEBUG_PARSING_HDR(
        "     Dynamic range update (drc_gain_update): %d (0x%03" PRIX16 ").\n",
        lb_sign_extend(entry->drc_gain_update, 9),
        entry->drc_gain_update
      );
      LIBBLU_MLP_DEBUG_PARSING_HDR(
        "     Dynamic time update (drc_time_update): %u (0x%" PRIX8 ").\n",
        entry->drc_time_update,
        entry->drc_time_update
      );
      if (entry->reserved_field_2) {
        LIBBLU_MLP_DEBUG_PARSING_HDR(
          "     WARNING Unexpected non-zero reserved field (reserved): 0b1.\n"
        );
      }
    }

    if (!entry->restart_nonexistent ^ is_major_sync)
      LIBBLU_MLP_ERROR_RETURN(
        "Substream %u 'restart_nonexistent' shall be set to 0b%x since "
        "the access unit %s with a major sync.",
        i, !entry->restart_nonexistent,
        (is_major_sync) ? "starts" : "does not start"
      );

    if (au_ss_length < entry->substream_end_ptr)
      LIBBLU_MLP_ERROR_RETURN(
        "Substream %u 'substream_end_ptr' out of Access Unit (%u < %u).\n",
        i, au_ss_length, entry->substream_end_ptr
      );

    if (entry->substream_end_ptr <= substream_start)
      LIBBLU_MLP_ERROR_RETURN(
        "Substream %u 'substream_end_ptr' lower than previous one (%u < %u).\n",
        i, entry->substream_end_ptr, substream_start
      );

    entry->substream_size = entry->substream_end_ptr - substream_start;
    substream_start = entry->substream_end_ptr;
  }

  return 0;
}