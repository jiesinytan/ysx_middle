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
	.res = QCAM_VIDEO_RES_720P,	/*resolution*/
	.fps = 15,				/*fps*/
	.bitrate = 1024,				/*h264 bitrate (kbps)*/
	.gop = 1,				/*interval I frame(s)*/
	.vbr = YSX_BITRATE_MODE_CBR,	/*VBR=1, CBR=0*/
	.cb = NULL
};

QCamVideoInputOSD osd_default = {
	.pic_enable = 0,
	.pic_path = "/usr/osd_char_lib/argb_2222",
	.pic_x = 200,
	.pic_y = 200,
	.time_enable = 1,
	.time_x = 100,
	.time_y  = 100
};

int resolution[] = {
	QCAM_VIDEO_RES_INVALID,
	QCAM_VIDEO_RES_720P,
	QCAM_VIDEO_RES_480P,
	QCAM_VIDEO_RES_360P
};

int Stream_bitrate[] = {2048, 1024, 768, 512};
int key_frame;
int g_exit;
FILE *f_stream;
FILE *f_stream1;

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
		printf("Stream0 keyframe = %d\n", keyframe);

	if (!f_stream)
		printf("file not exist, no space to save stream\n");

	ret = fwrite(data, sizeof(uint8_t), len, f_stream);
	if (ret != len)
		printf("lost data when save data, write[%d] get[%d]\n",
			len, ret);
	count++;
	if (count % 20 == 0)
		printf("Stream0 get [%llu] frames\n", count);
}

void h264_cb1(const struct timeval *tv, const void *data,
	const int len, const int keyframe)
{
	static uint64_t count;
	int ret = 0;

	if (key_frame && keyframe)
		printf("Stream1 keyframe = %d\n", keyframe);

	if (!f_stream1)
		printf("file not exist, no space to save stream\n");

	ret = fwrite(data, sizeof(uint8_t), len, f_stream1);
	if (ret != len)
		printf("lost data when save data, write[%d] get[%d]\n",
			len, ret);
	count++;
	if (count % 20 == 0)
	printf("Stream1 get [%llu] frames\n", count);
}

int test(int btest, int w, int h)
{
	int ret = 0;
	int m_x;
	static uint64_t index;
	char filename[64];
	FILE *pfile = NULL;
	char *buf;
	int buflen;

	if (btest) {
		for (m_x = 0; m_x < 10; m_x++) {
			buflen = 100 * 1024;
			buf = (char *)calloc(buflen, sizeof(char)); /* JPEG picture max size */

			if (buf == NULL) {
				printf("malloc yuv buffer fail\n");
				return -1;
			}

			ret = QCamVideoInput_CatchJpeg(buf, &buflen);
			snprintf(filename, sizeof(filename), "%lld.jpg", index++);

			pfile = fopen(filename, "wb");
			if (!pfile) {
				printf("open file %s fail\n", filename);
			}
			ret = fwrite(buf, 1, buflen, pfile);
			printf("Mjpeg write [%d] bytes, want [%d] bytes\n", ret, buflen);
			fclose(pfile);

			free(buf);
			buf = NULL;
			sleep (2);

		}
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
		printf("Catch_YUV write [%d] bytes, want [%d] bytes\n", ret, buflen);
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
	int mul_count = 0;
	QCamVideoInputOSD osd_info;
	Invert = Snap = Catch_YUV = RM_Mode = OSD = Light = Change_bitrate = 0;
	QCam_Video_Input_cb fun_cb[] = {h264_cb, h264_cb1};
	FILE *stream_file[2] = {f_stream, f_stream1};

	memset(&chn, 0, sizeof(QCamVideoInputChannel));
	memset(&osd_info, 0, sizeof(QCamVideoInputOSD));
	chn = chn_default;
	chn.cb = h264_cb;
	osd_info = osd_default;

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
			printf("\t-t Turn on Mul stream test:1,2\n");
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
	printf("show the chn %d\n",chn.channelId);

	switch (chn.channelId) {
	case 0:
		chn.res = resolution[1];
		chn.bitrate = Stream_bitrate[1];
		break;
	case 1:
		chn.res = resolution[2];
		chn.bitrate = Stream_bitrate[2];
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

	QCamAV_Context_Init();
	system("date -s \"2018-11-1 23:59:55\"");

	ret = QCamVideoInput_Init();
	if (ret) {
		printf("init CamVideoInput failed %d\n", ret);
		goto out;
	}

	if (mul_channel) {

		f_stream = fopen("out_0.h264", "w+");
		f_stream1 = fopen("out_1.h264", "w+");
		if (f_stream == NULL || f_stream1 == NULL) {
				printf("open file out.h264 fail\n");
				goto out;
		}

		for (mul_count = 0; mul_count < mul_channel; mul_count++) {
			stream[mul_count].channelId = mul_count;
			stream[mul_count].res = resolution[mul_count+1];
			stream[mul_count].bitrate = Stream_bitrate[mul_count+1];
			stream[mul_count].fps = 15;
			stream[mul_count].gop = 1;
			stream[mul_count].vbr = YSX_BITRATE_MODE_CBR;
			stream[mul_count].cb = fun_cb[mul_count];

			ret = QCamVideoInput_AddChannel(stream[mul_count]);
			printf("Id = %d, res = %d\n", mul_count, stream[mul_count].res);
			if (ret) {
				printf("current channelId = %d\n", mul_count);
				printf("Mul_AddChannel failed %d\n", ret);
				goto out;
			}
		}

		ret = QCamVideoInput_Start();
		if (ret) {
			printf("Mul_Start failed %d\n", ret);
			goto out;
		}

		if (mul_count == 1) {
			ret = QCamVideoInput_SetOSD(0, &osd_info);
			printf("id = %d\n", 0);
			if (ret) {
				printf("QCamVideoInput_SetOSD failed %d\n", ret);
				goto out;
			}
		} else if (mul_count == 2) {

			ret = QCamVideoInput_SetOSD(0, &osd_info);
			printf("Set Channel[0] OSD.\n");
			if (ret) {
				printf("QCamVideoInput_SetOSD failed %d\n", ret);
				goto out;
			}

			usleep(5000);
			ret = QCamVideoInput_SetOSD(1, &osd_info);
			printf("Set Channel[1] OSD.\n");
			if (ret) {
				printf("QCamVideoInput_SetOSD failed %d\n", ret);
				goto out;
			}
		}

		sleep(5);
		goto mul_exit;
	}

	ret = QCamVideoInput_AddChannel(chn);
	printf("add chn %d\n",chn.channelId);
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

	ret = QCamVideoInput_SetOSD(chn.channelId, &osd_info);
	if (ret) {
		printf("QCamVideoInput_SetOSD failed %d\n", ret);
		goto out;
	}

	if (Change_bitrate && (chn.bitrate != Change_bitrate)) {
		if (!chn.vbr)
			printf("change the bitrate_mode to test.\n");

		ret = QCamVideoInput_SetBitrate(chn.channelId, Change_bitrate, chn.vbr);
		if (ret) {
			printf("QCamVideoInput_SetBitrate failed %d\n", ret);
			goto out;
		}
	}
mul_exit:

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
		printf("Detect the input Light.\n");
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
		int w1 = 640;
		int h1 = 360;

		ret= QCamJpeg_Init(w1,h1);
		if (ret < 0) {
			printf("Jpeg init failed\n");
			goto out;
		}

		ret = test(1, w1, h1);
		if (ret) {
			printf("test_mjpeg failed %d\n", ret);
			goto out;
		}

		ret= QCamJpeg_Uinit();
		if (ret < 0) {
			printf("Jpeg uninit failed\n");
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
	for (int i = 0; i <= 1; i++) {
		if (stream_file[i] != NULL) {
			fclose(stream_file[i]);
			stream_file[i] = NULL;
		}
	}

	QCamAV_Context_Release();
	printf("preview retcode %d\n", ret);
	return ret;

}
