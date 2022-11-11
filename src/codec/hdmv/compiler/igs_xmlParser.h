/** \~english
 * \file igs_xmlParser.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief HDMV IGS Compiler internal XML definition format parsing module.
 */

#ifndef __LIBBLU_MUXER__CODECS__IGS_COMPILER__XML_PARSER_H__
#define __LIBBLU_MUXER__CODECS__IGS_COMPILER__XML_PARSER_H__

#include "igs_compiler_context_data.h"
#include "igs_compiler_data.h"
#include "../common/hdmv_pictures_common.h"
#include "../../../util/xmlParsing.h"
#include "../../../ini/iniData.h"

#define XML_CTX(IgsCompilerContextPtr)                                        \
  (                                                                           \
    (IgsCompilerContextPtr)->xmlCtx                                           \
  )

/** \~english
 * \brief Module debug echo header suffix.
 *
 * Will print "[DEBUG/IGSC XML] Msg" on terminal.
 */
#define IGS_COMPILER_XML_DEBUG_CTX_NAME  "/IGSC XML"

/** \~english
 * \brief Module debug echo normal verbose level.
 */
#define IGS_COMPILER_XML_VERBOSE_LEVEL  1

/** \~english
 * \brief Module debug echo lower importance verbose level.
 */
#define IGS_COMPILER_XML_VERBOSE_LEVEL_LOW  3

/** \~english
 * \brief Composition_descriptor()'s composition_state parameter
 * "normal" (0b00) value.
 */
#define IGS_COMPILER_XML_STATE_NORMAL_CASE_STR  "NormalCase"

/** \~english
 * \brief Composition_descriptor()'s composition_state parameter
 * "acquisition_point" (0b01) value.
 */
#define IGS_COMPILER_XML_STATE_ACQ_PNT_STR  "AcquisitionPoint"

/** \~english
 * \brief Composition_descriptor()'s composition_state parameter
 * "epoch_start" (0b10) value.
 */
#define IGS_COMPILER_XML_STATE_EPOCH_START_STR  "EpochStart"

/** \~english
 * \brief Composition_descriptor()'s composition_state parameter
 * "epoch_continue" (0b11) value.
 */
#define IGS_COMPILER_XML_STATE_EPOCH_CONT_STR  "EpochContinue"

/** \~english
 * \brief Interactive_composition()'s stream_model parameter
 * "out_of_mux" (0b0) value.
 */
#define IGS_COMPILER_XML_STREAM_MODEL_OOM  "OoM"

/** \~english
 * \brief Interactive_composition()'s stream_model parameter
 * "multiplexed" (0b1) value.
 */
#define IGS_COMPILER_XML_STREAM_MODEL_MULTIPLEXED  "Multiplexed"

/** \~english
 * \brief Interactive_composition()'s user_interface_model parameter
 * "pop_up" (0b0) value.
 */
#define IGS_COMPILER_XML_UI_MODEL_POP_UP  "Pop-Up"

/** \~english
 * \brief Interactive_composition()'s user_interface_model parameter
 * "normal" (0b1) value.
 */
#define IGS_COMPILER_XML_UI_MODEL_NORMAL  "Normal"

void echoErrorIgsXmlFile(
  const lbc * format,
  ...
);

/** \~english
 * \brief Print on terminal a debug message.
 *
 * \param format Formatted message.
 * \param ... Format arguments.
 */
void echoDebugIgsXmlFile(
  const lbc * format,
  ...
);

/** \~english
 * \brief Print on terminal a debug message with lower importance.
 *
 * \param format Formatted message.
 * \param ... Format arguments.
 */
void echoDebugLowIgsXmlFile(
  const lbc * format,
  ...
);

/** \~english
 * \brief Create a #IgsCompilerComposition object.
 *
 * \return IgsCompilerCompositionPtr On success, created composition
 * (NULL pointer otherwise).
 *
 * Allocated composition must be freed using
 * #destroyIgsCompilerComposition().
 */
IgsCompilerCompositionPtr createIgsCompilerComposition(
  void
);

/** \~english
 * \brief Relase memory allocated by #createIgsCompilerComposition().
 *
 * \param ctx Composition to free.
 */
void destroyIgsCompilerComposition(
  IgsCompilerCompositionPtr compo
);

#if 0
/** \~english
 * \brief Adds given picture to list (and modify parameters if required).
 *
 * \param list Destination list pointer.
 * \param listAllocationLen Destination list length.
 * \param listUsageLen Destination list number of used palette_entries.
 * \param pic Picture to add.
 * \param id Generated picture object_id return (can be NULL if not used).
 * \return int A zero value on success, otherwise a negative value.
 *
 * If destination list is full, it will be reallocated, and
 * allocation length redefined. in all cases, listUsageLen is
 * incremented.
 */
int addIgsCompilerInputPictureToList(
  IgsCompilerInputPicturePtr ** list,
  unsigned * listAllocationLen,
  unsigned * listUsageLen,
  IgsCompilerInputPicturePtr pic,
  uint16_t * id
);

/** \~english
 * \brief Add given picture as an object in given composition.
 *
 * \param compo Destination composition.
 * \param pic Picture to add.
 * \return uint16_t Generated object_id (or 0xFFFF if an error happen).
 */
uint16_t addObjPicIgsCompilerComposition(
  IgsCompilerCompositionPtr compo,
  IgsCompilerInputPicturePtr pic
);
#endif

/** \~english
 * \brief Parse UO_mask_tables() structure from IGS XML description file.
 *
 * \param ctx Used context.
 * \param path Structure root path.
 * \param maskDest Destination mask.
 * \return int A zero value on success, otherwise a negative value.
 */
int parseParametersUopIgsXmlFile(
  IgsCompilerContextPtr ctx,
  const char * path,
  uint64_t * maskDest
);

/** \~english
 * \brief Parse IGS XML description file parameters section.
 *
 * \param ctx Used context.
 * \return int A zero value on success, otherwise a negative value.
 */
int parseCommonSettingsIgsXmlFile(
  IgsCompilerContextPtr ctx
);

/** \~english
 * \brief Parse video_descriptor() structure from IGS XML description file.
 *
 * \param ctx Used context.
 * \return int A zero value on success, otherwise a negative value.
 */
int parseVideoDescriptorIgsXmlFile(
  IgsCompilerContextPtr ctx,
  HdmvVDParameters * param
);

/** \~english
 * \brief Parse composition_descriptor() structure
 * from IGS XML description file.
 *
 * \param ctx Used context.
 * \return int A zero value on success, otherwise a negative value.
 */
int parseCompositionDescriptorIgsXmlFile(
  IgsCompilerContextPtr ctx,
  HdmvCDParameters * param
);

/** \~english
 * \brief Parse interactive_composition() structure parameters
 * from IGS XML description file.
 *
 * \param ctx Used context.
 * \param param Return parameters pointer.
 * \return int A zero value on success, otherwise a negative value.
 *
 * Use relative XPath format, root must be defined before call.
 */
int parseCompositionParametersIgsXmlFile(
  IgsCompilerContextPtr ctx,
  HdmvICParameters * param
);

/** \~english
 * \brief Parse and return a picture from IGS XML description file.
 *
 * \param ctx Used context.
 * \param imgFilename Optionnal opened picture filename return (can be NULL).
 * \return HdmvPicturePtr Picture pointer on success
 * (NULL pointer otherwise).
 *
 * Use relative XPath format, root must be defined before call.
 */
HdmvPicturePtr parseImgIgsXmlFile(
  IgsCompilerContextPtr ctx,
  lbc ** imgFilename
);

/** \~english
 * \brief Parse and return a reference picture from IGS XML description file.
 *
 * \param ctx Used context.
 *
 * Fetch a previously parsed reference picture
 * (by #parseReferencePicturesIndexerIgsXmlFile()).
 * Use relative XPath format, root must be defined before call.
 */
HdmvPicturePtr parseRefImgIgsXmlFile(
  IgsCompilerContextPtr ctx
);

/** \~english
 * \brief Parse reference pictures from IGS XML description file.
 *
 * \param ctx Used context.
 * \return int A zero value on success, otherwise a negative value.
 */
int parseReferencePicturesIndexerIgsXmlFile(
  IgsCompilerContextPtr ctx
);

/** \~english
 * \brief Parse a button_overlap_group() structure
 * from IGS XML description file.
 *
 * \param ctx Used context.
 * \param bog Return parameters pointer.
 * \param index Bog index to parse.
 * \param nextButtonId Next available button_id value.
 * \return int A zero value on success, otherwise a negative value.
 */
int parsePageBogIgsXmlFile(
  IgsCompilerContextPtr ctx,
  HdmvButtonOverlapGroupParameters * bog,
  int index,
  uint16_t * nextButtonId
);

/** \~english
 * \brief Parse a page() structure from IGS XML description file.
 *
 * \param ctx Used context.
 * \param page Return parameters pointer.
 * \param index Page index to parse.
 * \param nextPageId Next available page_id value.
 * \return int A zero value on success, otherwise a negative value.
 */
int parsePageIgsXmlFile(
  IgsCompilerContextPtr ctx,
  HdmvPageParameters * page,
  int index,
  uint8_t * nextPageId
);

/** \~english
 * \brief Parse composition pages from IGS XML description file.
 *
 * \param ctx Used context.
 * \param compo Return composition pointer.
 * \return int A zero value on success, otherwise a negative value.
 */
int parsePagesIgsXmlFile(
  IgsCompilerContextPtr ctx,
  HdmvICParameters * compo
);

/** \~english
 * \brief Parse a interactive composition from IGS XML description file.
 *
 * \param ctx Used context.
 * \param compo Return composition pointer.
 * \return int A zero value on success, otherwise a negative value.
 */
int parseCompositionIgsXmlFile(
  IgsCompilerContextPtr ctx,
  IgsCompilerCompositionPtr compo
);

/** \~english
 * \brief Parse all interactive composition from IGS XML description file.
 *
 * \param ctx Used context.
 * \return int A zero value on success, otherwise a negative value.
 */
int parseCompositionsIgsXmlFile(
  IgsCompilerContextPtr ctx
);

/** \~english
 * \brief Parse context opened IGS XML description file.
 *
 * \param ctx Used Context (containing file, and used to store results).
 * \return int A zero value on success, otherwise a negative value.
 */
int parseIgsXmlFile(
  IgsCompilerContextPtr ctx
);

#endif