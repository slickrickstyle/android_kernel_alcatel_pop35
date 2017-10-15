////////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006-2014 MStar Semiconductor, Inc.
// All rights reserved.
//
// Unless otherwise stipulated in writing, any and all information contained
// herein regardless in any format shall remain the sole proprietary of
// MStar Semiconductor Inc. and be kept in strict confidence
// (??MStar Confidential Information??) by the recipient.
// Any unauthorized act including without limitation unauthorized disclosure,
// copying, use, reproduction, sale, distribution, modification, disassembling,
// reverse engineering and compiling of the contents of MStar Confidential
// Information is unlawful and strictly prohibited. MStar hereby reserves the
// rights to any and all damages, losses, costs and expenses resulting therefrom.
//
////////////////////////////////////////////////////////////////////////////////

/**
 *
 * @file    mstar_drv_platform_interface.c
 *
 * @brief   This file defines the interface of touch screen
 *
 *
 */

/*=============================================================*/
// INCLUDE FILE
/*=============================================================*/

#include "mstar_drv_platform_interface.h"
#include "mstar_drv_main.h"
#include "mstar_drv_ic_fw_porting_layer.h"
#include "mstar_drv_platform_porting_layer.h"
#include "mstar_drv_utility_adaption.h"

#ifdef CONFIG_ENABLE_HOTKNOT
#include "mstar_drv_hotknot.h"
#endif //CONFIG_ENABLE_HOTKNOT

/*=============================================================*/
// EXTERN VARIABLE DECLARATION
/*=============================================================*/

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
extern u32 g_GestureWakeupMode[2];
extern u8 g_GestureWakeupFlag;

#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
extern u8 g_GestureDebugFlag;
extern u8 g_GestureDebugMode;
#endif //CONFIG_ENABLE_GESTURE_DEBUG_MODE

#endif //CONFIG_ENABLE_GESTURE_WAKEUP

#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION
extern u8 g_EnableTpProximity;
#endif //CONFIG_ENABLE_PROXIMITY_DETECTION

/*=============================================================*/
// GLOBAL VARIABLE DEFINITION
/*=============================================================*/

extern struct input_dev *g_InputDevice;

#ifdef CONFIG_ENABLE_HOTKNOT
extern u8 g_HotKnotState;
#endif //CONFIG_ENABLE_HOTKNOT

//add by mike.li for suspend & resume.[2015.07.07]
#if defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
struct notifier_block TPfb_notifier;
#endif

/*=============================================================*/
// GLOBAL FUNCTION DEFINITION
/*=============================================================*/

//#ifdef CONFIG_ENABLE_NOTIFIER_FB
#if 0
int MsDrvInterfaceTouchDeviceFbNotifierCallback(struct notifier_block *pSelf, unsigned long nEvent, void *pData)
{
    struct fb_event *pEventData = pData;
    int *pBlank;

    if (pEventData && pEventData->data && nEvent == FB_EVENT_BLANK)
    {
        pBlank = pEventData->data;

        if (*pBlank == FB_BLANK_UNBLANK)
        {
            DBG("*** %s() TP Resume ***\n", __func__);

#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION
            if (g_EnableTpProximity == 1)
            {
                DBG("g_EnableTpProximity = %d\n", g_EnableTpProximity);
                return 0;
            }
#endif //CONFIG_ENABLE_PROXIMITY_DETECTION
            
#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
            if (g_GestureDebugMode == 1)
            {
                DrvIcFwLyrCloseGestureDebugMode();
            }
#endif //CONFIG_ENABLE_GESTURE_DEBUG_MODE

            if (g_GestureWakeupFlag == 1)
            {
                DrvIcFwLyrCloseGestureWakeup();
            }
            else
            {
                DrvPlatformLyrEnableFingerTouchReport(); 
            }
#endif //CONFIG_ENABLE_GESTURE_WAKEUP
    
#ifdef CONFIG_ENABLE_HOTKNOT
            if (g_HotKnotState != HOTKNOT_BEFORE_TRANS_STATE && g_HotKnotState != HOTKNOT_TRANS_STATE && g_HotKnotState != HOTKNOT_AFTER_TRANS_STATE)
#endif //CONFIG_ENABLE_HOTKNOT        
            {
                DrvPlatformLyrTouchDevicePowerOn(); 
            }   
    
#ifdef CONFIG_ENABLE_FIRMWARE_DATA_LOG
            DrvIcFwLyrRestoreFirmwareModeToLogDataMode(); // Mark this function call for avoiding device driver may spend longer time to resume from suspend state.
#endif //CONFIG_ENABLE_FIRMWARE_DATA_LOG

#ifndef CONFIG_ENABLE_GESTURE_WAKEUP
            DrvPlatformLyrEnableFingerTouchReport(); 
#endif //CONFIG_ENABLE_GESTURE_WAKEUP
        }
        else if (*pBlank == FB_BLANK_POWERDOWN)
        {
            DBG("*** %s() TP Suspend ***\n", __func__);
            
#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION
            if (g_EnableTpProximity == 1)
            {
                DBG("g_EnableTpProximity = %d\n", g_EnableTpProximity);
                return 0;
            }
#endif //CONFIG_ENABLE_PROXIMITY_DETECTION

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
            if (g_GestureWakeupMode[0] != 0x00000000 || g_GestureWakeupMode[1] != 0x00000000)
            {
                DrvIcFwLyrOpenGestureWakeup(&g_GestureWakeupMode[0]);
                return 0;
            }
#endif //CONFIG_ENABLE_GESTURE_WAKEUP


            DrvPlatformLyrFingerTouchReleased(0, 0); // Send touch end for clearing point touch
            input_sync(g_InputDevice);

            DrvPlatformLyrDisableFingerTouchReport();

#ifdef CONFIG_ENABLE_HOTKNOT
            if (g_HotKnotState != HOTKNOT_BEFORE_TRANS_STATE && g_HotKnotState != HOTKNOT_TRANS_STATE && g_HotKnotState != HOTKNOT_AFTER_TRANS_STATE)
#endif //CONFIG_ENABLE_HOTKNOT        
            {
                DrvPlatformLyrTouchDevicePowerOff(); 
            }    
        }
    }

    return 0;
}

#else
extern u8 _gIsUpdateFirmware;
void MsDrvInterfaceTouchDeviceSuspend(void)
{
    DBG("*** %s() ***\n", __func__);
#ifdef CONFIG_UPDATE_FIRMWARE_BY_SW_ID
	if( _gIsUpdateFirmware != 0x00)
		return;
#endif

	/* modify by furong, 2014/12/10, PR-865304, add for sometimes TP can't work after suspend/resume. */
	#if 0
	if (atomic_read(&tp_suspended)) {
		DBG("****Already in suspend state*****\n");
		return;
	}
	atomic_set(&tp_suspended, 1);
	#endif

	DrvPlatformLyrDisableFingerTouchReport();
	DrvPlatformLyrFingerTouchReleased(0, 0); // Send touch end for clearing point touch
	input_sync(g_InputDevice);

    //DrvPlatformLyrDisableFingerTouchReport();
    DrvPlatformLyrTouchDevicePowerOff(); 
    DrvPlatformLyrTouchDeviceVoltageControl(false);//zxzadd
}

void MsDrvInterfaceTouchDeviceResume(void)
{
	//Add by Luoxingxing.
	int i = 0;

    DBG("*** %s() ***\n", __func__);

#ifdef CONFIG_UPDATE_FIRMWARE_BY_SW_ID
	if( _gIsUpdateFirmware != 0x00)
		return;
#endif

/* modify by furong, 2014/12/10, PR-865304, add for sometimes TP can't work after suspend/resume.
  because sometimes suspend twice but resume once, this leads to irq disabled twice but enabled only one time,
  irq disabled times not equal to enabled times will cause TP irq not correctly enable and TP can't work. */
	#if 0
	if (!atomic_read(&tp_suspended)) {
		DBG("****Already in awake state*****\n");
		return;
	}
	atomic_set(&tp_suspended, 0);
	#endif

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
	if (g_GestureWakeupFlag != 0x0000)
	{
		DrvIcFwLyrCloseGestureWakeup();
		DrvPlatformLyrDisableFingerTouchReport(); //modify by furong, 2014.12.10, to make irq enable times are equal to irq disable times
	}
#endif

    DrvPlatformLyrTouchDeviceVoltageControl(true);//zxzadd
    DrvPlatformLyrTouchDevicePowerOn();
	mdelay(50);

#ifdef CONFIG_ENABLE_FIRMWARE_DATA_LOG
//    DrvIcFwLyrRestoreFirmwareModeToLogDataMode(); // Mark this function call for avoiding device driver may spend longer time to resume from suspend state.
#endif

    DrvPlatformLyrEnableFingerTouchReport(); 

	//Add by Luoxingxing to release all points when resume.START
	for (i = 0; i < MAX_TOUCH_NUM; i ++)
	{
		 input_report_abs(g_InputDevice, ABS_MT_TRACKING_ID, -1);
   		 input_mt_sync(g_InputDevice);
	}
	input_sync(g_InputDevice);
	//END
}

#endif //CONFIG_ENABLE_NOTIFIER_FB

#if 1
//add by mike.li for suspend & resume.[2015.07.07]
#if defined(CONFIG_FB)
static int fb_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;

	if (evdata && evdata->data && event == FB_EVENT_BLANK) {
		blank = evdata->data;
		if (*blank == FB_BLANK_UNBLANK)
			MsDrvInterfaceTouchDeviceResume();
		else if (*blank == FB_BLANK_POWERDOWN)
			MsDrvInterfaceTouchDeviceSuspend();
	}

	return 0;
}
#endif
#endif
extern int msg2638_pinctrl_init(struct i2c_client *pClient);
/* probe function is used for matching and initializing input device */
s32 /*__devinit*/ MsDrvInterfaceTouchDeviceProbe(struct i2c_client *pClient, const struct i2c_device_id *pDeviceId)
{
    s32 nRetVal = 0;

    DBG("*** %s() ***\n", __func__);
    DrvPlatformLyrTouchDeviceVoltageInit(pClient,true);//zxzadd
    DrvPlatformLyrTouchDeviceVoltageControl(true);//zxzadd

    nRetVal = msg2638_pinctrl_init(pClient);
	if (nRetVal < 0) {
		DBG("MIKE: msg2638_pinctrl_init err : %d\n", nRetVal);
		return nRetVal;
	}

    nRetVal = DrvPlatformLyrTouchDeviceRequestGPIO();
	if (nRetVal < 0) {
		DBG("MIKE: DrvPlatformLyrTouchDeviceRequestGPIO err : %d\n", nRetVal);
		return nRetVal;
	}

#ifdef CONFIG_ENABLE_REGULATOR_POWER_ON
    DrvPlatformLyrTouchDeviceRegulatorPowerOn();
#endif //CONFIG_ENABLE_REGULATOR_POWER_ON

    DrvPlatformLyrTouchDevicePowerOn();

    nRetVal = DrvPlatformLyrInputDeviceInitialize(pClient);
	if (nRetVal < 0) {
		DBG("MIKE: DrvPlatformLyrInputDeviceInitialize err : %d\n", nRetVal);
		return nRetVal;
	}
#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_DMA_IIC
    DmaAlloc();
#endif //CONFIG_ENABLE_DMA_IIC
#endif //CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM

    nRetVal = DrvMainTouchDeviceInitialize();
    if (nRetVal == -ENODEV)
    {
        DrvPlatformLyrTouchDeviceRemove(pClient);
        return nRetVal;
    }

    DrvPlatformLyrTouchDeviceRegisterFingerTouchInterruptHandler();

    //DrvPlatformLyrTouchDeviceRegisterEarlySuspend();

//add by mike.li for suspend & resume.[2015.07.07]
#if defined(CONFIG_FB)
	TPfb_notifier.notifier_call = fb_notifier_callback;

	nRetVal = fb_register_client(&TPfb_notifier);

	if (nRetVal)
		dev_err(&pClient->dev, "Unable to register fb_notifier: %d\n",
			nRetVal);
#endif

#ifdef CONFIG_UPDATE_FIRMWARE_BY_SW_ID
    DrvIcFwLyrCheckFirmwareUpdateBySwId();
#endif //CONFIG_UPDATE_FIRMWARE_BY_SW_ID

    DBG("*** MStar touch driver registered ***\n");

	tp_dbg = 0;
    return nRetVal;
}

/* remove function is triggered when the input device is removed from input sub-system */
s32 /*__devexit*/ MsDrvInterfaceTouchDeviceRemove(struct i2c_client *pClient)
{
    DBG("*** %s() ***\n", __func__);

    return DrvPlatformLyrTouchDeviceRemove(pClient);
}

void MsDrvInterfaceTouchDeviceSetIicDataRate(struct i2c_client *pClient, u32 nIicDataRate)
{
    DBG("*** %s() ***\n", __func__);

    DrvPlatformLyrSetIicDataRate(pClient, nIicDataRate);
}    
