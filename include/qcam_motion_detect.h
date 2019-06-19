/**
*                      Copyleft (C) 2019  Late Lee
*       This program is tested on LINUX PLATFORM, WITH GCC 4.x.
*       The program is distributed in the hope that it will be useful,
*       but WITHOUT ANY WARRANTY. Any questions or suggestions, or bugs,
*       please contact me at SZ or e-mail to youngds@yingshixun.com or 756729890@qq.com,
*       or zhaohuaqiang@360.cn. if you want to do this.

* @file   qcam_motion_detect.h
* @author Echo Young
* @date   2019-06-13 16:48:05
*
* @brief  contain motion detct arithmetics
* @note   This is a note.
* @warning This is a warning.
* @bug    This is a bug.
*/


#ifndef _QCAM_MOTION_DETECT_H
#define _QCAM_MOTION_DETECT_H

// 一般由360实现，需要硬件支持移动侦测

typedef enum {
    QCAM_MD_NONE = 0,
    QCAM_MD_FACE = 1,                   // 人脸
    QCAM_MD_WINDOW = 2,                 // 安防
    QCAM_MD_RECORD = 4,                 // 全屏动作检测，用于云录开始/停止
    QCAM_MD_FULLSCREEN = 8,             // 全屏动作检测，用于服务端进行未见人形检测
    QCAM_MD_MAX,
} QCAM_MOTION_DETECT_TYPE;

// 移动侦测回调函数
typedef void (*QCam_MD_cb)(int type);

/**
 * @fn void QCamInitMotionDetect(QCam_MD_cb cb)
 *
 * 初始化移动侦测，注册回调函数
 *
 * @param[in] QCam_MD_cb
 *
 * @param[in] 无
 *
 * @retval 无
 *
 * @remarks 无。
 *
 * @attention 无。
 *
 * @attention 无。
 */
void QCamInitMotionDetect(QCam_MD_cb cb);
/**
 * @fn void QCamStartFaceDetect(void)
 *
 * 开启人脸识别
 *
 * @param[in] 无
 *
 * @retval 无
 *
 * @remarks 无。
 *
 * @attention 无。
 */
void QCamStartFaceDetect(void);
/**
 * @fn void QCamStopFaceDetect(void)
 *
 * 关闭人脸识别
 *
 * @param[in] 无
 *
 * @retval 无
 *
 * @remarks 无。
 *
 * @attention 无。
 */
void QCamStopFaceDetect(void);
/**
 * @fn void QCamStartWindowAlert(double p1x, double p1y, double p2x, double p2y)
 *
 * 初始化指定窗口大小侦测
 *
 * @param[in] double 起点x坐标
 * @param[in] double 起点y坐标
 * @param[in] double 终点x坐标
 * @param[in] double 终点y坐标
 *
 * @retval 无
 *
 * @remarks 无
 *
 * @attention 无。
 */
void QCamStartWindowAlert(double p1x, double p1y, double p2x, double p2y);
/**
 * @fn void QCamStopWindowAlert(void)
 *
 * 关闭指定窗口大小侦测，反初始化
 *
 * @param[in] 无
 *
 * @retval 无
 *
 * @remarks 无。
 *
 * @attention 无。
 */
void QCamStopWindowAlert(void);
/**
 * @fn void QCamStartCloudRecord(void)
 *
 * 未知
 *
 * @param[in] 无
 *
 * @retval 无
 *
 * @remarks 无。
 *
 * @attention 无。
 */
void QCamStartCloudRecord(void);
/**
 * @fn void QCamStopCloudRecord(void)
 *
 * 未知
 *
 * @param[in] 无
 *
 * @retval 无
 *
 * @remarks 无。
 *
 * @attention 无。
 */
void QCamStopCloudRecord(void);
/**
 * @fn void QCamStartFullScreenDetect(void)
 *
 * 开启全屏侦测
 *
 * @param[in] 无
 *
 * @retval 无
 *
 * @remarks 无。
 *
 * @attention 无。
 */
void QCamStartFullScreenDetect(void);
/**
 * @fn void QCamStopFullScreenDetect(void)
 *
 * 关闭全屏侦测
 *
 * @param[in] 无
 *
 * @retval 无
 *
 * @remarks 无。
 *
 * @attention 无。
 */
void QCamStopFullScreenDetect(void);
/**
 * @fn void QCamMotionSet(void)
 *
 * 移动侦测灵敏度设置
 *
 * @param[in] sense 0-4， 依托sdk限制
 *
 * @retval 无
 *
 * @remarks 无。
 *
 * @attention 无。
 */
void QCamMotionSet(int sense);

#endif
