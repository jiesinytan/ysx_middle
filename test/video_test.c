#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdint.h>
#include <pthread.h>
#include <errno.h>

#include "../rts_middle_media.h"
#include <qcam_video_input.h>

QCamVideoInputChannel chn;
QCamVideoInputChannel stream[STREAM_COUNT];
QCamVideoInputChannel chn_default = {
	.channelId = 0,				/*MAIN_STREAM*/
	.res = QCAM_VIDEO_RES_1080P,	/*resolution*/
	.fps = 30,				/*fps*/
	.bitrate = 2048,				/*h264 bitrate (kbps)*/
	.gop = 1,				/*interval I frame(s)*/
	.vbr = YSX_BITRATE_MODE_CBR,	/*VBR=1, CBR=0*/
	.cb = NULL
};

QCamVideoInputOSD osd_default = {
	.pic_enable = 1,
	.pic_path = "/usr/osd_char_lib/argb_2222",
	.pic_x = 200,
	.pic_y = 200,
	.time_enable = 1,
	.time_x = 100,
	.time_y  = 100
};

int resolution[] = {
	QCAM_VIDEO_RES_INVALID,
	QCAM_VIDEO_RES_1080P,
	QCAM_VIDEO_RES_720P,
	QCAM_VIDEO_RES_480P,
	QCAM_VIDEO_RES_360P
};

int Stream_bitrate[] = {4096, 2048, 1024, 768, 512};
int key_frame;
int g_exit;
FILE *f_stream = NULL;
FILE *f_stream1 = NULL;
FILE *f_stream2 = NULL;

void sig_handle(int sig)
{
	g_exit = 1;
}

void h264_cb(const struct timeval *tv, const void *data,
	const int len, const int keyframe)
{
	static uint64_t count;
	int ret = 0;

	if (key_frame && keyframe)
		printf("keyframe = %d\n", keyframe);

	if (!f_stream)
		printf("file not exist, no space to save stream\n");

	ret = fwrite(data, sizeof(uint8_t), len, f_stream);
	if (ret != len)
		printf("lost data when save data, write[%d] get[%d]\n",
			len, ret);
	printf("get [%llu] frames\n", ++count);
}

void h264_cb1(const struct timeval *tv, const void *data,
	const int len, const int keyframe)
{
	static uint64_t count;
	int ret = 0;

	if (key_frame && keyframe)
		printf("keyframe = %d\n", keyframe);

	if (!f_stream1)
		printf("file not exist, no space to save stream\n");

	ret = fwrite(data, sizeof(uint8_t), len, f_stream1);
	if (ret != len)
		printf("lost data when save data, write[%d] get[%d]\n",
			len, ret);
	printf("get [%llu] frames\n", ++count);
}

void h264_cb2(const struct timeval *tv, const void *data,
	const int len, const int keyframe)
{
	static uint64_t count;
	int ret = 0;

	if (key_frame && keyframe)
		printf("keyframe = %d\n", keyframe);

	if (!f_stream2)
		printf("file not exist, no space to save stream\n");

	ret = fwrite(data, sizeof(uint8_t), len, f_stream2);
	if (ret != len)
		printf("lost data when save data, write[%d] get[%d]\n",
			len, ret);
	printf("get [%llu] frames\n", ++count);
}

int test(int btest, int w, int h)
{
	int ret = 0;
	static uint64_t index;
	char filename[64];
	FILE *pfile = NULL;
	char *buf;
	int buflen;

	if (btest) {
		buf = (char *)calloc(100 * 1024, sizeof(char)); /* JPEG max size */
		if (buf == NULL) {
			printf("malloc yuv buffer fail\n");
			return -1;
		}

		buflen = 100 * 1024;

		ret = QCamVideoInput_CatchJpeg(buf, &buflen);
		snprintf(filename, sizeof(filename), "%lld.jpg", index++);

		pfile = fopen(filename, "wb");
		if (!pfile)
			printf("open file %s fail\n", filename);

		ret = fwrite(buf, 1, buflen, pfile);
		printf("write [%d] bytes, want [%d] bytes\n", ret, buflen);
		fclose(pfile);

		free(buf);
		buf = NULL;
		return 0;
	} else {
		/* TODO fw support larger resolution */
		buflen = w * h + w * h / 2;
		buf = (char *)calloc(buflen, sizeof(char));
		if (buf == NULL) {
			printf("malloc yuv buffer fail\n");
			return -1;
		}

		ret = QCamVideoInput_CatchYUV(w, h, buf, buflen);
		snprintf(filename, sizeof(filename), "%lld.yuv", index++);

		pfile = fopen(filename, "w+");
		if (!pfile) {
			printf("open file %s fail\n", filename);
		}
		ret = fwrite(buf, 1, buflen, pfile);
		printf("write [%d] bytes, want [%d] bytes\n", ret, buflen);
		fclose(pfile);

		free(buf);
		buf = NULL;
		return 0;
	}
}

int main(int argc, char *argv[])
{
	int ret = 0;
	int qp = -1;
	int Invert, Snap, Catch_YUV, RM_Mode, OSD, Light, Change_bitrate;
	int QCAM_IRMODE;
	int w = 1280, h = 720;
	int ch;
	int mul_channel = 0;
	QCamVideoInputOSD osd_info;
	Invert = Snap = Catch_YUV = RM_Mode = OSD = Light = Change_bitrate = 0;
	QCam_Video_Input_cb fun_cb[] = {h264_cb, h264_cb1, h264_cb2};
	FILE *stream_file[3] = {f_stream, f_stream1, f_stream2};

	memset(&chn, 0, sizeof(QCamVideoInputChannel));
	memset(&osd_info, 0, sizeof(QCamVideoInputOSD));
	chn = chn_default;
	chn.cb = h264_cb;
	osd_info = osd_default;

	__init_middleware_context();

	while ((ch = getopt(argc, argv, "c:f:b:v:t:y:m:q:kisloh")) != -1) {
		switch (ch) {
		case 'c':
			chn.channelId = strtol(optarg, NULL, 10);
			break;
		case 'f':
			chn.fps = strtol(optarg, NULL, 10);
			break;
		case 'b':
			Change_bitrate = strtol(optarg, NULL, 10);
			break;
		case 'v':
			chn.vbr = strtol(optarg, NULL, 10);
			break;
		case 't':
			mul_channel = strtol(optarg, NULL, 10);
			break;
		case 'y':
			Catch_YUV = 1;
			if (sscanf(optarg, "%dx%d", &w, &h) != 2) {
				printf("Error  YUV resolution format\n");
				return -1;
			}
			break;
		case 'm':
			RM_Mode = strtol(optarg, NULL, 10);
			break;
		case 'q':
			qp = strtol(optarg, NULL, 10);
			break;
		case 'k':
			key_frame = 1;
			break;
		case 'i':
			Invert = 1;
			break;
		case 's':
			Snap = 1;
			chn.channelId = MAIN_STREAM;
			chn.res = QCAM_VIDEO_RES_720P;
			break;
		case 'l':
			Light = 1;
			break;
		case 'o':
			OSD = 1;
			break;
		case 'h':
			printf("Usage:\n");
			printf("\t-c set channelId: 0, 1, 2\n");
			printf("\t-f set fps:\n");
			printf("\t-v set bitrate_mode:(0:cbr, 1:vbr)\n");
			printf("\t-b set bitrate:\n");
			printf("\t-t  Turn on Mul stream test:1,2,3\n");
			printf("\t-i set Invert:\n");
			printf("\t-s Catch Mjpeg:\n");
			printf("\t-l Detect Light:\n");
			printf("\t-o Set OSD:\n");
			printf("\t-y Catch YUV:");
			printf("\tpara = wxh: (eg:320x240)\n");
			printf("\t-q SetQualityLvl:\t0--qp_low,\t1--qp_high\n");
			printf("\t-k set keyframe\n");
			printf("\t-m Set IR_MODE:");
			printf("\t-1--Not supported");
			printf("\t 0--Auto IR");
			printf("\t 1--Force open IR");
			printf("\t 2--close IR\n");
			/* fall through */
		default:
			return -1;
		}

	}

	switch (chn.channelId) {
	case 0:
		if (Snap) {
			chn.res = chn.res;
			chn.bitrate = Stream_bitrate[2];
		} else {
			chn.res = resolution[1];
			chn.bitrate = Stream_bitrate[1];
		}
		break;
	case 1:
		chn.res = resolution[2];
		chn.bitrate = Stream_bitrate[2];
		printf("chn.res=%d\n", chn.res);
		break;
	case 2:
		chn.res = resolution[3];
		chn.bitrate = Stream_bitrate[3];
		break;
	default:
		chn.res = resolution[1];
		chn.bitrate = Stream_bitrate[1];
	}

	signal(SIGTERM, sig_handle);
	signal(SIGINT, sig_handle);

	ret = QCamVideoInput_Init();
	if (ret) {
		printf("init CamVideoInput failed %d\n", ret);
		goto out;
	}

	if (mul_channel) {
		for (int i = 0; i < mul_channel; i++) {
			stream[i].channelId = i;
			stream[i].res = resolution[i+1];
			stream[i].bitrate = Stream_bitrate[i+1];
			stream[i].fps = 30;
			stream[i].gop = 1;
			stream[i].vbr = YSX_BITRATE_MODE_CBR;
			stream[i].cb = fun_cb[i];

			ret = QCamVideoInput_AddChannel(stream[i]);
			if (ret) {
				printf("current channelId = %d\n", i);
				printf("Mul_AddChannel failed %d\n", ret);
				goto out;
			}

			if (OSD) {
				system("date -s \"2018-11-1 23:59:55\"");
				ret = QCamVideoInput_SetOSD(i, &osd_info);
				printf("i = %d,ret = %d\n", i, ret);
				if (ret) {
					printf("QCamVideoInput_SetOSD failed %d\n", ret);
					goto out;
				}
			}
		}

		f_stream = fopen("out_0.h264", "w+");
		f_stream1 = fopen("out_1.h264", "w+");
		f_stream2 = fopen("out_2.h264", "w+");
		if (f_stream == NULL || f_stream1 == NULL
			|| f_stream2 == NULL) {
				printf("open file out.h264 fail\n");
				goto out;
		}

		ret = QCamVideoInput_Start();
		if (ret) {
			printf("Mul_Start failed %d\n", ret);
			goto out;
		}

		sleep(5);
		for (int i = 0; i <= 2; i++) {
			if (stream_file[i] != NULL) {
				fclose(stream_file[i]);
				stream_file[i] = NULL;
			}
		}

		goto out;
	}

	ret = QCamVideoInput_AddChannel(chn);
	if (ret) {
		printf("QCamVideoInput_AddChannel failed %d\n", ret);
		goto out;
	}

	f_stream = fopen("out.h264", "w+");
	if (f_stream == NULL) {
		printf("open file out.h264 fail\n");
		goto out;
	}

	ret = QCamVideoInput_Start();
	if (ret) {
		printf("QCamVideoInput_Start failed %d\n", ret);
		goto out;
	}

	if (OSD) {
		system("date -s \"2018-11-1 23:59:55\"");
		ret = QCamVideoInput_SetOSD(chn.channelId, &osd_info);
		if (ret) {
			printf("QCamVideoInput_SetOSD failed %d\n", ret);
			goto out;
		}
	}

	if (Change_bitrate && (chn.bitrate != Change_bitrate)) {
		if (!chn.vbr)
			printf("change the bitrate_mode to test.\n");

		ret = QCamVideoInput_SetBitrate(chn.channelId,
			Change_bitrate, chn.vbr);
		if (ret) {
			printf("QCamVideoInput_SetBitrate failed %d\n", ret);
			goto out;
		}
	}

	if (Catch_YUV) {
		ret = test(0, w, h);
		if (ret) {
			printf("test_YUV failed %d\n", ret);
			goto out;
		}
	}

	if (Invert) {
		ret = QCamVideoInput_SetInversion(Invert);
		if (ret) {
			printf("QCamVideoInput_SetInversion failed %d\n", ret);
			goto out;
		}
	}

	if (RM_Mode) {
		QCamSetIRMode(RM_Mode);
		QCAM_IRMODE = QCamGetIRMode();
		printf("%d\n", QCAM_IRMODE);
	}

	if (Light) {
		ret = QCamVideoInput_HasLight();
		if (ret) {
			printf("QCamVideoInput_HasLight failed %d\n", ret);
			goto out;
		}
	}

	if (qp >= 0) {
		ret = QCamVideoInput_SetQualityLvl(chn.channelId, qp);
		if (ret) {
			printf("QCamVideoInput_SetQualityLvl failed %d\n", ret);
			goto out;
		}
	}

	if (Snap) {
		ret = test(1, w, h);
		if (ret) {
			printf("test_mjpeg failed %d\n", ret);
			goto out;
		}
	}

	if (key_frame) {
		ret = QCamVideoInput_SetIFrame(chn.channelId);
		if (ret) {
			printf("QCamVideoInput_SetIFrame failed %d\n", ret);
			goto out;
		}
	}
out:

	sleep(10);
	ret = QCamVideoInput_Uninit();
	if (f_stream != NULL) {
		fclose(f_stream);
		f_stream = NULL;
	}

	__release_middleware_context();
	printf("preview retcode %d\n", ret);
	return ret;
}