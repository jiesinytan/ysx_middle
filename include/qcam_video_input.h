
#ifndef _QCAM_VIDEO_CTRL_H
#define _QCAM_VIDEO_CTRL_H



// H264视频采集回调
// tv 定义为采集时的时戳，gettimeofday(&tv, NULL);  必须真实
// data 为NALU数据（以 00 00 00 01开始的）
// keyframe 为关键帧标志。如果是关键帧，data需要同时包含NALU SPS、PPS
typedef void (*QCam_Video_Input_cb)(const struct timeval *tv, const void *data, const int len, const int keyframe);

// 视频流分辨率
typedef enum
{
	QCAM_VIDEO_RES_INVALID = 0,
	QCAM_VIDEO_RES_720P,		// 分辨率 1280x720
	QCAM_VIDEO_RES_480P,        // 分辨率 720x480
	QCAM_VIDEO_RES_360P,		// 分辨率 640x360
	QCAM_VIDEO_RES_1080P
}QCAM_VIDEO_RESOLUTION;

typedef enum
{
	QCAM_VIDEO_NIGHT = 0,
	QCAM_VIDEO_DAY,
}QCAM_VIDEO_HASLIGHT;

typedef enum
{
	QCAM_IR_MODE_UNSUPPORT = -1,// 硬件不支持红外
	QCAM_IR_MODE_AUTO = 0,		// 自动红外。由SDK层自动切换
	QCAM_IR_MODE_ON   = 1,		// 强制开红外，SDK不自动切换
	QCAM_IR_MODE_OFF  = 2,		// 强制关红外，SDK不自动切换
}QCAM_IR_MODE;

// 视频采集-频道属性
// 因为不同设备硬件不一样, 所以IQ由QCAM层内部调整
// 外部抽象层只设置基本参数
typedef struct QCamVideoInputChannel_t
{
	int channelId;				// channel ID，用于控制接口，如修改bitrate
	QCAM_VIDEO_RESOLUTION  res;	// resolution
	int fps;					// fps
	int bitrate;				// h264 bitrate (kbps)
	int gop;					// h264 I帧间隔（秒），如果sdk有自动降帧逻辑，要保证I帧间隔不变。
	int vbr;					// VBR=1, CBR=0
	QCam_Video_Input_cb cb;		// callback
}QCamVideoInputChannel;


// 使用流程：Open -> Add多个频道参数->Start(). 最终用完 Close();
int QCamVideoInput_Init();
int QCamVideoInput_AddChannel(QCamVideoInputChannel ch);
int QCamVideoInput_Start();
int QCamVideoInput_Uninit();


// 调整比特率
int QCamVideoInput_SetBitrate(int channel, int bitrate, int isVBR);

// 图像倒置，全局生效
int QCamVideoInput_SetInversion(int enable);

// 强制下次出I帧
int QCamVideoInput_SetIFrame(int channel);


// OSD
typedef struct QCamVideoInputOSD_t
{
	int pic_enable;			// 打开/关闭 图片水印 OSD
	char pic_path[128];		// 图片路径
	int pic_x;				// 图片在屏幕的显示位置（左上角坐标）
	int pic_y;

	int time_enable;		// 打开、关闭 时戳OSD（暂时无需支持，上层应用未使用）
	int time_x;
	int time_y;
}QCamVideoInputOSD;


int QCamVideoInput_SetOSD(int channel, QCamVideoInputOSD *pOsdInfo);

// 抓图
// bufLen 输入参数为buf最大长度。输出参数为实际长度
// 截图规格：分辨率同主码流（720p）要求有水印
// 画质：720p图片大概60KB
// 返回QCAM_FAIL/QCAM_OK;
int QCamVideoInput_CatchJpeg(char *buf, int *bufLen);

// 抓YUV。格式为 YUV420 Planar
// buf: Y数据在buf前面，大小为(W*H)字节，UV数据在Y数据后面，大小为(W*H/2)字节
// size: 外界可以传入w*h来仅获取Y，也可以传入w*h*1.5来获取全部的
// 返回QCAM_FAIL/QCAM_OK;
int QCamVideoInput_CatchYUV(int w, int h, char *buf, int bufLen);

// 检测环境是否有光亮
// return 1=有光 0=没有光 -1=检测失败
int QCamVideoInput_HasLight();

// 设置红外控制模式
// 注意：该函数可以不被调用，SDK默认为自动模式
// 该函数还可以随时被调用，SDK做相应处理
void QCamSetIRMode(QCAM_IR_MODE mode);

// 返回当前红外控制模式。
QCAM_IR_MODE QCamGetIRMode();

int QCamVideoInput_SetQualityLvl(int channel, int low_enable);

#endif
