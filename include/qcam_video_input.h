/**
*                      Copyleft (C) 2019  Late Lee
*       This program is tested on LINUX PLATFORM, WITH GCC 4.x.
*       The program is distributed in the hope that it will be useful,
*       but WITHOUT ANY WARRANTY. Any questions or suggestions, or bugs,
*       please contact me at SZ or e-mail to youngds@yingshixun.com or 756729890@qq.com,
*       or zhaohuaqiang@360.cn. if you want to do this.

* @file   qcam_video_input.h
* @author Echo Young
* @date   2019-06-13 16:48:05
*
* @brief  contain video functions
* @note   This is a note.
* @warning This is a warning.
* @bug    This is a bug.
*/

#ifndef _QCAM_VIDEO_CTRL_H
#define _QCAM_VIDEO_CTRL_H
#include <stdbool.h>

// H264视频采集回调
// tv 定义为采集时的时戳，gettimeofday(&tv, NULL);  必须真实
// data 为NALU数据（以 00 00 00 01开始的）
// keyframe 为关键帧标志。如果是关键帧，data需要同时包含NALU SPS、PPS
typedef void (*QCam_Video_Input_cb)(const struct timeval *tv, const void *data,
                                            const int len, const int keyframe);
// 镜头遮挡回调，shade 是否遮挡，0 未遮挡，非 0 遮挡
typedef void (*Ysx_ShadeDetect_cb)(int shade);

// 视频流分辨率
typedef enum {
    QCAM_VIDEO_RES_INVALID = 0,
    QCAM_VIDEO_RES_720P,                // 分辨率 1280x720
    QCAM_VIDEO_RES_480P,                // 分辨率 720x480
    QCAM_VIDEO_RES_360P,                // 分辨率 640x360
    QCAM_VIDEO_RES_1080P
}QCAM_VIDEO_RESOLUTION;

typedef enum {
    QCAM_VIDEO_NIGHT = 0,
    QCAM_VIDEO_DAY,
}QCAM_VIDEO_HASLIGHT;

typedef enum {
    QCAM_IR_MODE_UNSUPPORT = -1,    // 硬件不支持红外
    QCAM_IR_MODE_AUTO = 0,          // 自动红外。由SDK层自动切换
    QCAM_IR_MODE_ON = 1,            // 强制开红外，SDK不自动切换
    QCAM_IR_MODE_OFF = 2,           // 强制关红外，SDK不自动切换
    QCAM_IR_MODE_EXIT,              // 强制关红外，退出红外模式
}QCAM_IR_MODE;

typedef struct {
    int channelId;                  // channel ID，用于控制接口，如修改bitrate
    QCAM_VIDEO_RESOLUTION  res;     // resolution
    int fps;                        // fps
    int bitrate;                    // h264 bitrate (kbps)
    int gop;                        // h264 I帧间隔（秒），如果sdk有自动降帧逻辑，要保证I帧间隔不变。
    int vbr;                        // VBR=1, CBR=0
    QCam_Video_Input_cb cb;         // callback
}QCamVideoInputChannel;

// OSD
typedef struct {
    int pic_enable;                 // 打开/关闭 图片水印 OSD
    char pic_path[128];             // 图片路径
    int pic_x;                      // 图片在屏幕的显示位置（左上角坐标）
    int pic_y;
    int time_enable;                // 打开、关闭 时戳OSD（暂时无需支持，上层应用未使用）
    int time_x;
    int time_y;
}QCamVideoInputOSD;

/**
 * @fn int QCamVideoInput_SetOSD(int channel, QCamVideoInputOSD *pOsdInfo);
 *
 * 控制视频OSD设置
 *
 * @param[in] channel 通道 视频出流通道号
 * @param[in] pOsdInfo 属性 OSD配置属性.
 *
 * @retval 0 成功
 * @retval -1 失败
 *
 * @remarks 调用此 API 时 QCamVideoInput_Init 被调用。
 *
 * @attention 无。
 */
int QCamVideoInput_SetOSD(int channel, QCamVideoInputOSD *pOsdInfo);
/**
 * @fn int Ysx_VideoInput_OSD_Switch(int osd_type, int val)
 *
 * 控制视频OSD开关
 *
 * @param[in] osd_type osd 模式，时间/图片
 * @param[in] val 0, off. 1, on.
 *
 * @retval 0 成功
 * @retval -1 失败
 *
 * @remarks 调用此 API 时 QCamVideoInput_Init 被调用。
 * @remarks 调用此 API 时 QCamVideoInput_SetOSD 被调用。
 *
 * @attention 无。
 */
int Ysx_VideoInput_OSD_Switch(int osd_type, int val);
/**
 * @fn void QCamSetIRMode(QCAM_IR_MODE mode);
 *
 * 设置红外模式
 * 该函数可以不被调用，SDK默认为自动模式
 * 该函数还可以随时被调用，SDK做相应处理

 * @param[in] 无
 *
 * @retval 0 成功
 * @retval -1 失败
 *
 * @remarks 自动 强制开启 强制关闭
 *
 * @attention 无
 */
void QCamSetIRMode(QCAM_IR_MODE mode);
/**
 * @fn QCAM_IR_MODE QCamGetIRMode(void);
 *
 * 获取红外模式

 * @param[in] 无
 *
 * @retval QCAM_IR_MODE
 *
 * @remarks 无
 *
 * @attention 无
 */
QCAM_IR_MODE QCamGetIRMode(void);
/**
 * @fn int QcamVideoGetStatus(void);
 *
 * 获取 video 是否工作状态
 *
 * @param[in] 无
 *
 * @retval 0 成功
 * @retval -1 失败
 *
 * @remarks 无
 *
 * @attention 无
 */
int QcamVideoGetStatus(void);
/**
 * @fn int QCamVideoInput_Init(void);
 *
 * video 初始化
 * 使用流程：Open -> Add多个频道参数->Start(). 最终用完 Close();
 *
 * @param[in] 无
 *
 * @retval 0 成功
 * @retval -1 失败
 *
 * @remarks 无
 *
 * @attention 无
 */
int QCamVideoInput_Init(void);
/**
 * @fn int QCamVideoInput_AddChannel(QCamVideoInputChannel cn);
 *
 * 视频采集-频道属性
 * 因为不同设备硬件不一样, 所以IQ由QCAM层内部调整
 * 外部抽象层只设置基本参数
 * 创建通道，设置并做好绑定通道
 *
 * @param[in] 无
 *
 * @retval 0 成功
 * @retval -1 失败
 *
 * @remarks 无
 *
 * @attention 无
 */
int QCamVideoInput_AddChannel(QCamVideoInputChannel cn);
/**
 * @fn int QCamVideoInput_Start(void);
 *
 * video 准备并开启
 *
 * @param[in] 无
 *
 * @retval 0 成功
 * @retval -1 失败
 *
 * @remarks 无
 *
 * @attention 无
 */
int QCamVideoInput_Start(void);
/**
 * @fn int QCamVideoInput_CatchJpeg(char *buf, int *bufLen);
 *
 // 抓图
 // bufLen 输入参数为buf最大长度。输出参数为实际长度
 // 截图规格：分辨率同主码流（720p）要求有水印
 // 画质：720p图片大概60KB
 *
 * @param[in] 无
 *
 * @retval 0 成功
 * @retval -1 失败
 *
 * @remarks 无
 *
 * @attention 无
 */
int QCamVideoInput_CatchJpeg(char *buf, int *bufLen);
/**
 * @fn int QCamVideoInput_CatchYUV(int w, int h, char *buf, int bufLen);
 *
 * 抓YUV。格式为 YUV420 Planar
 * buf: Y数据在buf前面，大小为(W*H)字节，UV数据在Y数据后面，大小为(W*H/2)字节
 * size: 外界可以传入w*h来仅获取Y，也可以传入w*h*1.5来获取全部的
 *
 * @param[in] 无
 *
 * @retval 0 成功
 * @retval -1 失败
 *
 * @remarks 与抓取 JPEG 不同，需要设置数据流缓存
 *
 * @attention 无
 */
int QCamVideoInput_CatchYUV(int w, int h, char *buf, int bufLen);
/**
 * @fn int QCamVideoInput_SetQualityLvl(int channel, int low_enable);
 *
 * 码率控制模式支持 ENC_RC_MODE_FIXQP, ENC_RC_MODE_CBR, ENC_RC_MODE_VBR 与 ENC_RC_MODE_SMART
 *
 * @param[in] 无
 *
 * @retval 0 成功
 * @retval -1 失败
 *
 * @remarks 无
 *
 * @attention 调用此API会设置通道的码率控制模式属性，下一个 IDR 生效,调用此 API 需要通道已经存在
 */
int QCamVideoInput_SetQualityLvl(int channel, int low_enable);
/**
 * @fn int QCamVideoInput_SetBitrate(int channel, int bitrate, int isVBR);
 *
 * 设置码率
 *
 * @param[in] 无
 *
 * @retval 0 成功
 * @retval -1 失败
 *
 * @remarks 无
 *
 * @attention 目前仅支持 cbr 模式下调整，受制QP和实际数据量，存在实际码率超过所设置的码率。
 */
int QCamVideoInput_SetBitrate(int channel, int bitrate, int isVBR);
/**
 * @fn int QCamVideoInput_SetInversion(int enable);
 *
 * 是否镜像处理 video 输出
 *
 * @param[in] 无
 *
 * @retval 0 成功
 * @retval -1 失败
 *
 * @remarks 无
 *
 * @attention 无
 */
int QCamVideoInput_SetInversion(int enable);
/**
 * @fn int QCamVideoInput_SetIFrame(int channel);
 *
 * 强发 I 帧
 *
 * @param[in] 无
 *
 * @retval 0 成功
 * @retval -1 失败
 *
 * @remarks 无
 *
 * @attention 当前 video 缓存队列需要清空处理
 */
int QCamVideoInput_SetIFrame(int channel);
/**
 * @fn int QCamVideoInput_HasLight();
 *
 * 双光源模式下，led大灯是否开启
 *
 * @param[in] 无
 *
 * @retval 1 有光
 * @retval 0 没有光
 * @retval -1 检测失败
 *
 * @remarks 无
 *
 * @attention 仅在双光源相关项目中使用
 */
int QCamVideoInput_HasLight();
/**
 * @fn int QCamIVSChnSet(int chn, bool flag);
 *
 * ivs 工作通道设置
 *
 * @param[in] 无
 *
 * @retval 0 成功
 * @retval -1 失败
 *
 * @remarks debug使用
 *
 * @attention 无
 */
int QCamIVSChnSet(int chn, bool flag);
/**
 * @fn int QCamJpeg_Init(unsigned int width, unsigned int height);
 *
 * jpeg 初始化
 *
 * @param[in] 无
 *
 * @retval 0 成功
 * @retval -1 失败
 *
 * @remarks 无
 *
 * @attention 无
 */
int QCamJpeg_Init(unsigned int width, unsigned int height);
/**
 * @fn int QCamJpeg_Uninit(void);
 *
 * jpeg 去初始化操作
 *
 * @param[in] 无
 *
 * @retval 0 成功
 * @retval -1 失败
 *
 * @remarks 无
 *
 * @attention 无
 */
int QCamJpeg_Uninit(void);
/**
 * @fn int QCamVideoInput_Uninit(void);
 *
 * video 输入流去初始化
 *
 * @param[in] 无
 *
 * @retval 0 成功
 * @retval -1 失败
 *
 * @remarks 无
 *
 * @attention 去初始化后，输出流没有数据，也不能继续从输出流取数据
 */
int QCamVideoInput_Uninit(void);
#if (defined IPC_SHADE)
/**
 * @fn void Ysx_UninitShadeDetect(void);
 *
 * 去初始化遮挡算法，释放 ivs 运算资源，释放线程资源。
 *
 * @param[in] 无
 *
 * @retval 0 成功
 * @retval -1 失败
 *
 * @remarks 无
 *
 * @attention 无
 */
void Ysx_UninitShadeDetect(void);
/**
 * @fn void Ysx_InitShadeDetect(void( *cb)());
 *
 * 初始化遮挡算法。
 *
 * @param[in] 无
 *
 * @retval 0 成功
 * @retval -1 失败
 *
 * @remarks 无
 *
 * @attention sensor 出流正常，ivs 算法支持
 */
void Ysx_InitShadeDetect(void( *cb)());
#endif

//for realtek need
/* called at the very beginning */
void QCamAV_Context_Init(void);

/* called at the very last */
void QCamAV_Context_Release(void);

#endif
