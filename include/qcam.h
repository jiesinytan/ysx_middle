/**
*                      Copyleft (C) 2019  Late Lee
*       This program is tested on LINUX PLATFORM, WITH GCC 4.x.
*       The program is distributed in the hope that it will be useful,
*       but WITHOUT ANY WARRANTY. Any questions or suggestions, or bugs,
*       please contact me at SZ or e-mail to youngds@yingshixun.com or 756729890@qq.com,
*       or zhaohuaqiang@360.cn. if you want to do this.

* @file   qcam.h
* @author Echo Young
* @date   2019-06-13 16:48:05
*
* @brief  contain audio in, audio out, video and sys utils
* @note   This is a note.
* @warning This is a warning.
* @bug    This is a bug.
*/

#ifndef _QCAM_H_
#define _QCAM_H_

#define QCAM_OK		(0)
#define QCAM_FAIL	(-1)

// 依据QCAM为c实现还是C++实现，在makefile定义不同的值

#if (defined QCAM_AV_C_API)  && (defined __cplusplus)
	extern "C" {
#endif
	#include "qcam_audio_input.h"
	#include "qcam_audio_output.h"
	#include "qcam_video_input.h"
	#include "qcam_motion_detect.h"
#if (defined QCAM_AV_C_API)  && (defined __cplusplus)
	}
#endif


#if (defined QCAM_SYS_C_API)  && (defined __cplusplus)
	extern "C" {
#endif
	#include "qcam_sys.h"
#if (defined QCAM_SYS_C_API)  && (defined __cplusplus)
	}
#endif

#endif
