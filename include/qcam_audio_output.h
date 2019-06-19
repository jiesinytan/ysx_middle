/**
*                      Copyleft (C) 2019  Late Lee
*       This program is tested on LINUX PLATFORM, WITH GCC 4.x.
*       The program is distributed in the hope that it will be useful,
*       but WITHOUT ANY WARRANTY. Any questions or suggestions, or bugs,
*       please contact me at SZ or e-mail to youngds@yingshixun.com or 756729890@qq.com,
*       or zhaohuaqiang@360.cn. if you want to do this.

* @file   qcam_audio_output.h
* @author Echo Young
* @date   2019-06-13 16:46:56
*
* @brief  audio out with speaker
* @note   This is a note.
* @warning This is a warning.
* @bug    This is a bug.
*/

#ifndef _QCAM_AUDIO_OUTPUT_H_
#define _QCAM_AUDIO_OUTPUT_H_

#define SPEAKER_VOLUME_RANGE      		100

typedef struct {
    int sampleRate;                             // sample Hz
    int sampleBit;                              // 16 or 8 bit
    int volume;                                 // 音量 [0~100]， -1 表示使用系统默认，不进行修改
}QCamAudioOutputAttribute;

// 打开设备

typedef struct {
    int total;                                  // buffer最多存储时长 (ms)
    int busy;                                   // buffer正在播放时长 (ms)
}QCamAudioOutputBufferStatus;

int QCamAudioOutputQueryBuffer(QCamAudioOutputBufferStatus *pStat);
/**
 * @fn QCamAudioOutputOpen(QCamAudioOutputAttribute *pAttr)
 *
 * 打开设备 初始化AO
 *
 * @param[in] QCamAudioOutputAttribute AO采样率等配置
 *
 * @retval 0 成功
 * @retval -1 失败
 *
 * @remarks 无
 *
 * @attention 无。
 */
int QCamAudioOutputOpen(QCamAudioOutputAttribute *pAttr);
/**
 * @fn int QCamAudioOutputClose(void)
 *
 * 关闭设备
 *
 * @param[in] null
 *
 * @retval 0 成功
 * @retval -1 失败
 *
 * @remarks 调用此 API 完成AO disable。
 *
 * @attention 无。
 */
int QCamAudioOutputClose(void);
/**
 * @fn int QCamAudioOutputPlay_ysx(char *pcm_data, int len)
 *
 * 播放PCM音频（阻塞式播放）
 * 播放格式为原始PCM，无需编解码
 * 需要支持任意大小的PCM数据播放。自动切割并缓存。
 * 即：多余一帧时开始播放，少于一帧时需要缓存，待下次调用时拼接数据，攒够一帧再开始播
 *
 * @param[in] pcm_data  pcm 原始数据
 * @param[in] len       pcm 原始数据长度
 *
 * @retval 0 成功
 * @retval -1 失败
 *
 * @remarks 无
 *
 * @attention 无。
 */
int QCamAudioOutputPlay_ysx(char *pcm_data, int len);
/**
 * @fn QCamAudioOutputSetVolume(int vol)
 *
 * 音量 [0~100]
 *
 * @param[in] vol 播放原始pcm数据音量
 *
 * @retval 0 成功
 * @retval -1 失败
 *
 * @remarks 不仅仅是调整again
 *
 * @attention 无。
 */
int QCamAudioOutputSetVolume(int vol);
#endif
