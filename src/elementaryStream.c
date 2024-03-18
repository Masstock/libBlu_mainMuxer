#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include "elementaryStream.h"

/* ### ES settings definition : ############################################ */

int setFpsChangeLibbluESSettings(
  LibbluESSettings *dst,
  const lbc *fps_string
)
{
  HdmvFrameRateCode idc;

  if (parseFpsChange(fps_string, &idc) < 0)
    return -1;

  LIBBLU_ES_SETTINGS_SET_OPTION(dst, fps_mod, idc);
  return 0;
}

int setArChangeLibbluESSettings(
  LibbluESSettings *dst,
  const lbc *ar_string
)
{
  LibbluAspectRatioMod values;

  if (parseArChange(ar_string, &values) < 0)
    return -1;

  LIBBLU_ES_SETTINGS_SET_OPTION(dst, ar_mod, values);
  return 0;
}

int setLevelChangeLibbluESSettings(
  LibbluESSettings *dst,
  const lbc *level_string
)
{
  uint8_t idc;

  if (parseLevelChange(level_string, &idc) < 0)
    return -1;

  LIBBLU_ES_SETTINGS_SET_OPTION(dst, level_mod, idc);
  return 0;
}

int setHdmvInitialTimestampLibbluESSettings(
  LibbluESSettings *dst,
  uint64_t value
)
{
#if 0 < LIBBLU_MIN_HDMV_INIT_TIMESTAMP
  if (value < LIBBLU_MIN_HDMV_INIT_TIMESTAMP)
    return -1;
#endif
  if (LIBBLU_MAX_HDMV_INIT_TIMESTAMP < value)
    return -1;

  LIBBLU_ES_SETTINGS_SET_OPTION(dst, hdmv.initial_timestamp, value);
  return 0;
}

int setMainESFilepathLibbluESSettings(
  LibbluESSettings *dst,
  const lbc *filepath,
  const lbc *anchor_fp
)
{
  assert(NULL != dst);
  assert(NULL != filepath);

  return lb_gen_anchor_absolute_fp(
    &dst->es_filepath,
    anchor_fp,
    filepath
  );
}

int setScriptFilepathLibbluESSettings(
  LibbluESSettings *dst,
  const lbc *filepath,
  const lbc *anchor_fp
)
{
  assert(NULL != dst);
  assert(NULL != filepath);

  return lb_gen_anchor_absolute_fp(
    &dst->es_script_filepath,
    anchor_fp,
    filepath
  );
}

static int _checkScriptFile(
  LibbluES *checked_es,
  LibbluESFormatUtilities *es_associated_utils_ret,
  bool force_script_build
)
{
  const LibbluESSettings *es_sets = checked_es->settings_ref;
  const lbc *script_filepath      = checked_es->script_filepath;

  assert(NULL != es_sets);

  /* Checking ESMS script file : */
  LIBBLU_SCRIPT_DEBUG(
    "Check script filepath '%" PRI_LBCS "'.\n",
    script_filepath
  );
  uint64_t exp_flags = computeFlagsLibbluESSettingsOptions(es_sets->options);

  ESMSFileValidatorRet ret;
  if (
    force_script_build
    || (0 > (ret = isAValidESMSFile(script_filepath, exp_flags, NULL)))
  ) {
    /* Not valid/missing/forced rebuilding */
    if (force_script_build)
      LIBBLU_SCRIPT_DEBUG("Forced rebuilding enabled, building script.\n");
    else
      LIBBLU_SCRIPT_DEBUG(
        "Invalid script, building a new one (%s).\n",
        ESMSValidatorErrorStr(ret)
      );

    if (NULL == es_sets->es_filepath)
      LIBBLU_ERROR_RETURN(
        "Unable to generate script '%" PRI_LBCS "', "
        "no source ES defined.\n",
        script_filepath
      );

    LibbluStreamCodingType exp_coding_type;
    if (0 <= es_sets->coding_type)
      exp_coding_type = es_sets->coding_type;
    else {
      LIBBLU_SCRIPT_DEBUG("Undefined stream coding type, use guessing.\n");
      if ((exp_coding_type = guessESStreamCodingType(es_sets->es_filepath)) < 0)
        LIBBLU_ERROR_RETURN(
          "Unable to guess stream coding type of '%" PRI_LBCS "', "
          "file type is not recognized.\n",
          es_sets->es_filepath
        );
    }

    /* Init format utilities with expected stream coding type. */
    LIBBLU_SCRIPT_DEBUG("Defining ES associated utilities.\n");
    LibbluESFormatUtilities utilities;
    if (initLibbluESFormatUtilities(&utilities, exp_coding_type) < 0)
      return -1;
    *es_associated_utils_ret = utilities;

    LIBBLU_SCRIPT_DEBUG("Generate script.\n");
    int ret = generateScriptES(
      utilities,
      es_sets->es_filepath,
      script_filepath,
      es_sets->options
    );
    if (ret < 0)
      LIBBLU_ERROR_RETURN(
        "Invalid input file '%" PRI_LBCS "', "
        "unable to generate script.\n",
        es_sets->es_filepath
      );
  }

  return 0;
}

static int _parseScriptLibbluES(
  LibbluES *es
)
{
  const lbc *script_filepath = es->script_filepath;

  /* Open script file */
  LIBBLU_SCRIPTRO_DEBUG("Opening script.\n");
  BitstreamReaderPtr script_bs = createBitstreamReaderDefBuf(
    script_filepath
  );
  if (NULL == script_bs)
    return -1;

  LIBBLU_SCRIPTRO_DEBUG("Parsing script properties.\n");

  /* ES Properties section : */
  LIBBLU_SCRIPTRO_DEBUG(" Reading ES Properties section.\n");
  if (seekESPropertiesEsms(script_filepath, script_bs) < 0)
    goto free_return;

  uint64_t PTS_reference, PTS_final;
  if (parseESPropertiesHeaderEsms(script_bs, &es->prop, &PTS_reference, &PTS_final) < 0)
    goto free_return;
  es->PTS_reference = PTS_reference;
  es->PTS_final = PTS_final;

  /* ES Properties source files : */
  LIBBLU_SCRIPTRO_DEBUG(" Reading ES Properties Source Files section.\n");
  if (parseESPropertiesSourceFilesEsms(script_bs, &es->script.es_source_files) < 0)
    goto free_return;

  if (isConcernedESFmtPropertiesEsms(es->prop)) {
    /* Reading ES Format Properties section according to stream type. */
    LIBBLU_SCRIPTRO_DEBUG(" Reading ES Format properties section.\n");
    if (seekESFmtPropertiesEsms(script_filepath, script_bs) < 0)
      goto free_return;
    if (parseESFmtPropertiesEsms(script_bs, &es->prop, &es->fmt_prop) < 0)
      goto free_return;
  }

  /* Data blocks definition section : */
  int ret = isPresentESDataBlocksDefinitionEsms(script_filepath);
  if (ret < 0)
    goto free_return;
  if (0 < ret) {
     /* Reading ES Data Blocks Definition section if present. */
     LIBBLU_SCRIPTRO_DEBUG(" Reading ES Data Blocks Definition section.\n");
    if (seekESDataBlocksDefinitionEsms(script_filepath, script_bs) < 0)
      goto free_return;
    if (parseESDataBlocksDefinitionEsms(script_bs, &es->script.data_sections) < 0)
      goto free_return;
  }

  /* Seek to the ES PES Cutting section. */
  LIBBLU_SCRIPTRO_DEBUG(" Seeking ES PES Cutting section.\n");
  if (seekESPesCuttingEsms(script_filepath, script_bs) < 0)
    goto free_return;

  LIBBLU_SCRIPTR_DEBUG(
    "PES frames Cutting script section:\n"
  );

  if (checkDirectoryMagic(script_bs, PES_CUTTING_HEADER_MAGIC, 4) < 0)
    goto free_return;

  LIBBLU_SCRIPTR_DEBUG(
    " PES frames Cutting script section magic (PESC_magic): "
    STR(PES_CUTTING_HEADER_MAGIC) ".\n"
  );

  es->script.bs_handle = script_bs;
  return 0;

free_return:
  LIBBLU_ERROR(
    "Error happen during parsing of script \"%" PRI_LBCS "\".\n",
    script_filepath
  );
  closeBitstreamReader(script_bs);
  return -1;
}

static int setPesPacketsDurationLibbluES(
  LibbluES *es
)
{
  LibbluESProperties *prop = &es->prop;

  prop->nb_pes_per_sec = prop->nb_pes_sec_per_sec = 0;
  prop->br_based_on_duration = false;

  double frame_rate = -1.f, sample_rate = -1.f;

  if (prop->type == ES_VIDEO) {
    if ((frame_rate = frameRateCodeToDouble(prop->frame_rate)) <= 0)
      LIBBLU_ERROR_RETURN(
        "Broken script '%" PRI_LBCS "', unknown frame-rate.\n",
        es->script_filepath
      );
  }
  else if (prop->type == ES_AUDIO) {
    if ((sample_rate = sampleRateCodeToDouble(prop->sample_rate)) <= 0)
      LIBBLU_ERROR_RETURN(
        "Broken script '%" PRI_LBCS "', unknown sample-rate.\n",
        es->script_filepath
      );
  }

  switch (prop->coding_type) {
  case STREAM_CODING_TYPE_MPEG1: /* H.261 / MPEG-1 */
  case STREAM_CODING_TYPE_H262:  /* H.262 / MPEG-2 */
  case STREAM_CODING_TYPE_AVC:   /* H.264 / AVC    */
  case STREAM_CODING_TYPE_VC1:   /* VC-1           */
  case STREAM_CODING_TYPE_HEVC:  /* H.265 / HEVC   */
    assert(0 < frame_rate);
    prop->nb_pes_per_sec = frame_rate;
    break;

  case STREAM_CODING_TYPE_LPCM:  /* LPCM */
    /* Frame duration: 1/200s */
    prop->nb_pes_per_sec = LPCM_PES_FRAMES_PER_SECOND;
    break;

  case STREAM_CODING_TYPE_AC3:   /* AC-3 */
    /* Frame duration: 1536 samples */
    assert(0 < sample_rate);
    prop->nb_pes_per_sec = sample_rate / 1536;
    break;

  case STREAM_CODING_TYPE_DTS:   /* DTS Coherent Acoustics */
    /* Frame duration: 512 samples */
    assert(0 < sample_rate);
    prop->nb_pes_per_sec = sample_rate / 512;
    break;

  case STREAM_CODING_TYPE_HDHR:  /* DTS-HDHR */
  case STREAM_CODING_TYPE_HDMA:  /* DTS-HDMA */
    /* Frame duration: 512 samples (2 consecutive frames) */
    assert(0 < sample_rate);
    prop->nb_pes_per_sec = sample_rate / 512;
    prop->nb_pes_sec_per_sec = sample_rate / 512;
    break;

  case STREAM_CODING_TYPE_TRUEHD: /* Dolby TrueHD (+AC-3 Core) */
    /* Frame duration: 1536 samples & 1/200s */
    assert(0 < sample_rate);
    prop->nb_pes_per_sec = sample_rate / 1536;
    prop->nb_pes_sec_per_sec = 200;
    break;

  case STREAM_CODING_TYPE_EAC3:  /* EAC-3 (+AC-3 Core) */
    /* Frame duration: 1536 samples (2 consecutive frames) */
    assert(0 < sample_rate);
    prop->nb_pes_per_sec = sample_rate / 1536;
    prop->nb_pes_sec_per_sec = sample_rate / 1536;
    break;

  case STREAM_CODING_TYPE_DTSE_SEC: /* DTS-Express (Secondary track) */
    /* Frame duration: 4096 samples */
    assert(0 < sample_rate);
    prop->nb_pes_per_sec = sample_rate / 4096;
    prop->nb_pes_sec_per_sec = sample_rate / 4096;
    break;

  case STREAM_CODING_TYPE_IG: /* Interactive Graphic Stream (IGS, Menus) */
  case STREAM_CODING_TYPE_PG: /* Presentation Graphic Stream (PGS, Subs) */
    prop->br_based_on_duration = true;
    break;

  default:
    LIBBLU_ERROR_RETURN(
      "Missing PES packets timing information specification "
      "for %s stream coding type.\n",
      LibbluStreamCodingTypeStr(prop->coding_type)
    );
  }

  assert(
    0 < prop->nb_pes_per_sec
    || prop->br_based_on_duration
  );

  return 0;
}

int prepareLibbluES(
  LibbluES *es,
  LibbluESFormatUtilities *es_utilities_ret,
  bool force_script_build
)
{

  /* Check and/or generate ES script */
  LibbluESFormatUtilities utilities = (LibbluESFormatUtilities) {0};
  if (_checkScriptFile(es, &utilities, force_script_build) < 0)
    return -1;

  /* Open and parse ES script */
  if (_parseScriptLibbluES(es) < 0)
    return -1;

  /* Open each source file */
  if (openAllEsmsESSourceFiles(&es->script.es_source_files) < 0)
    return -1;

  if (!utilities.initialized) {
    /* No script generation done, use script content */
    /* Init format utilities according to script stream coding type. */
    if (initLibbluESFormatUtilities(&utilities, es->prop.coding_type) < 0)
      return -1;
  }

  LIBBLU_SCRIPT_DEBUG("Set PES packets timing informations.\n");
  if (setPesPacketsDurationLibbluES(es) < 0)
    return -1;

  if (NULL != es_utilities_ret)
    *es_utilities_ret = utilities;

  return 0;
}

static int _addPesPacketToBdavStdLibbluES(
  LibbluES *es,
  LibbluESPesPacketProperties prop,
  uint32_t pesp_header_size,
  uint16_t pid,
  uint64_t ref_STC_timestamp
)
{
  assert(NULL != es->lnkd_tstd_buf_list);

  uint64_t removal_timestamp = prop.dts;
  uint64_t output_timestamp  = prop.pts;
  if (
    es->prop.coding_type == STREAM_CODING_TYPE_AVC
    && prop.extension_data_pres
  ) {
    removal_timestamp = prop.extension_data.h264.cpb_removal_time + ref_STC_timestamp;
    output_timestamp  = prop.extension_data.h264.dpb_output_time  + ref_STC_timestamp;
  }

  LIBBLU_T_STD_VERIF_DECL_DEBUG(
    "Registering %zu+%zu=%zu bytes of PES for PID 0x%04" PRIX16 ", "
    "Remove: %" PRIu64 ", Output: %" PRIu64 ".\n",
    pesp_header_size,
    prop.payload_size,
    pesp_header_size + prop.payload_size,
    pid,
    removal_timestamp,
    output_timestamp
  );

  if (es->prop.type == ES_VIDEO) {
    if (
      addFrameToBufferFromList(
        es->lnkd_tstd_buf_list, MULTIPLEX_BUFFER, NULL,
        (BufModelBufferFrame) {
          .header_size = pesp_header_size * 8,
          .data_size = prop.payload_size * 8,
          .removal_timestamp = removal_timestamp,
          .output_data_size = 0,
          .do_not_remove = false
        }
      ) < 0
    )
      return -1;

    return addFrameToBufferFromList(
      es->lnkd_tstd_buf_list, ELEMENTARY_BUFFER, NULL,
      (BufModelBufferFrame) {
        .header_size = 0,
        .data_size = prop.payload_size * 8,
        .removal_timestamp = output_timestamp,
        .output_data_size = 0,
        .do_not_remove = false
      }
    );
  }

  return addFrameToBufferFromList(
    es->lnkd_tstd_buf_list, MAIN_BUFFER, NULL,
    (BufModelBufferFrame) {
      .header_size = pesp_header_size * 8,
      .data_size = prop.payload_size * 8,
      .removal_timestamp = removal_timestamp,
      .output_data_size = 0,
      .do_not_remove = false
    }
  );
}

static int _writePesHeader(
  LibbluESPesPacketData *dst,
  const PesPacketHeaderParam *prop,
  size_t expected_size
)
{
  uint32_t written_size = writePesHeader(dst->data, 0x0, prop);

  if (expected_size != written_size)
    LIBBLU_ERROR_RETURN(
      "Incorrect ESMS PES header length, expect %zu, set %zu bytes.\n",
      expected_size,
      written_size
    );

  return 0;
}

static int _buildNextPesPacket(
  LibbluES *es,
  uint16_t pid,
  uint64_t ref_STC_timestamp,
  LibbluPesPacketHeaderPrep_fun preparePesHeader
)
{

  /* Buffer new PES packets from script if required */
  size_t nb_buf_pes = nbEntriesCircularBuffer(&es->script.pes_packets_queue);
  if (
    nb_buf_pes < LIBBLU_ES_MIN_BUF_PES_SCRIPT_PACKETS
    && !es->script.end_reached
  ) {
    for (unsigned i = 0; i < LIBBLU_ES_INC_BUF_PES_SCRIPT_PACKETS; i++) {
      if (isEndReachedESPesCuttingEsms(es->script.bs_handle))
        break;

      EsmsPesPacket *esms_pes_packet = newEntryCircularBuffer(
        &es->script.pes_packets_queue,
        sizeof(EsmsPesPacket)
      );
      if (NULL == esms_pes_packet)
        LIBBLU_ERROR_RETURN("Script PES packets queue error.\n");

      int ret = parseFrameNodeESPesCuttingEsms(
        esms_pes_packet,
        es->script.bs_handle,
        es->prop.type,
        es->prop.coding_type,
        &es->script.commands_data_parsing
      );
      if (ret < 0)
        return -1;
    }

    if (isEndReachedESPesCuttingEsms(es->script.bs_handle))
      es->script.end_reached = true;
  }

  /* Try to extract next buffered script PES packet. */
  EsmsPesPacket *esms_pes_packet = popCircularBuffer(
    &es->script.pes_packets_queue
  );
  if (NULL == esms_pes_packet)
    return 0; // No more PES packets.

  /* Build from it next PES packet. */
  LibbluESPesPacketProperties pesp_prop;
  if (prepareLibbluESPesPacketProperties(&pesp_prop, esms_pes_packet, ref_STC_timestamp, es->PTS_reference) < 0)
    return -1;
  uint32_t payload_size = pesp_prop.payload_size;

  es->current_pes_packet.extension_frame = pesp_prop.extension_frame;
  es->current_pes_packet.PTS             = pesp_prop.pts;
  es->current_pes_packet.DTS             = pesp_prop.dts;

  PesPacketHeaderParam *pesp_header = &es->current_pes_packet.header;
  if (preparePesHeader(pesp_header, pesp_prop, es->prop.coding_type) < 0)
    return -1;
  uint32_t header_size = computePesPacketHeaderLen(pesp_header);

  setLengthPesPacketHeaderParam(pesp_header, header_size, payload_size);
  uint32_t pesp_size = header_size + payload_size;

  LibbluESPesPacketData *pesp_data = &es->current_pes_packet.data;
  if (allocateLibbluESPesPacketData(pesp_data, pesp_size) < 0)
    return -1;
  pesp_data->size   = pesp_size;
  pesp_data->offset = 0;

  if (_writePesHeader(pesp_data, pesp_header, header_size) < 0)
    return -1;
  uint8_t *payload_dst = &pesp_data->data[header_size];

  for (unsigned i = 0; i < esms_pes_packet->nb_commands; i++) {
    const EsmsCommand *command = &esms_pes_packet->commands[i];

    int ret = 0;
    switch (command->type) {
    case ESMS_ADD_DATA:
      ret = applyEsmsAddDataCommand(command->data.add_data, payload_dst, payload_size);
      break;

    case ESMS_CHANGE_BYTEORDER:
      ret = applyEsmsChangeByteOrderCommand(command->data.change_byte_order, payload_dst, payload_size);
      break;

    case ESMS_ADD_PAYLOAD_DATA:
      ret = applyEsmsAddPesPayloadCommand(command->data.add_pes_payload, payload_dst, payload_size, &es->script.es_source_files);
      break;

    case ESMS_ADD_PADDING_DATA:
      ret = applyEsmsAddPaddingCommand(command->data.add_padding, payload_dst, payload_size);
      break;

    case ESMS_ADD_DATA_BLOCK:
      ret = applyEsmsAddDataBlockCommand(command->data.add_data_block, payload_dst, payload_size, es->script.data_sections);
    }

    if (ret < 0)
      return -1;
  }

  /* Add to stream buffering model chain if used */
  if (NULL != es->lnkd_tstd_buf_list) {
    if (_addPesPacketToBdavStdLibbluES(es, pesp_prop, header_size, pid, ref_STC_timestamp) < 0)
      return -1;
  }

  cleanEsmsPesPacketPtr(esms_pes_packet);
  return 1; // Packet builded.
}

int buildNextPesPacketLibbluES(
  LibbluES *es,
  uint16_t pid,
  uint64_t ref_STC_timestamp,
  LibbluPesPacketHeaderPrep_fun preparePesHeader
)
{
  return _buildNextPesPacket(es, pid, ref_STC_timestamp, preparePesHeader);
}