#ifndef OPENCMR2_PORT_VR_H
#define OPENCMR2_PORT_VR_H
#include "port/types.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Inert in ordinary builds and --no-vr. Input applies only to local player 1. */
BOOL VR_IsEnabled(void);
BOOL VR_GetDriveInput(float *steer, float *throttle, float *brake);
void VR_MergeKeyboard(BYTE keys[256]);
void VR_ShutdownInput(void);
#ifdef __cplusplus
}
#endif
#endif
