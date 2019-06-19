/**
*                      Copyleft (C) 2019  Late Lee
*       This program is tested on LINUX PLATFORM, WITH GCC 4.x.
*       The program is distributed in the hope that it will be useful,
*       but WITHOUT ANY WARRANTY. Any questions or suggestions, or bugs,
*       please contact me at SZ or e-mail to youngds@yingshixun.com or 756729890@qq.com,
*       or zhaohuaqiang@360.cn. if you want to do this.

* @file   qcam_audio_input.h
* @author Echo Young
* @date   2019-06-13 16:07:38
*
* @brief  audio in with Micphone
* @note   This is a note.
* @warning This is a warning.
* @bug    This is a bug.
*/

#ifndef _QCAM_AUDIO_INPUT_H_
#define _QCAM_AUDIO_INPUT_H_

#define MIC_VOLUME_RANGE                100

// 采集回调函数
// tv 定义为采集时的时戳，gettimeofday(&tv, NULL); 必须真实
typedef void (*QCamAudioInputCallback_aec)(const struct timeval *tv, const void *pcm_buf,
                                                    const int pcm_len, const void *spk_buf);

typedef struct {
    int sampleRate;                     // sample Hz（必须支持48Khz采样，可选支持8k、16Khz）
    int sampleBit;                      // 16 or 8 (bits)
    int volume;                         // 音量 [0~100]， -1 表示使用系统默认，不进行修改
    QCamAudioInputCallback_aec cb;      // callback
}QCamAudioInputAttr_aec;

/**
 * @fn QCamAudioInputOpen_ysx(QCamAudioInputAttr_aec *pAttr)
 *
 * 打开设备 初始化AI
 *
 * @param[in] QCamAudioInputAttr_aec AI采样率等配置
 *
 * @retval 0 成功
 * @retval -1 失败
 *
 * @remarks 无
 *
 * @attention 无。
 */
int QCamAudioInputOpen_ysx(QCamAudioInputAttr_aec *pAttr);
/**
 * @fn int QCamAudioInputClose_ysx(void)
 *
 * 关闭设备
 *
 * @param[in] null
 *
 * @retval 0 成功
 * @retval -1 失败
 *
 * @remarks 调用此 API 完成AI disable。
 *
 * @attention 无。
 */
int QCamAudioInputClose_ysx(void);
/**
 * @fn int QCamAudioInputStart(void)
 *
 * start/stop 用于控制是否采集callback发生
 * 开启降噪，AEC
 * 开启线程
 *
 * @param[in] null
 *
 * @retval 0 成功
 * @retval -1 失败
 *
 * @remarks 无
 *
 * @attention 无。
 */
int QCamAudioInputStart(void);
/**
 * @fn int QCamAudioInputStop(void)
 *
 * start/stop 用于控制是否采集callback发生
 *
 * @param[in] null
 *
 * @retval 0 成功
 * @retval -1 失败
 *
 * @remarks 无
 *
 * @attention 无。
 */
int QCamAudioInputStop(void);
/**
 * @fn int QCamAudioInputSetVolume(int vol)
 *
 * 设置AI音量
 * 不仅仅调整again
 *
 * @param[in] vol 音量，-1 不修改，[-30 ~ 120]. -30代表静音,120表示将声音放大30dB,步长0.5dB. 60是音量设置的一个临界点
 *
 * @retval 0 成功
 * @retval -1 失败
 *
 * @remarks 无
 *
 * @attention 无。
 */
int QCamAudioInputSetVolume(int vol);
/**
 * @fn int QCamAudioAecEnable(int enable)
 *
 * 开启、关闭AEC
 *
 * @param[in] enable 非0: 开启，0: 关闭
 *
 * @retval 0 成功
 * @retval -1 失败
 *
 * @remarks 无
 *
 * @attention 无。
 */
int QCamAudioAecEnable(int enable);

/**
 * @fn int QCamAudioInputSetGain(int gain)
 *
 * 设置AIgain值
 *
 * @param[in] gain 限制依托sdk限制
 *
 * @retval 0 成功
 * @retval -1 失败
 *
 * @remarks 无
 *
 * @attention 无。
 */
void QCamAudioInputSetGain(int gain);

#endif
