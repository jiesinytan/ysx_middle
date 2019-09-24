/*!
 * Realtek Semiconductor Corp.
 *
 * example/example_h264_ctrl.c
 *
 * Copyright (C) 2016      Ming Qian<ming_qian@realsil.com.cn>
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/time.h>
#include <signal.h>
#include <string.h>
#include <rtscamkit.h>
#include <rtsavapi.h>
#include <rtsvideo.h>

static int g_exit;

static void Termination(int sign)
{
	g_exit = 1;
}

static int refresh_h264_ctrl(struct rts_video_h264_ctrl *ctrl)
{
	int ret;

	ret = rts_av_get_h264_ctrl(ctrl);
	if (ret) {
		RTS_ERR("set h264 ctrl fail, ret = %d\n", ret);
		return RTS_RETURN(RTS_E_GET_FAIL);
	}

	RTS_INFO("intra_qp_delta = %d, bitrate_mode = %d\n",
		ctrl->intra_qp_delta, ctrl->bitrate_mode);

	if (ctrl->hrd)
		return RTS_OK;

	ctrl->bitrate_mode = RTS_BITRATE_MODE_CBR;
	ctrl->intra_qp_delta = -3;

	ret = rts_av_set_h264_ctrl(ctrl);
	if (ret) {
		RTS_ERR("set h264 ctrl fail, ret = %d\n", ret);
		return RTS_RETURN(RTS_E_SET_FAIL);
	}

	/**
	 * get check whether the new value is set or not,
	 * no need in actual use
	 */
	ret = rts_av_get_h264_ctrl(ctrl);
	if (ret) {
		RTS_ERR("set h264 ctrl fail, ret = %d\n", ret);
		return RTS_RETURN(RTS_E_GET_FAIL);
	}

	RTS_INFO("after refresh intra_qp_delta = %d, bitrate_mode = %d\n",
		ctrl->intra_qp_delta, ctrl->bitrate_mode);

	return ret;
}

static int stream_reconfig(int width, int height, int framerate, int gop, int bitrate)
{
	int ret = 0;
	printf("%dx%d %dfps %dgop %dkbps\n", width, height, framerate, gop, bitrate);
	venc_stream_start(isp);
	while(!venc_stream_status())
		usleep(50 * 1000);
	rts_av_disable_chn(isp);
	rts_av_disable_chn(osd);
	rts_av_disable_chn(enc);

	ret = set_video_width_height_framerate(isp, width, height, framerate);
	if (ret) {
		printf("reconfig video_width_height_framerate failed.\n");
		goto exit;
	}

	ret = set_h264_video_bitrate_gop(h264, C_VBR, bitrate, gop);

}

int main(int argc, char *argv[])
{
	struct rts_av_profile profile;
	struct rts_isp_attr isp_attr;
	struct rts_h264_attr h264_attr;
	struct rts_video_h264_ctrl *ctrl = NULL;

	FILE *pfile = NULL;
	int isp = -1;
	int h264 = -1;
	int ret;
	int number = 0;


	rts_set_log_mask(RTS_LOG_MASK_CONS);

	signal(SIGINT, Termination);
	signal(SIGTERM, Termination);

	ret = rts_av_init();
	if (ret) {
		RTS_ERR("rts_av_init fail\n");
		return ret;
	}

	isp_attr.isp_id = 0;
	isp_attr.isp_buf_num = 2;
	isp = rts_av_create_isp_chn(&isp_attr);
	if (isp < 0) {
		RTS_ERR("fail to create isp chn, ret = %d\n", isp);
		ret = RTS_RETURN(RTS_E_OPEN_FAIL);
		goto exit;
	}

	RTS_INFO("isp chnno:%d\n", isp);

	profile.fmt = RTS_V_FMT_YUV420SEMIPLANAR;
	profile.video.width = 640;
	profile.video.height = 480;
	profile.video.numerator = 1;
	profile.video.denominator = 15;
	ret = rts_av_set_profile(isp, &profile);
	if (ret) {
		RTS_ERR("set isp profile fail, ret = %d\n", ret);
		goto exit;
	}

	h264_attr.level = H264_LEVEL_4;
	h264_attr.qp = -1;
	h264_attr.bps = 2 * 1024 * 1024;
	h264_attr.gop = 30;
	h264_attr.videostab = 0;
	h264_attr.rotation = RTS_AV_ROTATION_0;
	h264 = rts_av_create_h264_chn(&h264_attr);
	if (h264 < 0) {
		RTS_ERR("fail to create h264 chn, ret = %d\n", h264);
		ret = RTS_RETURN(RTS_E_OPEN_FAIL);
		goto exit;
	}
	RTS_INFO("h264 chnno:%d\n", h264);

	ret = rts_av_query_h264_ctrl(h264, &ctrl);
	if (ret) {
		RTS_ERR("query h264 ctrl fail, ret = %d\n", ret);
		return ret;
	}

	ctrl->bitrate_mode = RTS_BITRATE_MODE_C_VBR;
	ctrl->intra_qp_delta = 0;

	ret = rts_av_set_h264_ctrl(ctrl);
	if (ret) {
		RTS_ERR("set h264 ctrl fail, ret = %d\n", ret);
		goto exit;
	}

	ret = rts_av_bind(isp, h264);
	if (ret) {
		RTS_ERR("fail to bind isp and h264, ret %d\n", ret);
		goto exit;
	}

	ret = rts_av_enable_chn(isp);
	if (ret) {
		RTS_ERR("enable isp fail, ret = %d\n", ret);
		goto exit;
	}

	ret = rts_av_enable_chn(h264);
	if (ret) {
		RTS_ERR("enable h264 fail, ret = %d\n", ret);
		goto exit;
	}

	ret = rts_av_start_recv(h264);
	if (ret) {
		RTS_ERR("start recv h264 fail, ret = %d\n", ret);
		goto exit;
	}

	RTS_INFO("save to h264ctrl_out.h264\n");
	pfile = fopen("h264ctrl_out.h264", "wb");
	if (!pfile)
		RTS_ERR("open h264 file h264ctrl_out.h264 fail\n");

	while (!g_exit) {
		struct rts_av_buffer *buffer = NULL;

		usleep(1000);

		if (rts_av_poll(h264))
			continue;
		if (rts_av_recv(h264, &buffer))
			continue;

		if (buffer) {
			fwrite(buffer->vm_addr, 1,
				buffer->bytesused, pfile);
			number++;
			rts_av_put_buffer(buffer);
		}

		if (number == 60) {
			ret = refresh_h264_ctrl(ctrl);
			if (RTS_IS_ERR(ret))
				goto exit;
		}
	}

	RTS_INFO("get %d frames\n", number);

	rts_av_disable_chn(isp);
	rts_av_disable_chn(h264);

exit:
	RTS_SAFE_RELEASE(ctrl, rts_av_release_h264_ctrl);

	if (isp >= 0) {
		rts_av_destroy_chn(isp);
		isp = -1;
	}
	if (h264 >= 0) {
		rts_av_destroy_chn(h264);
		h264 = -1;
	}

	rts_av_release();

	return ret;
}
