/**
*                      Copyleft (C) 2019  Late Lee
*       This program is tested on LINUX PLATFORM, WITH GCC 4.x.
*       The program is distributed in the hope that it will be useful,
*       but WITHOUT ANY WARRANTY. Any questions or suggestions, or bugs,
*       please contact me at SZ or e-mail to youngds@yingshixun.com or 756729890@qq.com,
*       or zhaohuaqiang@360.cn. if you want to do this.

* @file   qcam_sys.h
* @author Echo Young
* @date   2019-06-13 16:48:05
*
* @brief  contain leds keys control, and watchdog, flash burn
* @note   This is a note.
* @warning This is a warning.
* @bug    This is a bug.
*/

#ifndef _QCAM_SYS_H
#define _QCAM_SYS_H

// KEY STATUS
typedef enum {
    QCAM_KEY_INVALID = -1,
    QCAM_KEY_RELEASED,
    QCAM_KEY_PRESSED,
}QCAM_KEY_STATUS;

// LED 灯控
typedef enum {
    LED_MODE_MIN            = -1,
    LED_MODE_OFF,           //led all off
    LED_MODE_ON,            //led all on
    LED_MODE_RED,           //led mod is red
    LED_MODE_GREEN,         //led mod is green/yellow
    LED_MODE_BLUE,
    LED_MODE_GB,            //led mod is green and blue
    LED_MODE_RG,
    LED_MODE_RB,
    LED_MODE_RGB,
    LED_MODE_NORMAL_MAX,
    LED_MODE_RED_Q,
    LED_MODE_GREEN_Q,
    LED_MODE_BLUE_Q,
    LED_MODE_GB_Q,
    LED_MODE_RG_Q,
    LED_MODE_RB_Q,
    LED_MODE_RGB_Q,
    LED_MODE_MAX,
}QCAM_LED_MODE;

typedef enum {
    LED_STATUS_MIN            = -1,
    LED_STATUS_OFF,             //led off
    LED_STATUS_ON,              //led on
    LED_STATUS_BLINK,           //led blink, fliker
    LED_STATUS_MAX,
}QCAM_LED_STATUS;

//Callback for key status
typedef void (*QCam_Key_Status_cb)(const int status);


/**
 * @fn int Ysx_Led_Init(void)
 *
 * 初始化LED RGB灯色
 *
 * @param[in] null
 *
 * @retval 0 成功
 * @retval -1 失败
 *
 * @remarks 调用此 API 完成pwm配置和 gpio 注册。
 *
 * @attention 无。
 */
int Ysx_Led_Init(void);
/**
 * @fn int Ysx_Led_Unnit(void)
 *
 * 去初始化LED RGB灯色
 *
 * @param[in] null
 *
 * @retval 0 成功
 * @retval -1 失败
 *
 * @remarks 调用此 API 完成pwm配置和 gpio 反注册。
 *
 * @attention 无。
 */
int Ysx_Led_Unnit(void);
/**
 * @fn int QCamLedSet(QCAM_LED_MODE mode, QCAM_LED_STATUS blink, int strength)
 *
 * 控制LED RGB灯色
 *
 * @param[in] mode QCAM_LED_MODE
 * @param[in] blink QCAM_LED_STATUS 0, off. 1, on. 2 blink.
 * @param[in] strength 0-100. need modified
 *
 * @retval 0 成功
 * @retval -1 失败
 *
 * @remarks 调用此 API 时 Ysx_Led_Init 被调用。
 *
 * @attention 无。
 */
int QCamLedSet(QCAM_LED_MODE mode, QCAM_LED_STATUS blink, int strength);
/**
 * @fn int QCamGetKeyStatus(void)
 *
 * 复位键状态获取
 * 上层调用自己计数，确定长短按
 * 底层完成gpio初始化，无需上层调用
 *
 * @param[in] 无
 *
 * @retval 0 未按下
 * @retval 1 按下
 * @retval -1 检测失败
 *
 * @remarks 无
 *
 * @attention 无。
 */
int QCamGetKeyStatus(void);
/**
 * @fn int QCamGetKey2Status(void)
 *
 * 报警键状态获取
 * 上层调用自己计数，确定长短按
 * 底层完成gpio初始化，无需上层调用
 *
 * @param[in] 无
 *
 * @retval 0 未按下
 * @retval 1 按下
 * @retval -1 检测失败
 *
 * @remarks 无
 *
 * @attention 无。
 */
int QCamGetKey2Status(void);
/**
 * @fn void QCamRegKeyListener(QCam_Key_Status_cb cb)
 *
 * 按键回调注册（复位按键）
 * 无需上层调用自己计数，确定长短按
 * 底层完成gpio初始化，无需上层调用
 *
 * @param[in] 回调函数
 *
 * @retval 无
 *
 * @remarks 无
 *
 * @attention 无。
 */
void QCamRegKeyListener(QCam_Key_Status_cb cb);
/**
 * @fn int QCamFlashBurn(const char *firmwarePath)
 *
 * 固件烧录到flash
 *
 * @param[in] firmwarePath 固件文件
 *
 * @retval 0 成功
 * @retval -1 失败
 *
 * @remarks 无
 *
 * @attention 无。
 */
int QCamFlashBurn(const char *firmwarePath);

#endif
