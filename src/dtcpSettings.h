/** \~english
 * \file dtcpSettings.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief Digital Transmission Content Protection configuration module.
 *
 *
 * \xrefitem references "References" "References list"
 *  [1] Advanced Access Content System (AACS), Blu-ray Disc Pre-recorded Book,
 *      Revision 0.953 Final (October 26, 2012).\n
 *  [2] DTLA, Digital Transmission Content Protection Specitification,
 *      Volume 1 (Informational Version), Revision 1.71.
 *      https://www.dtcp.com/specifications.aspx
 *      https://www.dtcp.com/documents/dtcp/info-20150204-dtcp-v1-rev%201-71.pdf\n
 *  [3] ATSC A/70 Part 1:2010 & Part 2:2011.\n
 *  [4] DVB-SI, CA System ID list
 *      https://www.dvbservices.com/identifiers/ca_system_id
 *
 */

#ifndef __LIBBLU_MUXER__DTCP_SETTINGS_H__
#define __LIBBLU_MUXER__DTCP_SETTINGS_H__

/** \~english
 * \brief HDMV HDCP (Sony Corporation) registered Conditional Access System
 * Identifier.
 *
 * Type of CA system used by Blu-ray applications. Value used for the
 * 'CA_System_ID' DTCP descriptor field.
 */
#define DTCP_CA_SYSTEM_ID_HDMV  0x0FFF

/** \~english
 * \brief DTCP Retention State value.
 *
 * This value is used to set the allowed retention time for a 'Copy Never'
 * content.
 */
typedef enum {
  LIBBLU_DTCP_RET_STATE_FOREVER     = 0x0,  /**< Forever.                    */
  LIBBLU_DTCP_RET_STATE_1_WEEK      = 0x1,  /**< 1 week.                     */
  LIBBLU_DTCP_RET_STATE_2_DAYS      = 0x2,  /**< 2 days.                     */
  LIBBLU_DTCP_RET_STATE_1_DAY       = 0x3,  /**< 1 day.                      */
  LIBBLU_DTCP_RET_STATE_12_HOURS    = 0x4,  /**< 12 hours.                   */
  LIBBLU_DTCP_RET_STATE_6_HOURS     = 0x5,  /**< 6 hours.                    */
  LIBBLU_DTCP_RET_STATE_3_HOURS     = 0x6,  /**< 3 hours.                    */
  LIBBLU_DTCP_RET_STATE_90_MINUTES  = 0x7   /**< 90 minutes.                 */
} LibbluDtcpRetentionStateValue;

/** \~english
 * \brief DTCP Copy Control Information, copy generation management value.
 *
 * The #LIBBLU_DTCP_CCI_NO_MORE_COPIES shall not be used, since it is the value
 * used for copies of a content with the #LIBBLU_DTCP_CCI_COPY_ONE_GENERATION
 * setting.
 */
typedef enum {
  LIBBLU_DTCP_CCI_COPY_FREE            = 0x0,  /**< Copy Control Not
    Asserted, no copy control.                                               */
  LIBBLU_DTCP_CCI_NO_MORE_COPIES       = 0x1,  /**< No More Copies, reserved
    for copies of Copy One Generation content.                               */
  LIBBLU_DTCP_CCI_COPY_ONE_GENERATION  = 0x2,  /**< Copy One Generation,
    allows a one genetation copy of protected content.                       */
  LIBBLU_DTCP_CCI_COPY_NEVER           = 0x3   /**< Never Copy.              */
} LibbluDtcpCopyControlInformationValue;

/** \~english
 * \brief DTCP Analog Copy Protection information.
 *
 * Macrovision protection settings, set usage of Automatic Gain Control (AGC)
 * and Colourstripe protections for Analog outputs (VCR copy prevention).
 */
typedef enum {
  LIBBLU_DTCP_APS_COPY_FREE  = 0x0,  /**< Copy Control Not Asserted,
  no copy control.                                                           */
  LIBBLU_DTCP_APS_TYPE_1     = 0x1,  /**< AGC only.                          */
  LIBBLU_DTCP_APS_TYPE_2     = 0x2,  /**< AGC + 2L Colourstripe.             */
  LIBBLU_DTCP_APS_TYPE_3     = 0x3   /**< AGC + 4L Colourstripe.             */
} LibbluDtcpAnalogCopyProtectionInformationValue;

/** \~english
 * \brief Digital Transmission Content Protection descriptor settings.
 *
 */
typedef struct {
  uint16_t caSystemId;    /**< Conditional Access system identifier used
    (shall be equal to #DTCP_CA_SYSTEM_ID_HDMV for Blu-ray).                 */

  bool retentionMoveMode;  /**< Retention Move Mode.
    0b0: Activated (see dtcpCci), 0b1: Disabled/Reserved, no retention.

    \note According to AACS specifications, this field is reserved and shall
    be set to 0b1.                                                           */
  LibbluDtcpRetentionStateValue retentionState;  /**< Retention State.
    Allowed retention time for 'dtcpCci' == #LIBBLU_DTCP_CCI_COPY_NEVER
    content. Shall be set to #LIBBLU_DTCP_RET_STATE_90_MINUTES when
    'dtcpCci' != #LIBBLU_DTCP_CCI_COPY_NEVER.                                */
  bool epn;  /**< Encryption Plus Non-assertion.
    0b0: EPN-asserted, 0b1: EPN-unasserted/Reserved.
    If EPN is asserted, content nor its legit copies shall be allowed to be
    retransmited over the internet.
    This value is only refered if #dtcpCci == #LIBBLU_DTCP_CCI_COPY_FREE and
    so shall be set to true otherwise (meaning 'Reserved').                  */
  LibbluDtcpCopyControlInformationValue dtcpCci;  /**<  DTCP Copy Control
    Information mode.                                                        */
  bool dot;  /**< Digital Only Token.
    0b0: DOT-asserted, allows carriage of decrypted content over Analog
    outputs as well as Digital outputs;
    0b1: DOT-unasserted/Reserved, restrict carriage of decrypted content
    to Digital outputs (and forbids Analog ones).

    \note According to AACS specifications, this field is reserved and shall
    be set to 0b1.                                                           */
  bool ast;  /**< Analog Sunset Token.
    0b0: AST-asserted, allows carriage of all decrypted content over Analog
    outputs as well as Digital outputs;
    0b1: AST-unasserted/Reserved, only allows carriage of SD interlaced
    content over Analog outputs.

    \deprecated Licensed products must ignore this field. This
    field shall be set to 0b1 now ('Reserved'). The imageConstraintToken
    field shall be rather used.
    \note According to AACS specifications, this field is reserved and shall
    be set to 0b1.                                                           */
  bool imageConstraintToken; /**< Image Contraint Token.
    0b0: High Definition Analog Output in the form of Constrained Image;
    0b1: High Definition Analog Output is unrestricted and can output High
    Definition Analog form.                                                  */
  LibbluDtcpAnalogCopyProtectionInformationValue aps;  /**< Analog Copy
    Protection Information mode.                                             */
} LibbluDtcpSettings;

/** \~english
 * \brief Set DTCP settings as a Embedded CCI for the HDMV copy control
 * descriptor.
 *
 * \param dst Destination structure.
 * \param retentionMoveMode Retention_Move_Mode value.
 * \param retentionState Retention_State value.
 * \param epn EPN value.
 * \param cci CCI value.
 * \param dot DOT value.
 * \param imageConstraintToken Image_Constraint_Token value.
 * \param aps APS value.
 *
 * Fields are defined in 3.11 chapter of [1]. See #LibbluDtcpSettings for
 * fields meaning.
 * \note DOT parameter is present as it is in AACS Basic CCI.
 */
void setHdmvLibbluDtcpSettings(
  LibbluDtcpSettings *dst,
  bool retentionMoveMode,
  LibbluDtcpRetentionStateValue retentionState,
  bool epn,
  LibbluDtcpCopyControlInformationValue cci,
  bool dot,
  bool imageConstraintToken,
  LibbluDtcpAnalogCopyProtectionInformationValue aps
);

/** \~english
 * \brief Set DTCP settings as a Embedded CCI for the HDMV copy control
 * descriptor with default unencrypted settings.
 *
 * \param dst Destination structure.
 */
void setHdmvDefaultUnencryptedLibbluDtcpSettings(
  LibbluDtcpSettings *dst
);

#endif