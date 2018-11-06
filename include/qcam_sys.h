//-----------------------------------------------------
// 360Cam 硬件抽象层
// 系统控制（灯控，按键，烧录，看门狗等）
// author : zhaohuaqiang@360.cn
//-----------------------------------------------------

#ifndef _QCAM_SYS_H
#define _QCAM_SYS_H

// KEY STATUS
typedef enum
{
    KEY_INVALID = -1,
	KEY_RELEASED = 0,
	KEY_PRESSED = 1,
}QCAM_KEY_STATUS;

//Callback for key status
typedef void (*QCam_Key_Status_cb)(const int status);

// LED 灯控
typedef enum
{
	LED_MODE_OFF 			= 0,
	LED_MODE_GREEN 			= 1,
	LED_MODE_BLUE 			= 2,
	LED_MODE_RED 			= 3,
	LED_MODE_NORMAL_MAX	 	= 4,
	LED_MODE_GREEN_Q,
	LED_MODE_BLUE_Q,
	LED_MODE_RED_Q,
	LED_MODE_MAX,
}QCAM_LED_MODE;

int QCamLedSet(QCAM_LED_MODE mode, int blink);

// 获取按键状态 0=未按下 1=按下 -1=检测失败
int QCamGetKeyStatus();
int QCamGetKey2Status();

void QCamRegKeyListener(QCam_Key_Status_cb cb);

int QCamFlashBurn(const char *firmwarePath);

#endif
