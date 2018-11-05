//#include <platform_av.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdint.h>
#include <pthread.h>
#include <errno.h>
//#include <debug_log.h>
#include <rtscamkit.h>
//#include <rts_middle_media.h>
#include "../rts_middle_media.h"
#include <qcam_video_input.h>
QCamVideoInputChannel chn ;

int g_exit;

void sig_handle(int sig)
{
	g_exit = 1;
}

FILE *f_stream = NULL;


void h264_cb(const struct timeval *tv, const void *data,
	const int len, const int keyframe)
{
	static uint64_t count;
	int ret = 0;

	if (!f_stream) {
		printf("file not exist, no space to save stream\n");
	}
	ret = fwrite(data, sizeof(uint8_t), len, f_stream);
	if (ret != len)
		printf("lost data when save data, write[%d] get[%d]\n",
								len, ret);
	printf("get [%llu] frames\n", ++count);
}

int test(int btest)
{
	int ret = 0;
	static uint64_t index;
	char filename[64];
	FILE *pfile = NULL;
	char *buf;
	int buflen;

	if(btest){
		buf = (char *)rts_malloc(100 * 1024); /* JPEG picture max size */
		if (buf == NULL) {
			printf("malloc yuv buffer fail\n");
			return -1;
		}

		buflen = 100 * 1024;

		ret = QCamVideoInput_CatchJpeg(buf, &buflen);
		snprintf(filename, sizeof(filename), "%lld.jpg", index++);

		pfile = fopen(filename, "wb");
		if (!pfile) {
			printf("open file %s fail\n", filename);
		}
		ret = fwrite(buf, 1, buflen, pfile);
		printf("write [%d] bytes, want [%d] bytes\n", ret, buflen);
		fclose(pfile);

		RTS_SAFE_DELETE(buf);
		return 0;
	}
	else{
		/* TODO fw support larger resolution */
		buflen = 320 * 240 + 320 * 240 / 2;
		buf = (char *)rts_malloc(buflen);
		if (buf == NULL) {
			printf("malloc yuv buffer fail\n");
			return -1;
		}

		ret = QCamVideoInput_CatchYUV(320, 240, buf, buflen);
		snprintf(filename, sizeof(filename), "%lld.yuv", index++);

		pfile = fopen(filename, "w+");
		if (!pfile) {
			printf("open file %s fail\n", filename);
		}
		ret = fwrite(buf, 1, buflen, pfile);
		printf("write [%d] bytes, want [%d] bytes\n", ret, buflen);
		fclose(pfile);

		RTS_SAFE_DELETE(buf);
		return 0;
	}
}

int __get_stream(int id, int count, FILE *pf)
{
	int ret = 0;
	GW_VENC_STREAM_S vframe;
	char filename[64];

	memset(&vframe, 0, sizeof(vframe));
	while (count-- && !g_exit) {
		ret = PlatformVEncGetVideoStream(id, &vframe);
		if (ret)
			return ret;


		if (enpic) {
			sprintf(filename, "cap_%d.jpg", count);
			PlatformCaptureJPEG(filename, 0, &Prm.osd);
		}

		if (!enmd)
			printf("Getstream %p seq %lu packcount %lu",
					vframe.pstPack,
					vframe.u32Seq,
					vframe.u32PackCount);

		for (int i = 0; i < vframe.u32PackCount; i++) {
			GW_VENC_PACK_S *pc = vframe.pstPack + i;

			if (!enmd)
				printf("\tpack_t %d len %lu",
					pc->DataType.enH264EType,
					pc->u32Len);

			fwrite(pc->pu8Addr,
				1,
				pc->u32Len, pf);
		}

		PlatformVEncReleaseStream(id, &vframe);
		if (!enmd)
			printf("\n");
	}



	return ret;
}

int main(int argc, char *argv[])
{
	int ret = 0;
	int qp = -1;
	int Invert, Snop, Catch_YUV, RM_Mode, OSD, Light, Change_bitrate;
	int QCAM_IRMODE;
	int ch;
	QCamVideoInputOSD osd_info;

	Invert = Snop = Catch_YUV = RM_Mode = OSD = Light = Change_bitrate = 0;

	memset(&chn, 0, sizeof(QCamVideoInputChannel));
	//default QCamVideoInputChannel config
	chn.channelId = 0;
	chn.res = QCAM_VIDEO_RES_1080P;		// resolution
	chn.fps = 15;					// fps
	chn.bitrate = 2*1024;				// h264 bitrate (kbps)
	chn.gop = 30;					//h264 gop size
	chn.vbr = 0;					// VBR=1, CBR=0
	chn.cb = h264_cb;
	while ((ch = getopt(argc, argv, "r:c:f:b:v:isloqyhm:")) != -1) {
		switch (ch) {
		case 'r':
			chn.res = strtol(optarg, NULL, 10);
			break;
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
		case 'i':
			Invert = 1;
			break;
		case 's':
			Snop = 1;
			chn.res = QCAM_VIDEO_RES_720P;
			break;
		case 'l':
			Light = 1;
			break;
		case 'o':
			OSD = 1;
			break;
		case 'm':
			RM_Mode = strtol(optarg, NULL, 10);
			break;
		case 'y':
			Catch_YUV = 1;
			break;
		case 'q':
			qp = strtol(optarg, NULL, 10);
			break;
		case 'h':
			printf("Usage:\n");
			printf("\t-r set resolution:\n");
			printf("\t para = 0--invalid");
			printf("\t para = 1--720p");
			printf("\t para = 2--480p");
			printf("\t para = 3--360p");
			printf("\t para = 4--1080p\n");
			printf("\t-c set channelId\n");
			printf("\t-f set fps:\n");
			printf("\t-v set bitrate_mode:(0:cbr, 1:vbr)\n");
			printf("\t-b set bitrate:\n");
			printf("\t-i set Invert:\n");
			printf("\t-s Catch Mjpeg:\n");
			printf("\t-l Detect Light:\n");
			printf("\t-o Set OSD:\n");
			printf("\t-y Catch YUV:\n");
			printf("\t-q SetQualityLvl:\n");
			printf("\t-m Set IR_MODE:\n");
			printf("\t -1--Not supported");
			printf("\t 0--Auto IR");
			printf("\t 1--Force open IR");
			printf("\t 2--close IR\n");
			printf("\t-p enable capture picture\n");
			/* fall through */
		default:
			return -1;
		}

	}

	signal(SIGTERM, sig_handle);
	signal(SIGINT, sig_handle);

	ret = QCamVideoInput_Init();
	if(ret){
		printf("init CamVideoInput failed %d\n",ret);
		goto out;
	}

	ret = QCamVideoInput_AddChannel(chn);
	if(ret){
		printf("QCamVideoInput_AddChannel failed %d\n",ret);
		goto out;
	}
	f_stream = fopen("out.h264", "w+");
	if (f_stream == NULL) {
		printf("open file out.h264 fail\n");
		goto out;
	}

	ret = QCamVideoInput_Start();
	if(ret){
		printf("QCamVideoInput_Start failed %d\n",ret);
		goto out;
	}

	if(OSD){

		system("date -s \"2018-11-1 23:59:55\"");
		osd_info.pic_enable = 1;
		strcpy(osd_info.pic_path, "/usr/osd_char_lib/argb_2222");
		osd_info.pic_x = 500;
		osd_info.pic_y = 500;
		osd_info.time_enable = 1;
		osd_info.time_x = 350;
		osd_info.time_y  = 300;

		ret = QCamVideoInput_SetOSD(chn.channelId, &osd_info);
		if(ret){
			printf("QCamVideoInput_SetOSD failed %d\n",ret);
			goto out;
		}
	}

	if(Change_bitrate && (chn.bitrate != Change_bitrate)){
		if(!chn.vbr){
			printf("change the bitrate_mode to vbr to test.\n");
		}
		ret = QCamVideoInput_SetBitrate(chn.channelId, Change_bitrate, chn.vbr);
		if(ret){
			printf("QCamVideoInput_SetBitrate failed %d\n",ret);
			goto out;
		}
	}

	if(Catch_YUV){
		ret = test(0);
		if(ret){
			printf("test_YUV failed %d\n",ret);
			goto out;
		}
	}

	if(Invert){
		ret = QCamVideoInput_SetInversion(Invert);
		if(ret){
			printf("QCamVideoInput_SetInversion failed %d\n",ret);
			goto out;
		}
	}

	if(RM_Mode){
		QCamSetIRMode(RM_Mode);
		QCAM_IRMODE = QCamGetIRMode();
		printf("%d\n", QCAM_IRMODE);
	}

	if(Light){
		ret = QCamVideoInput_HasLight();
		if(ret){
			printf("QCamVideoInput_HasLight failed %d\n",ret);
			goto out;
		}
	}

	if(qp >= 0){
		ret = QCamVideoInput_SetQualityLvl(chn.channelId, qp);
		if(ret){
			printf("QCamVideoInput_SetQualityLvl failed %d\n",ret);
			goto out;
		}
	}

	if(Snop){
		ret = test(1);
		if(ret){
			printf("test_mjpeg failed %d\n",ret);
			goto out;
		}
	}
out:

	sleep(20);
	ret = QCamVideoInput_Uninit();
	if (f_stream != NULL) {
		fclose(f_stream);
		f_stream = NULL;
	}
	printf("preview retcode %d\n", ret);
	return ret;

}