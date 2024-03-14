#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#include "dtcpSettings.h"

void setHdmvLibbluDtcpSettings(
  LibbluDtcpSettings *dst,
  bool retentionMoveMode,
  LibbluDtcpRetentionStateValue retentionState,
  bool epn,
  LibbluDtcpCopyControlInformationValue cci,
  bool dot,
  bool imageConstraintToken,
  LibbluDtcpAnalogCopyProtectionInformationValue aps
)
{
  assert(NULL != dst);

  *dst = (LibbluDtcpSettings) {
    .caSystemId = DTCP_CA_SYSTEM_ID_HDMV,
    .retentionMoveMode = retentionMoveMode,
    .retentionState = retentionState,
    .epn = epn,
    .dtcpCci = cci,
    .dot = dot,
    .ast = 0x1,
    .imageConstraintToken = imageConstraintToken,
    .aps = aps
  };
}

void setHdmvDefaultUnencryptedLibbluDtcpSettings(
  LibbluDtcpSettings *dst
)
{
  setHdmvLibbluDtcpSettings(
    dst,
    0x1,
    LIBBLU_DTCP_RET_STATE_90_MINUTES,
    0x1,
    LIBBLU_DTCP_CCI_COPY_NEVER,
    0x1,
    0x1,
    LIBBLU_DTCP_APS_COPY_FREE
  );
}