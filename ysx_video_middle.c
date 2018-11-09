#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <pthread.h>
#include <rtscamkit.h>
#include <rtsavapi.h>
#include <rtsvideo.h>
#include <rtsisp.h>
#include <rtsamixer.h>
#include <rtsaudio.h>
#include <rts_pthreadpool.h>
#include <rts_io_adc.h>
#include <rts_io_gpio.h>

#include <qcam_video_input.h>

#include "rts_middle_media.h"


/* static void __attribute__((constructor(AV_INIT_NO))) */
void __init_middleware_context(void)
{
	rts_set_log_ident("MIDDLEWARE");
	rts_set_log_mask(RTS_LOG_MASK_CONS);

	rts_av_init();
	__init_sys_daemon();
	__init_audio_server();
	RTS_INFO("RTS AV INIT\n");
}

/* static void __attribute__((destructor(AV_INIT_NO))) */
void __release_middleware_context(void)
{
	__release_sys_daemon();
	__release_audio_server();
	rts_av_release();
	RTS_INFO("RTS AV RELEASE\n");
}

static PthreadPool tpool;
static int cur_stream_count;
static struct rts_middle_stream vstream[STREAM_COUNT];
static struct rts_m_ir ir;
static struct rts_m_osd2_common osd2_com;

/*
static struct rts_middle_stream *__find_stm_by_id(int id)
{
	int i;
	struct rts_middle_stream *stm = NULL;

	for (i = 0; i < STREAM_COUNT; i++) {
		if (vstream[i].id == id) {
			stm = &vstream[i];
			break;
		}
	}

	return stm;
}
*/

static int __ld_time_pict(uint8_t *pbuf, uint16_t len, int index)
{
	FILE *pfile = NULL;
	char file_name[64] = {'\0'};

	sprintf(file_name, "%s/%d", OSD_TM_CHAR_LIB_FILE_PREFIX, index);

	pfile = fopen(file_name, "rb");
	if (pfile == NULL)
		goto exit;

	if (fread(pbuf, 1, len, pfile) != len)
		goto exit;

	RTS_SAFE_RELEASE(pfile, fclose);

	return 0;

exit:
	RTS_SAFE_RELEASE(pfile, fclose);
	RTS_ERR("Load osd time character lib fail, file[%s]\n", file_name);
	return -1;
}

static int __ld_osd_time_charlib()
{
	char patt[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':'};
	int i;
	int ret = 0;

	osd2_com.tm_img_patt = (uint8_t *)
		rts_calloc(sizeof(patt), TM_PICT_SIZE);
	if (osd2_com.tm_img_patt == NULL)
		goto exit;

	osd2_com.tm_img_2222 = (uint8_t *)
		rts_calloc(sizeof(patt),
				TM_ELEM_NUM * TM_PICT_SIZE);
	if (osd2_com.tm_img_2222 == NULL)
		goto exit;

	for (i = 0; i < sizeof(patt); i++) {
		ret = __ld_time_pict((osd2_com.tm_img_patt + TM_PICT_SIZE * i),
							TM_PICT_SIZE, i);
		if (ret < 0)
			goto exit;
	}

	return 0;

exit:
	if (osd2_com.tm_img_patt != NULL)
		RTS_SAFE_DELETE(osd2_com.tm_img_patt);
	if (osd2_com.tm_img_2222 != NULL)
		RTS_SAFE_DELETE(osd2_com.tm_img_2222);

	RTS_ERR("Load osd time character lib fail\n");
	return -1;
}

static int __set_osd_attr(struct rts_video_osd2_attr *osd_attr, int blkidx)
{
	int ret = 0;

	ret = rts_av_set_osd2_single(osd_attr, blkidx);
	if (ret < 0) {
		RTS_ERR("Set osd attr fail, ret[%d]\n", ret);
		return -1;
	}

	return 0;
}

static void __set_osd2_stuf(int init)
{
	int i;
	pthread_mutex_t *mutex = NULL;

	/*TODO dose isp channel need osd?? */
	for (i = 0; i < STREAM_COUNT - 1; i++) {
		mutex = &vstream[i].osd.osd_info.tm.mutex;

		if (init)
			pthread_mutex_init(mutex, NULL);
		else
			pthread_mutex_destroy(mutex);

		vstream[i].osd.osd_info.tm.enable = 0;
		vstream[i].osd.osd_info.pict.enable = 0;
	}
}

static void __init_osd2_stuf()
{
	__set_osd2_stuf(1);
}

static void __deinit_osd2_stuf()
{
	__set_osd2_stuf(0);
}

static void __get_time(char *now_time, char *now_date, struct tm *tm)
{
	time_t now;

	now = time(NULL);
	localtime_r(&now, tm);

	sprintf(now_time, "%02d:%02d:%02d",
		tm->tm_hour, tm->tm_min, tm->tm_sec);

	sprintf(now_date, "%04d:%02d:%02d",
		tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
}



static int __get_pict_from_pattern(struct text_info *txt)
{
	int i;
	int val;
	char *text = txt->text;
	int len = strlen(text);

	for (i = 0; i < len; i++) {
		if (text[i] == ':')
			val = 10;
		else
			val = (int)(text[i] - '0');

		int p;
		int q;
		uint8_t *src = osd2_com.tm_img_patt + val * TM_PICT_SIZE;

		/* splicing: row scanning */
		for (p = 0; p < TM_PICT_HEIGHT; p++) {
			for (q = 0; q < TM_PICT_WIDTH; q++)
				osd2_com.tm_img_2222[p * TM_PICT_WIDTH
					* txt->cnt + i * TM_PICT_WIDTH + q]
					= src[p * TM_PICT_WIDTH + q];
		}
	}

	txt->pdata = osd2_com.tm_img_2222;
	txt->len = TM_PICT_SIZE * txt->cnt;

	return 0;
}

static int __set_osd2_timedate(struct rts_video_osd2_attr *attr,
					struct text_info *text, int blkidx)
{
	int ret;
	struct rts_video_osd2_block *block;

	ret = __get_pict_from_pattern(text);
	if (ret < 0) {
		RTS_ERR("Get time/date block picture fail\n");
		return ret;
	}

	block = &attr->blocks[blkidx];

	block->picture.length = text->len;
	block->picture.pdata = text->pdata;
	block->picture.pixel_fmt = TM_PICT_FORMAT;

	block->rect.left = text->x;
	block->rect.top = text->y;
	block->rect.right = text->x + TM_PICT_WIDTH * text->cnt;
	block->rect.bottom = text->y + TM_PICT_HEIGHT;

	block->enable = RTS_TRUE;

	ret = rts_av_set_osd2_single(attr, blkidx);
	if (ret < 0)
		RTS_ERR("Set osd attr fail, ret[%d]\n", ret);

	return ret;
}

static void __flush_osd_time()
{
	int need_update = 0;
	char now_time[9] = "00:00:00";
	char now_date[11] = "2018:01:01";
	struct tm tm;
	struct text_info text_tm;
	struct text_info text_date;
	struct rts_video_osd2_attr *osd_attr;
	int i;
	int first_run = 1;
	int ret = 0;

	/*TODO dose isp channel need osd?? */
	for (i = 0; i < (STREAM_COUNT - 1); i++)
		need_update += vstream[i].osd.osd_info.tm.enable;

	if (!need_update)
		return;

	sleep(1);
	/* need update date for the 1st time
	 * caution when update date(at 00:00:00, there's maybe second error)
	 * (00:00:00~00:00:05 update date)
	 */

	__get_time(now_time, now_date, &tm);

	/*TODO dose isp channel need osd?? */
	for (i = 0; i < (STREAM_COUNT - 1); i++) {
		pthread_mutex_lock(&vstream[i].osd.osd_info.tm.mutex);

		if (vstream[i].osd.osd_info.tm.enable) {
			osd_attr = vstream[i].osd_attr;


			/* flush osd time */
			text_tm.text = now_time;
			text_tm.cnt = strlen(now_time);
			text_tm.x = vstream[i].osd.osd_info.tm.x + 100;
			text_tm.y = vstream[i].osd.osd_info.tm.y;

			ret = __set_osd2_timedate(osd_attr, &text_tm,
							OSD_TM_TIME_BLKIDX);
			if (ret < 0)
				RTS_ERR("Flush stream[%d] osd time fail\n", i);


			/* flush osd date */
			if (first_run ||
					((tm.tm_hour == 0)
					 && (tm.tm_min == 0)
					 && (tm.tm_sec < 5))) {
				text_date.text = now_date;
				text_date.cnt = strlen(now_date);
				text_date.x = vstream[i].osd.osd_info.tm.x;
				text_date.y = vstream[i].osd.osd_info.tm.y;

				ret = __set_osd2_timedate(osd_attr, &text_date,
							OSD_TM_DATE_BLKIDX);
				if (ret < 0) {
					RTS_ERR("Flush stream[%d] osd", i);
					RTS_ERR("date fail\n");
				}
			}
		}

		pthread_mutex_unlock(&vstream[i].osd.osd_info.tm.mutex);
	}
}

void *__osd_supervisor(void *arg)
{
	osd2_com.run = 1;

	while (osd2_com.run)
		__flush_osd_time();

	return NULL;
}

static int __start_osd_supervisor(void)
{
	int ret = 0;

	osd2_com.osd_tid.stat = false;
	ret = pthread_create(&osd2_com.osd_tid.tid, NULL, __osd_supervisor, NULL);
	if (ret < 0)
		goto exit;
	osd2_com.osd_tid.stat = true;

	return 0;

exit:
	RTS_ERR("Start osd supervisor fail\n");
	return -1;
}

static void __stop_osd_supervisor(void)
{
	osd2_com.run = 0;

	if (osd2_com.osd_tid.stat == true) {
		pthread_join(osd2_com.osd_tid.tid, NULL);
		osd2_com.osd_tid.stat = false;
	}
	RTS_SAFE_DELETE(osd2_com.tm_img_patt);
	RTS_SAFE_DELETE(osd2_com.tm_img_2222);
	__deinit_osd2_stuf();
}

static int __osd_sanity_check(int channel, QCamVideoInputOSD *pOsdInfo)
{
	Mchannel *pchn = NULL;

	if ((channel < 0) || (channel >= (STREAM_COUNT - 1)))
		goto exit;

	pchn = &(vstream[channel].osd);

	if ((vstream[channel].stat != RTS_STM_STAT_OK)
			|| !pchn || (pchn->stat != RTS_CHN_STAT_OK)) {
		RTS_ERR("Stream is not ok, set osd fail\n");
		goto exit;
	}

	if (pOsdInfo->pic_enable == 1) {
		if ((access(pOsdInfo->pic_path, F_OK) < 0)
				|| (pOsdInfo->pic_x < 0)
				|| (pOsdInfo->pic_y < 0))
			goto exit;
	}

	if (pOsdInfo->time_enable == 1) {
		if ((pOsdInfo->time_x < 0)
				|| (pOsdInfo->time_y < 0))
			goto exit;
	}

	return 0;

exit:
	RTS_INFO("file[%s]\n", pOsdInfo->pic_path);
	RTS_ERR("[QCamVideoInput_SetOSD] sanity check fail\n");
	return -1;
}

static int __get_osd_pict_buf(char *f_pict, uint8_t **buf, int *len)
{
	FILE *pf = NULL;
	int ret = 0;

	*len = rts_get_file_size(f_pict);
	if (!*len)
		goto exit;

	/* NOTE: free after set osd attr on success return */
	*buf = (uint8_t *)rts_calloc(1, *len);
	if (*buf == NULL)
		goto exit;

	pf = fopen(f_pict, "rb");
	if (pf == NULL)
		goto exit;

	ret = fread(*buf, sizeof(uint8_t), *len, pf);
	if (ret != *len)
		goto exit;

	RTS_SAFE_RELEASE(pf, fclose);

	return 0;

exit:
	RTS_SAFE_DELETE(*buf);
	RTS_SAFE_RELEASE(pf, fclose);
	*buf = NULL;
	*len = 0;

	RTS_ERR("Get osd picture buffer from file fail\n");

	return -1;
}

static int __update_osd_pict(int channel, QCamVideoInputOSD *pOsdInfo)
{
	Mchannel *pchn = NULL;
	struct rts_video_osd2_attr *osd_attr;
	struct rts_video_osd2_block *block;
	int ret = 0;

	pchn = &(vstream[channel].osd);

	if (pOsdInfo->pic_enable == pchn->osd_info.pict.enable) {
		RTS_INFO("OSD picture status not changed\n");
		return 0;
	}

	osd_attr = vstream[channel].osd_attr;
	block = &vstream[channel].osd_attr->blocks[OSD_PICT_BLKIDX];

	if (pOsdInfo->pic_enable) {
		int len;
		uint8_t *buf = NULL;

		ret = __get_osd_pict_buf(pOsdInfo->pic_path, &buf, &len);
		if (ret < 0)
			goto exit;

		block->picture.length = len;
		block->picture.pdata = buf;
		block->picture.pixel_fmt = OSD_PICT_FORMAT;

		block->rect.left = pOsdInfo->pic_x;
		block->rect.top = pOsdInfo->pic_y;
		block->rect.right = pOsdInfo->pic_x + OSD_PICT_WIDTH;
		block->rect.bottom = pOsdInfo->pic_y + OSD_PICT_HEIGHT;

		block->enable = RTS_TRUE;

		ret = __set_osd_attr(osd_attr, OSD_PICT_BLKIDX);
		RTS_SAFE_DELETE(buf);
		if (ret < 0)
			goto exit;
	} else {
		block->enable = RTS_FALSE;

		ret = __set_osd_attr(osd_attr, OSD_PICT_BLKIDX);
		if (ret < 0)
			goto exit;
	}

	return 0;

exit:
	return -1;
}

static int __update_osd_time(int channel, QCamVideoInputOSD *pOsdInfo)
{
	Mchannel *pchn = NULL;
	struct rts_video_osd2_attr *osd_attr;
	struct rts_video_osd2_block *block;
	int ret = 0;

	pchn = &(vstream[channel].osd);

	if (pOsdInfo->time_enable == pchn->osd_info.tm.enable) {
		RTS_INFO("OSD time status not changed\n");
		return 0;
	}

	osd_attr = vstream[channel].osd_attr;

	if (pOsdInfo->time_enable) {
		/*osd time flush by thread "osd_supervisor"*/
		pthread_mutex_lock(&pchn->osd_info.tm.mutex);

		pchn->osd_info.tm.enable = 1;
		pchn->osd_info.tm.x = pOsdInfo->time_x;
		pchn->osd_info.tm.y = pOsdInfo->time_y;

		pthread_mutex_unlock(&pchn->osd_info.tm.mutex);
	} else {
		pthread_mutex_lock(&pchn->osd_info.tm.mutex);

		block = &vstream[channel].osd_attr->blocks[OSD_TM_TIME_BLKIDX];
		block->enable = RTS_FALSE;
		ret |= __set_osd_attr(osd_attr, OSD_TM_TIME_BLKIDX);

		block = &vstream[channel].osd_attr->blocks[OSD_TM_DATE_BLKIDX];
		block->enable = RTS_FALSE;
		ret |= __set_osd_attr(osd_attr, OSD_TM_DATE_BLKIDX);

		if (ret < 0)
			RTS_ERR("Close osd time fail\n");
		else
			pchn->osd_info.tm.enable = 0;

		pthread_mutex_unlock(&pchn->osd_info.tm.mutex);
	}

	return ret;
}

static int __set_isp_ctrl(int id, int value)
{
	struct rts_video_control v_ctrl;
	int ret = 0;

	ret = rts_av_get_isp_ctrl(id, &v_ctrl);
	if (ret < 0)
		goto exit;

	v_ctrl.current_value = value;
	ret = rts_av_set_isp_ctrl(id, &v_ctrl);

exit:
	if (ret < 0)
		RTS_ERR("Set isp ctrl fail, id[%d], ret[%d]\n", id, ret);
	return ret;
}

static int __register_ir_gpio(void)
{
	ir.gpio_ir_cut = NULL;
	ir.gpio_ir_led = NULL;

	ir.gpio_ir_cut = rts_io_gpio_request(SYSTEM_GPIO, GPIO_IR_CUT);
	if (ir.gpio_ir_cut == NULL)
		goto exit;

	rts_io_gpio_set_direction(ir.gpio_ir_cut, GPIO_OUTPUT);

	ir.gpio_ir_led = rts_io_gpio_request(SYSTEM_GPIO, GPIO_IR_LED);
	if (ir.gpio_ir_led == NULL)
		goto exit;

	rts_io_gpio_set_direction(ir.gpio_ir_led, GPIO_OUTPUT);

	return 0;

exit:
	RTS_ERR("Register IR gpio fail\n");
	return -1;
}

static void free_ir_gpio(void)
{
	if (ir.gpio_ir_cut != NULL)
		rts_io_gpio_free(ir.gpio_ir_cut);
	if (ir.gpio_ir_led != NULL)
		rts_io_gpio_free(ir.gpio_ir_led);
}

/* @return digital averaging filtering value, -1 on error
 * @you should call this api for ADC_FACTOR times
 * to avoid getting the outdated value
 * */
static int __get_adc_smooth_value(int chn)
{
	int value;
	int value_avg = 0;
	static int index, count;
	static int adc_data[ADC_FACTOR];
	int i;

	value = rts_io_adc_get_value(ADC_CHN);
	if (value < 0) {
		RTS_ERR("Warning get ADC value fail\n");
		return -1;
	}

	adc_data[index++] = value;
	if (count < sizeof(adc_data)/sizeof(adc_data[0]))
		count++;
	if (index >= sizeof(adc_data)/sizeof(adc_data[0]))
		index = 0;
	for (i = 0; i < count; i++)
		value_avg += adc_data[i];
	value_avg /= count;
	return value_avg;
}

static int __switch_ir_gpio(enum rts_m_ir_stat mode)
{
	int ret = 0;

	if (mode == DAY) {
		ret |= rts_io_gpio_set_value(ir.gpio_ir_led, GPIO_IR_LED_DAY);
		ret |= rts_io_gpio_set_value(ir.gpio_ir_cut, GPIO_IR_CUT_DAY);
	} else {
		ret |= rts_io_gpio_set_value(ir.gpio_ir_led, GPIO_IR_LED_NIGHT);
		ret |= rts_io_gpio_set_value(ir.gpio_ir_cut, GPIO_IR_CUT_NIGHT);
	}

	if (ret < 0)
		RTS_ERR("Switch IR gpio fail\n");
	return ret;
}

static void __switch_daynight_mode(enum rts_m_ir_stat mode)
{
	int ret = 0;

	if (ir.ir_stat == mode)
		return;

	ret |= __set_isp_ctrl(RTS_VIDEO_CTRL_ID_IR_MODE, !mode);
	ret |= __set_isp_ctrl(RTS_VIDEO_CTRL_ID_GRAY_MODE, !mode);
	ret |= __switch_ir_gpio(mode);

	if (ret < 0) {
		RTS_ERR("Switch IR mode fail\n");
		return;
	}

	ir.ir_stat = mode;
	RTS_INFO("Switch to [%s] mode\n", mode ? "day" : "night");
}

void *__auto_ir_thread(void *arg)
{
	int adc_value = 0;
	int i;

	RTS_INFO("Auto IR working\n");

	while (!ir.auto_ir_exit) {
		pthread_mutex_lock(&ir.mutex);
		ir.auto_ir_stat = AUTO_IR_RUN;

		for (i = 0; i < ADC_FACTOR; i++) {
			adc_value = __get_adc_smooth_value(ADC_CHN);
		}

		if (adc_value >= ADC_DAY_THR)
			__switch_daynight_mode(DAY);
		else if (adc_value <= ADC_NIGHT_THR)
			__switch_daynight_mode(NIGHT);

		pthread_mutex_unlock(&ir.mutex);
	}

	return NULL;
}

static void __stop_auto_ir(void)
{
	ir.auto_ir_exit = 1;
}

static void __pause_auto_ir(void)
{
	if (ir.auto_ir_stat == AUTO_IR_RUN) {
		pthread_mutex_lock(&ir.mutex);
		ir.auto_ir_stat = AUTO_IR_STOP;
	}
}

static void __restart_auto_ir(void)
{
	if (ir.auto_ir_stat == AUTO_IR_STOP)
		pthread_mutex_unlock(&ir.mutex);
}

static int __start_auto_ir(void)
{
	int ret = 0;

	memset(&ir, 0, sizeof(ir));
	ir.ir_stat = UNKNOWN; /* TODO what is the start stat */
	ir.ir_mode = QCAM_IR_MODE_AUTO;
	ir.ir_tid.stat = false;
	ir.gpio_ir_cut = NULL;
	ir.gpio_ir_led = NULL;
	ir.auto_ir_stat = AUTO_IR_STOP;


	ret = __register_ir_gpio();
	if (ret < 0)
		goto exit;

	pthread_mutex_init(&ir.mutex, NULL);

	ret = pthread_create(&ir.ir_tid.tid, NULL, __auto_ir_thread, NULL);
	if (ret < 0)
		goto exit;
	ir.ir_tid.stat = true;

	return 0;

exit:
	RTS_ERR("Start auto IR fail\n");
	return -1;
}

static void __release_auto_ir(void)
{
	QCamSetIRMode(QCAM_IR_MODE_AUTO);
	__stop_auto_ir();
	if (ir.ir_tid.stat == true) {
		pthread_join(ir.ir_tid.tid, NULL);
		ir.ir_tid.stat = false;
	}
	pthread_mutex_destroy(&ir.mutex);

	free_ir_gpio();
}

void __init_chn_info(Mchannel *pchn, int type)
{
	memset(pchn , 0, sizeof(*pchn));

	pchn->id = -1;
	pchn->type = type;
	pchn->stat = RTS_CHN_STAT_UNINIT;
	pchn->err = RTS_CHN_E_UNINIT;
}

static void __set_video_stream_info(int init)
{
	int i;

	for (i = 0; i < STREAM_COUNT; i++) {
		__init_chn_info(&vstream[i].isp, RTS_MIDDLE_VIDEO_CHN);
		__init_chn_info(&vstream[i].osd, RTS_MIDDLE_VIDEO_CHN);
		__init_chn_info(&vstream[i].h264, RTS_MIDDLE_VIDEO_CHN);
		__init_chn_info(&vstream[i].jpg, RTS_MIDDLE_VIDEO_CHN);

		vstream[i].id = i;
		vstream[i].stat = RTS_STM_STAT_UNINIT;
		vstream[i].h264_ctrl = NULL;
		vstream[i].exit = 0;
		if (init)
			pthread_mutex_init(&vstream[i].mutex, NULL);
		else
			pthread_mutex_destroy(&vstream[i].mutex);
	}
}

static void __init_video_stream_info(void)
{
	__set_video_stream_info(1);
}

static void __deinit_video_stream_info(void)
{
	__set_video_stream_info(0);
}

#ifdef CONFIG_REAL_TIME_STAMP
static int __switch_time_stamp(void)
{
	int fd = -1;
	int ret = 0;

	fd = open("/sys/module/rts_cam/parameters/clock", O_WRONLY | O_TRUNC);
	if (fd < 0)
		return -1;

	ret = write(fd, "realtime", 8);
	if (ret != 8)
		ret = -1;

	close(fd);
	fd = -1;

	return ret;
}
#endif

int QCamVideoInput_Init()
{
	int ret = 0;

#ifdef CONFIG_REAL_TIME_STAMP
	/*TODO monotonic time is recommend*/
	ret = __switch_time_stamp();
	if (ret < 0) {
		RTS_ERR("Switch time stamp type to realtime fail\n");
		return -1;
	}
#endif

	/* TODO what if hw photosensitivity is not support,
	 * use sensor to detect
	 */
	ret = __start_auto_ir();
	if (ret < 0)
		return -1;

	ret = __ld_osd_time_charlib();
	if (ret < 0)
		return -1;

	__init_video_stream_info();

	return 0;
}

static int __add_chn_sanity_check(QCamVideoInputChannel *pchn)
{
	/* there's one stream only equipped with isp channel */
	if ((pchn->channelId < 0) || (pchn->channelId >= (STREAM_COUNT - 1)))
		goto exit;
	if ((pchn->res <= 0) || (pchn->res > 4))
		goto exit;
	if ((pchn->fps <= 0) || (pchn->fps > MAX_FPS))
		goto exit;
	if ((pchn->bitrate <= 0) || (pchn->bitrate > MAX_BITRATE))
		goto exit;
	if (pchn->gop <= 0)
		goto exit;
	if ((pchn->vbr != YSX_BITRATE_MODE_CBR)
		&& (pchn->vbr != YSX_BITRATE_MODE_VBR))
		goto exit;
	if (pchn->cb == NULL)
		goto exit;

	return 0;

exit:
	RTS_ERR("[QCamVideoInput_AddChannel] sanity check fail\n");
	return -1;
}

static void __transform_resolution(QCAM_VIDEO_RESOLUTION res,
					int *width, int *height)
{
	switch (res) {
	case QCAM_VIDEO_RES_720P:
		*width = 1280;
		*height = 720;
		break;
	case QCAM_VIDEO_RES_480P:
		*width = 720;
		*height = 480;
		break;
	case QCAM_VIDEO_RES_360P:
		*width = 640;
		*height = 320;
		break;
	case QCAM_VIDEO_RES_1080P:
		/*TODO*/
		*width = 1920;
		*height = 1080;
		RTS_INFO("~~~~~~Please confirm the 1080p is right\n");
		break;
	default:
		RTS_ERR("Unknown resolution\n");
		break;
	}
}

static void __init_isp_params(QCamVideoInputChannel *pchn,
				struct rts_m_isp_params *isp_para)
{
	int width, height;

	if (pchn->fps >= 20)
		isp_para->attr.isp_buf_num = 3;
	else
		isp_para->attr.isp_buf_num = 2;
	isp_para->attr.isp_id = pchn->channelId;

	__transform_resolution(pchn->res, &width, &height);

	isp_para->profile.fmt = RTS_V_FMT_YUV420SEMIPLANAR;
	isp_para->profile.video.width = width;
	isp_para->profile.video.height = height;
	isp_para->profile.video.numerator = 1;
	isp_para->profile.video.denominator = pchn->fps;
}

static int __create_isp_chn(struct rts_middle_stream *pstm,
				struct rts_m_isp_params *isp_para)
{
	int ret = 0;

	ret = rts_av_create_isp_chn(&isp_para->attr);
	if (ret < 0) {
		pstm->isp.err = RTS_CHN_E_CREATE_FAIL;
		goto exit;
	}

	pstm->isp.id = ret;

	ret = rts_av_set_profile(pstm->isp.id, &isp_para->profile);
	if (ret) {
		pstm->isp.err = RTS_CHN_E_SET_PROFILE_FAIL;
		goto exit;
	}


	pstm->isp.stat = RTS_CHN_STAT_OK;
	pstm->isp.err = RTS_CHN_E_OK;
	pstm->isp.buf_num = isp_para->attr.isp_buf_num;
	pstm->isp.fps = isp_para->profile.video.denominator;
	pstm->isp.width = isp_para->profile.video.width;
	pstm->isp.height = isp_para->profile.video.height;

	RTS_INFO("Stream[%d] create isp chn success\n", pstm->id);

	return 0;

exit:
	RTS_ERR("Stream[%d] isp chn fail, err[%d], ret[%d]\n",
			pstm->id, pstm->isp.err, ret);
	pstm->isp.stat = RTS_CHN_STAT_FAIL;
	pstm->stat = RTS_STM_STAT_ISP_FAIL;
	return -1;
}

static int __create_osd_chn(struct rts_middle_stream *pstm)
{
	int ret = 0;

	ret = rts_av_create_osd_chn();
	if (ret < 0) {
		pstm->osd.err = RTS_CHN_E_CREATE_FAIL;
		goto exit;
	}

	pstm->osd.id = ret;
	pstm->osd.stat = RTS_CHN_STAT_OK;
	pstm->osd.err = RTS_CHN_E_OK;

	RTS_INFO("Stream[%d] create osd chn success\n", pstm->id);

	return 0;

exit:
	RTS_ERR("Stream[%d] osd chn fail,err[%d], ret[%d]\n",
			pstm->id, pstm->osd.err, ret);
	pstm->osd.stat = RTS_CHN_STAT_FAIL;
	pstm->stat = RTS_STM_STAT_OSD_FAIL;
	return -1;
}

static int __query_osd2_attr(struct rts_middle_stream *pstm)
{
	int ret = 0;

	ret = rts_av_query_osd2(pstm->osd.id, &pstm->osd_attr);
	if (ret < 0) {
		pstm->osd.err = RTS_CHN_E_QUERY_FAIL;
		goto exit;
	}

	/* time background half transparency */
	ret = rts_av_set_osd2_color_table(pstm->osd_attr, TM_PICT_FORMAT,
			0x00000080, 0, 0, 0, 0);
	if (ret)
		RTS_ERR("Set_osd_color_table failed ,ret[%d]\n", ret);

	pstm->osd.stat = RTS_CHN_STAT_OK;
	pstm->osd.err = RTS_CHN_E_OK;

	RTS_INFO("Stream[%d] query osd attr success\n", pstm->id);

	return 0;

exit:
	RTS_ERR("Stream[%d] query osd attr fail, err[%d], ret[%d]\n",
			pstm->id, pstm->osd.err, ret);
	pstm->osd.stat = RTS_CHN_STAT_FAIL;
	pstm->stat = RTS_STM_STAT_OSD_FAIL;
	return -1;
}

static int __create_jpg_chn(struct rts_middle_stream *pstm)
{
	struct rts_jpgenc_attr attr;
	int ret = 0;

	attr.rotation = RTS_AV_ROTATION_0;

	ret = rts_av_create_mjpeg_chn(&attr);
	if (ret < 0) {
		pstm->jpg.err = RTS_CHN_E_CREATE_FAIL;
		goto exit;
	}

	pstm->jpg.id = ret;
	pstm->jpg.stat = RTS_CHN_STAT_OK;
	pstm->jpg.err = RTS_CHN_E_OK;

	RTS_INFO("Stream[%d] create mjpeg chn success\n", pstm->id);

	return 0;

exit:
	RTS_ERR("Stream[%d] mjpeg chn fail,err[%d], ret[%d]\n",
			pstm->id, pstm->jpg.err, ret);
	pstm->jpg.stat = RTS_CHN_STAT_FAIL;
	pstm->stat = RTS_STM_STAT_JPEG_FAIL;
	return -1;
}

static int __set_jpg_ctrl(struct rts_middle_stream *pstm)
{
	int ret = 0;

	ret = rts_av_query_mjpeg_ctrl(pstm->jpg.id, &pstm->mjpeg_ctrl);
	if (ret < 0) {
		pstm->jpg.err = RTS_CHN_E_QUERY_FAIL;
		goto exit;
	}

	pstm->mjpeg_ctrl->normal_compress_rate = MJPEG_COMPRESS_RATE;

	ret = rts_av_set_mjpeg_ctrl(pstm->mjpeg_ctrl);
	if (ret < 0) {
		pstm->jpg.err = RTS_CHN_E_SET_JPEG_FAIL;
		goto exit;
	}

	pstm->jpg.stat = RTS_CHN_STAT_OK;
	pstm->jpg.err = RTS_CHN_E_OK;

	RTS_INFO("Stream[%d] set mjpeg ctrl success\n", pstm->id);

	return 0;

exit:
	RTS_ERR("Stream[%d] set mjpeg ctrl fail, err[%d], ret[%d]\n",
			pstm->id, pstm->jpg.err, ret);
	pstm->jpg.stat = RTS_CHN_STAT_FAIL;
	pstm->stat = RTS_STM_STAT_JPEG_FAIL;
	return -1;
}

static void __init_h264_params(QCamVideoInputChannel *pchn,
				struct rts_m_h264_params *h264_para)
{
	/*TODO 0 main stream*/
	if (pchn->channelId == 0)
		h264_para->attr.level = H264_LEVEL_4_1;
	else
		h264_para->attr.level = H264_LEVEL_4;

	h264_para->attr.qp = -1;
	h264_para->attr.bps = pchn->bitrate * 1024;
	h264_para->attr.gop = pchn->gop * pchn->fps;
	h264_para->attr.videostab = 0;
	h264_para->attr.rotation = RTS_AV_ROTATION_0;

	h264_para->bitrate_mode = pchn->vbr;
}

static int __create_h264_chn(struct rts_middle_stream *pstm,
		struct rts_m_h264_params *h264_para)
{
	int ret = 0;

	ret = rts_av_create_h264_chn(&h264_para->attr);
	if (ret < 0) {
		pstm->h264.err = RTS_CHN_E_CREATE_FAIL;
		goto exit;
	}

	pstm->h264.id = ret;
	ret = rts_av_query_h264_ctrl(pstm->h264.id, &pstm->h264_ctrl);
	if (ret < 0) {
		pstm->h264.err = RTS_CHN_E_QUERY_FAIL;
		goto exit;
	}

	switch (h264_para->bitrate_mode) {
	case YSX_BITRATE_MODE_CBR:
		pstm->h264_ctrl->bitrate_mode = RTS_BITRATE_MODE_CBR;
		pstm->h264_ctrl->qp = QP_CBR_DEFAULT;
		pstm->h264_ctrl->bitrate = h264_para->attr.bps;
		break;
	case YSX_BITRATE_MODE_VBR:
		pstm->h264_ctrl->bitrate_mode = RTS_BITRATE_MODE_VBR;
		pstm->h264_ctrl->qp = pstm->h264_ctrl->min_qp =
					pstm->h264_ctrl->max_qp =
							QP_VBR_DEFAULT;
		break;
	}

	ret = rts_av_set_h264_ctrl(pstm->h264_ctrl);
	if (ret < 0) {
		pstm->h264.err = RTS_CHN_E_SET_H264_FAIL;
		goto exit;
	}

	pstm->h264.stat = RTS_CHN_STAT_OK;
	pstm->h264.err = RTS_CHN_E_OK;

	RTS_INFO("Stream[%d] create h264 chn success\n");

	return 0;

exit:
	RTS_ERR("Stream[%d] h264 chn fail,err[%d], ret[%d]\n",
			pstm->id, pstm->h264.err, ret);
	pstm->h264.stat = RTS_CHN_STAT_FAIL;
	pstm->stat = RTS_STM_STAT_H264_FAIL;
	return -1;
}

int __destroy_chn(Mchannel *pchn)
{
	int ret = 0;

	if (pchn == NULL)
		return -1;
	if (pchn->stat != RTS_CHN_STAT_OK)
		goto exit;

	ret = rts_av_destroy_chn(pchn->id);
	if (ret < 0) {
		pchn->err = RTS_CHN_E_DESTROY_FAIL;
		goto exit;
	}

	return 0;

exit:
	pchn->stat = RTS_CHN_STAT_FAIL;
	RTS_ERR("Destroy chn fail, type[%d], ret[%d]\n", pchn->type, ret);
	return -1;
}

int __bind_chn(Mchannel *pchn1, Mchannel *pchn2)
{
	int ret = 0;

	if ((pchn1 == NULL) || (pchn2 == NULL))
		return -1;
	if ((pchn1->stat != RTS_CHN_STAT_OK)
			|| pchn2->stat != RTS_CHN_STAT_OK)
		goto exit;

	ret = rts_av_bind(pchn1->id, pchn2->id);
	if (ret < 0) {
		pchn1->err = pchn2->err = RTS_CHN_E_BIND_FAIL;
		goto exit;
	}

	return 0;

exit:
	pchn1->stat = pchn2->stat = RTS_CHN_STAT_FAIL;
	RTS_ERR("Bind chn fail, ret[%d]\n", ret);
	return -1;
}

int __unbind_chn(Mchannel *pchn1, Mchannel *pchn2)
{
	int ret = 0;

	if ((pchn1 == NULL) || (pchn2 == NULL))
		return -1;
	if ((pchn1->stat != RTS_CHN_STAT_OK)
			|| (pchn2->stat != RTS_CHN_STAT_OK))
		goto exit;

	ret = rts_av_unbind(pchn1->id, pchn2->id);
	if (ret < 0) {
		pchn1->err = pchn2->err = RTS_CHN_E_UNBIND_FAIL;
		goto exit;
	}

	return 0;

exit:
	pchn1->stat = pchn2->stat = RTS_CHN_STAT_FAIL;
	RTS_ERR("Unbind chn fail, ret[%d]\n", ret);
	return -1;
}

int __enable_chn(Mchannel *pchn)
{
	int ret = 0;

	if (pchn == NULL)
		return -1;
	if (pchn->stat != RTS_CHN_STAT_OK)
		goto exit;

	ret = rts_av_enable_chn(pchn->id);
	if (ret < 0) {
		pchn->err = RTS_CHN_E_ENABLE_FAIL;
		goto exit;
	}

	return 0;

exit:
	pchn->stat = RTS_CHN_STAT_FAIL;
	RTS_ERR("Enable chn fail, type[%d], ret[%d]\n", pchn->type, ret);
	return -1;
}

int __disable_chn(Mchannel *pchn)
{
	int ret = 0;

	if (pchn == NULL)
		return -1;
	if (pchn->stat != RTS_CHN_STAT_OK)
		goto exit;

	ret = rts_av_disable_chn(pchn->id);
	if (ret < 0) {
		pchn->err = RTS_CHN_E_DISABLE_FAIL;
		goto exit;
	}

	return 0;

exit:
	pchn->stat = RTS_CHN_STAT_FAIL;
	RTS_ERR("Disable chn fail, type[%d], ret[%d]\n", pchn->type, ret);
	return -1;
}

static int __create_stream(struct rts_middle_stream *pstm,
					QCamVideoInputChannel *pchn)
{
	struct rts_m_isp_params isp_para;
	struct rts_m_h264_params h264_para;
	int ret = 0;

	pthread_mutex_lock(&pstm->mutex);

	__init_isp_params(pchn, &isp_para);
	ret = __create_isp_chn(pstm, &isp_para);
	if (ret < 0)
		goto exit;

	/* TODO every channel need osd ?? */
	ret = __create_osd_chn(pstm);
	if (ret < 0)
		goto exit;

	if (pchn->channelId == 0) {
		ret = __create_jpg_chn(pstm);
		if (ret < 0)
			goto exit;
	}

	__init_h264_params(pchn, &h264_para);
	ret = __create_h264_chn(pstm, &h264_para);
	if (ret < 0)
		goto exit;

	ret = __bind_chn(&pstm->isp, &pstm->osd);
	if (ret < 0)
		goto exit;

	ret = __query_osd2_attr(pstm);
	if (ret < 0)
		goto exit;

	ret = __bind_chn(&pstm->osd, &pstm->h264);
	if (ret < 0)
		goto exit;

	if (pchn->channelId == 0) {
		ret = __bind_chn(&pstm->osd, &pstm->jpg);
		if (ret < 0)
			goto exit;
	}

	ret = __enable_chn(&pstm->isp);
	if (ret < 0)
		goto exit;

	ret = __enable_chn(&pstm->osd);
	if (ret < 0)
		goto exit;

	ret = __enable_chn(&pstm->h264);
	if (ret < 0)
		goto exit;

	if (pchn->channelId == 0) {
		ret = __enable_chn(&pstm->jpg);
		if (ret < 0)
			goto exit;
		ret =  __set_jpg_ctrl(pstm);
		if (ret < 0)
			goto exit;
	}

	pstm->cb = pchn->cb;
	pstm->stat = RTS_STM_STAT_OK;

exit:
	pthread_mutex_unlock(&pstm->mutex);
	return ret;
}

static int __destroy_stream(struct rts_middle_stream *pstm)
{
	int ret = 0;

	pthread_mutex_lock(&pstm->mutex);

	/* disbale channel */
	if (pstm->h264.stat != RTS_CHN_STAT_UNINIT)
		ret |= __disable_chn(&pstm->h264);

	if (pstm->jpg.stat != RTS_CHN_STAT_UNINIT)
		ret |= __disable_chn(&pstm->jpg);

	if (pstm->osd.stat != RTS_CHN_STAT_UNINIT)
		ret |= __disable_chn(&pstm->osd);

	if (pstm->isp.stat != RTS_CHN_STAT_UNINIT)
		ret |= __disable_chn(&pstm->isp);

	/* unbind channel */
	if (pstm->h264.stat != RTS_CHN_STAT_UNINIT)
		ret |= __unbind_chn(&pstm->h264, &pstm->osd);

	if (pstm->jpg.stat != RTS_CHN_STAT_UNINIT)
		ret |= __unbind_chn(&pstm->jpg, &pstm->osd);

	if (pstm->osd.stat != RTS_CHN_STAT_UNINIT)
		ret |= __unbind_chn(&pstm->osd, &pstm->isp);

	/* destroy channel */
	if (pstm->h264.stat != RTS_CHN_STAT_UNINIT) {
		rts_av_release_h264_ctrl(pstm->h264_ctrl);
		ret |= __destroy_chn(&pstm->h264);
	}

	if (pstm->jpg.stat != RTS_CHN_STAT_UNINIT) {
		rts_av_release_mjpeg_ctrl(pstm->mjpeg_ctrl);
		ret |= __destroy_chn(&pstm->jpg);
	}

	if (pstm->osd.stat != RTS_CHN_STAT_UNINIT) {
		rts_av_release_osd2(pstm->osd_attr);
		ret |= __destroy_chn(&pstm->osd);
	}

	if (pstm->isp.stat != RTS_CHN_STAT_UNINIT)
		ret |= __destroy_chn(&pstm->isp);


	pthread_mutex_unlock(&pstm->mutex);

	return ret;
}

int QCamVideoInput_AddChannel(QCamVideoInputChannel ch)
{
	int ret = 0;
	struct rts_middle_stream *pstm;

	ret = __add_chn_sanity_check(&ch);
	if (ret < 0)
		return -1;

	pstm = &vstream[ch.channelId];
	if (pstm->stat != RTS_STM_STAT_UNINIT) {
		RTS_ERR("Stream[%d] already initialized\n", ch.channelId);
		return -1;
	}

	ret = __create_stream(pstm, &ch);
	if (ret < 0)
		goto exit;

	cur_stream_count++;

	RTS_INFO("Create stream[%d] success\n", ch.channelId);

	return 0;

exit:
	RTS_ERR("Create stream[%d] fail, ret[%d]\n", ch.channelId, ret);
	/*TODO*/
	/*__dump_stream(pstm);*/
	return -1;
}

int __start_chn(Mchannel *pchn, int dir)
{
	int ret = 0;

	if (pchn == NULL)
		return -1;
	if (pchn->stat == RTS_CHN_STAT_RUN)
		return 0;
	if (pchn->stat != RTS_CHN_STAT_OK)
		goto exit;

	if (dir == RTS_RECV) {
		ret = rts_av_start_recv(pchn->id);
		if (ret < 0)
			pchn->err = RTS_CHN_E_START_RECV_FAIL;
	} else {
		ret = rts_av_start_send(pchn->id);
		if (ret < 0)
			pchn->err = RTS_CHN_E_START_SEND_FAIL;
	}

	if (ret == 0) {
		pchn->stat = RTS_CHN_STAT_RUN;
		return 0;
	}

exit:
	pchn->stat = RTS_CHN_STAT_FAIL;
	RTS_ERR("Start chn fail, type[%d], dir[%d], ret[%d]\n",
						pchn->type, dir, ret);
	return -1;
}

int __stop_chn(Mchannel *pchn, int dir)
{
	int ret = 0;

	if (pchn == NULL)
		return -1;
	if (pchn->stat == RTS_CHN_STAT_OK)
		return 0;
	if (pchn->stat != RTS_CHN_STAT_RUN)
		goto exit;

	if (dir == RTS_RECV) {
		ret = rts_av_stop_recv(pchn->id);
		if (ret < 0)
			pchn->err = RTS_CHN_E_STOP_RECV_FAIL;
	} else {
		ret = rts_av_stop_send(pchn->id);
		if (ret < 0)
			pchn->err = RTS_CHN_E_STOP_SEND_FAIL;
	}

	if (ret == 0) {
		pchn->stat = RTS_CHN_STAT_OK;
		return 0;
	}

exit:
	pchn->stat = RTS_CHN_STAT_FAIL;
	RTS_ERR("Stop chn fail, type[%d], dir[%d], ret[%d]\n",
						pchn->type, dir, ret);
	return -1;
}

static void __stream_thread(void *arg)
{
	struct rts_middle_stream *pstm = NULL;
	struct rts_av_buffer *buffer = NULL;
	struct timeval time;
	int keyframe = 0;
	int ret = 0;

	pstm = (struct rts_middle_stream *)arg;

	ret = __start_chn(&pstm->h264, RTS_RECV);
	if (ret < 0)
		return;

	while (!pstm->exit) {
		usleep(1000);
		/*TODO add mutex or not*/
		pthread_mutex_lock(&pstm->mutex);

		if (rts_av_poll(pstm->h264.id)) {
			pthread_mutex_unlock(&pstm->mutex);
			continue;
		}
		if (rts_av_recv(pstm->h264.id, &buffer)) {
			pthread_mutex_unlock(&pstm->mutex);
			continue;
		}

		if (buffer) {
			/*TODO monotonic time is recommend*/
			time.tv_sec = (__time_t)(buffer->timestamp / 1000000);
			time.tv_usec =
				(__suseconds_t)(buffer->timestamp % 1000000);

			if (buffer->flags & RTSTREAM_PKT_FLAG_KEY)
				keyframe = 1;
			else
				keyframe = 0;

			pstm->cb((const struct timeval *)&time,
					(const void *)(buffer->vm_addr),
					(const int)(buffer->bytesused),
					(const int)keyframe);

			rts_av_put_buffer(buffer);
		}

		pthread_mutex_unlock(&pstm->mutex);
	}

	__stop_chn(&pstm->h264, RTS_RECV);
}

int QCamVideoInput_Start()
{
	int ret = 0;
	int i;

	tpool = rts_pthreadpool_init(cur_stream_count);
	if (!tpool)
		return -1;

	for (i = 0; i < cur_stream_count; i++) {
		ret = rts_pthreadpool_add_task(tpool, __stream_thread,
							&vstream[i], NULL);
		if (ret < 0) {
			RTS_ERR("Add stream thread fail\n");
			return -2;
		}
	}

	/*wait pthread pool work*/
	usleep(50000);

	return 0;
}

static void __stop_stream(struct rts_middle_stream *pstm)
{
	pstm->exit = 1;
}

int QCamVideoInput_Uninit()
{
	int ret = 0;
	int i;

	for (i = 0; i < cur_stream_count; i++)
		__stop_stream(&vstream[i]);

	RTS_SAFE_RELEASE(tpool, rts_pthreadpool_destroy);

	if (osd2_com.run)
		__stop_osd_supervisor();

	for (i = 0; i < STREAM_COUNT; i++) {
		ret = __destroy_stream(&vstream[i]);
		if (ret < 0)
			goto exit;
	}

	__deinit_video_stream_info();

	__release_auto_ir();

	return 0;

exit:
	RTS_ERR("Destroy stream[%d] fail, ret[%d]\n", i, ret);
	return -1;
}

static int __update_h264_ctrl(struct rts_middle_stream *pstm,
						int bitrate, int qp)
{
	int ret = 0;

	pthread_mutex_lock(&pstm->mutex);

	__stop_chn(&pstm->h264, RTS_RECV);
	__disable_chn(&pstm->h264);

	if (bitrate != -1) {
		if (pstm->h264_ctrl->bitrate_mode == RTS_BITRATE_MODE_CBR)
			pstm->h264_ctrl->bitrate = bitrate;
		if (pstm->h264_ctrl->bitrate_mode == RTS_BITRATE_MODE_VBR)
			RTS_INFO("VBR mode bitrate is auto changed by sdk\n");
	}

	if (qp != -1) {
		if (pstm->h264_ctrl->bitrate_mode == RTS_BITRATE_MODE_VBR)
			pstm->h264_ctrl->qp = qp;
		if (pstm->h264_ctrl->bitrate_mode == RTS_BITRATE_MODE_CBR)
			RTS_INFO("CBR mode qp is not recommend to change\n");
	}

	ret = rts_av_set_h264_ctrl(pstm->h264_ctrl);
	if (ret < 0) {
		pstm->h264.err = RTS_CHN_E_SET_H264_FAIL;
		RTS_ERR("Set h264 ctrl fail, ret[%d]\n", ret);
	}

	__enable_chn(&pstm->h264);
	__start_chn(&pstm->h264, RTS_RECV);
	pthread_mutex_unlock(&pstm->mutex);

	return ret;
}

int QCamVideoInput_SetBitrate(int channel, int bitrate, int isVBR)
{
	/*TODO confirm isVBR*/
	int ret = 0;
	struct rts_middle_stream *pstm = NULL;

	if ((bitrate <= 0) || (bitrate > MAX_BITRATE))
		goto exit;

	if ((channel < 0) || (channel > STREAM_COUNT)
			|| (vstream[channel].stat == RTS_STM_STAT_UNINIT))
		goto exit;

	pstm = &vstream[channel];

	ret = __update_h264_ctrl(pstm, bitrate, -1);

	return ret;

exit:
	RTS_ERR("[QCamVideoInput_SetBitrate] sanity check fail\n");
	return -1;
}

int QCamVideoInput_SetInversion(int enable)
{
	return __set_isp_ctrl(RTS_VIDEO_CTRL_ID_FLIP, enable);
}

int QCamVideoInput_SetIFrame(int channel)
{
	struct rts_middle_stream *pstm = NULL;
	int ret = 0;

	if ((channel < 0) || (channel > STREAM_COUNT)
			|| (vstream[channel].stat == RTS_STM_STAT_UNINIT))
		goto exit;

	pstm = &vstream[channel];

	 ret = rts_av_request_h264_key_frame(pstm->h264.id);
	 if (ret < 0)
		 RTS_ERR("Set I frame fail, ret[%d]\n", ret);

	 return ret;

exit:
	RTS_ERR("[QCamVideoInput_SetIFrame] sanity check fail\n");
	return -1;
}

int QCamVideoInput_SetOSD(int channel, QCamVideoInputOSD *pOsdInfo)
{
	int ret = 0;

	ret = __osd_sanity_check(channel, pOsdInfo);
	if (ret < 0)
		goto exit;

	if (!osd2_com.run && pOsdInfo->time_enable) {
		__init_osd2_stuf();
		ret = __start_osd_supervisor();
		if (ret < 0)
			goto exit;
	}

	ret = __update_osd_pict(channel, pOsdInfo);
	if (ret < 0)
		goto exit;

	ret = __update_osd_time(channel, pOsdInfo);
	if (ret < 0)
		goto exit;

	return 0;

exit:
	return -1;
}

static void __mjpeg_cb_func(void *priv, struct rts_av_profile *profile,
			struct rts_av_buffer *buffer)
{
	struct mjpeg_cb_data *jpg_cb_d = NULL;

	jpg_cb_d = (struct mjpeg_cb_data *)priv;

	if ((buffer->bytesused) > *(jpg_cb_d->plen)) {
		RTS_ERR("Mjpeg need a bigger buffer [%d] bytes\n",
							buffer->bytesused);
		jpg_cb_d->cb_ret = -1;
		jpg_cb_d->cb_done = 1;
		return;
	}

	memcpy(jpg_cb_d->pbuf, buffer->vm_addr, buffer->bytesused);
	*(jpg_cb_d->plen) = buffer->bytesused;
	jpg_cb_d->cb_ret = 0;
	jpg_cb_d->cb_done = 1;
}

/* catch main stream */
int QCamVideoInput_CatchJpeg(char *buf, int *bufLen)
{
	struct rts_av_callback cb;
	struct mjpeg_cb_data cb_d;
	int ret = 0;

	if (vstream[0].stat == RTS_STM_STAT_UNINIT) {
		RTS_ERR("Main stream uninitial, catchjpeg fail\n");
		return -1;
	}


	cb.func = __mjpeg_cb_func;
	cb.start = 0;
	cb.times = 1;
	cb.interval = 0;
	cb.type = RTS_AV_CB_TYPE_ASYNC;
	cb_d.pbuf = buf;
	cb_d.plen = bufLen;
	cb_d.cb_ret = 0;
	cb_d.cb_done = 0;
	cb.priv = &cb_d;

	ret = rts_av_set_callback(vstream[MAIN_STREAM].jpg.id, &cb, 0);
	if (ret < 0) {
		RTS_ERR("Set mjpeg callback fail, ret = %d\n", ret);
		return ret;
	}

	while (!cb_d.cb_done)
		usleep(5000);

	ret = cb_d.cb_done;

	return ret;
}

static int __create_yuv_stm(int width, int height)
{
	struct rts_m_isp_params isp_para;
	int ret = 0;

	isp_para.attr.isp_buf_num = YUV_CAPTURE_BUFS;
	isp_para.attr.isp_id = YUV_CAPTURE_CHN;

	isp_para.profile.fmt = RTS_V_FMT_YUV420SEMIPLANAR;
	isp_para.profile.video.width = width;
	isp_para.profile.video.height = height;
	isp_para.profile.video.numerator = 1;
	isp_para.profile.video.denominator = YUV_CAPTURE_FPS;

	ret = __create_isp_chn(&vstream[YUV_CAPTURE_CHN], &isp_para);
	if (ret < 0)
		goto exit;

	ret = __enable_chn(&vstream[YUV_CAPTURE_CHN].isp);
	if (ret < 0)
		goto exit;

	vstream[YUV_CAPTURE_CHN].stat = RTS_STM_STAT_OK;
	RTS_INFO("Create YUV stream success\n");

	return 0;

exit:
	RTS_ERR("Create YUV stream fail\n");
	return -1;
}

struct yuv_cb_data {
	char *pbuf;
	int len;
	int width;
	int height;
	int cb_done;
	int cb_ret;
};

static void __yuv_cb_func(void *priv, struct rts_av_profile *profile,
			struct rts_av_buffer *buffer)
{
	struct yuv_cb_data *yuv_cb_d = NULL;
	int product;

	yuv_cb_d = (struct yuv_cb_data *)priv;
	product = yuv_cb_d->width * yuv_cb_d->height;

	if (product == yuv_cb_d->len)
		memcpy(yuv_cb_d->pbuf, buffer->vm_addr, yuv_cb_d->len);
	else if ((product + product / 2) == yuv_cb_d->len)
		memcpy(yuv_cb_d->pbuf, buffer->vm_addr, yuv_cb_d->len);
	else {
		RTS_ERR("[QCamVideoInput_CatchYUV] sanity check fail\n");
		yuv_cb_d->cb_done = 1;
		yuv_cb_d->cb_ret = -1;
		return;
	}

	yuv_cb_d->cb_done = 1;
	yuv_cb_d->cb_ret = 0;
}

static int __catch_yuv(int width, int height, char *buf, int buflen)
{
	struct rts_av_callback cb;
	struct yuv_cb_data cb_d;
	int ret = 0;

	cb.func = __yuv_cb_func;
	cb.start = 0;
	cb.times = 1;
	cb.interval = 0;
	cb.type = RTS_AV_CB_TYPE_ASYNC;
	cb_d.pbuf = buf;
	cb_d.len = buflen;
	cb_d.width = width;
	cb_d.height = height;
	cb_d.cb_ret = 0;
	cb_d.cb_done = 0;
	cb.priv = &cb_d;

	ret = rts_av_set_callback(vstream[YUV_CAPTURE_CHN].isp.id, &cb, 0);
	if (ret < 0) {
		RTS_ERR("Set yuv callback fail, ret = %d\n", ret);
		return ret;
	}

	while (!cb_d.cb_done)
		usleep(5000);

	ret = cb_d.cb_done;

	return ret;
}

static int __change_resolution(Mchannel *pchn, int width, int height)
{
	struct rts_av_profile profile;
	int ret = 0;

	ret = __disable_chn(&vstream[YUV_CAPTURE_CHN].isp);
	if (ret < 0)
		goto exit;

	ret = rts_av_get_profile(vstream[YUV_CAPTURE_CHN].isp.id, &profile);
	if (ret < 0) {
		vstream[YUV_CAPTURE_CHN].isp.err = RTS_CHN_E_GET_PROFILE_FAIL;
		goto exit;
	}

	profile.video.width = width;
	profile.video.height = height;

	ret = rts_av_set_profile(vstream[YUV_CAPTURE_CHN].isp.id, &profile);
	if (ret < 0) {
		vstream[YUV_CAPTURE_CHN].isp.err = RTS_CHN_E_SET_PROFILE_FAIL;
		goto exit;
	}

	vstream[YUV_CAPTURE_CHN].isp.width = width;
	vstream[YUV_CAPTURE_CHN].isp.height = height;

	ret = __enable_chn(&vstream[YUV_CAPTURE_CHN].isp);
	if (ret < 0)
		goto exit;

	RTS_INFO("YUV stream change resolution to [%dx%d]\n", width, height);

	return 0;
exit:
	RTS_ERR("Change resolution fail, err[%d], ret[%d]\n",
				vstream[YUV_CAPTURE_CHN].isp.err, ret);
	return -1;
}

int QCamVideoInput_CatchYUV(int w, int h, char *buf, int bufLen)
{
	int ret = 0;

	if (vstream[YUV_CAPTURE_CHN].stat == RTS_STM_STAT_UNINIT) {
		ret = __create_yuv_stm(w, h);
		if (ret < 0)
			return -1;
	}

	if ((vstream[YUV_CAPTURE_CHN].isp.width != w)
			|| (vstream[YUV_CAPTURE_CHN].isp.height != h)) {
		ret = __change_resolution(&vstream[YUV_CAPTURE_CHN].isp, w, h);
		if (ret < 0)
			return -1;
	}

	return __catch_yuv(w, h, buf, bufLen);
}

/* TODO what if hw photosensitivity is not support,
 * use sensor to detect
 */
int QCamVideoInput_HasLight()
{
	int adc_value = 0;
	int i;
	int ret = -1;

	for (i = 0; i < ADC_FACTOR; i++)
		adc_value = __get_adc_smooth_value(ADC_CHN);


	if (adc_value >= ADC_DAY_THR)
		ret = QCAM_VIDEO_DAY;
	else if (adc_value <= ADC_NIGHT_THR)
		ret = QCAM_VIDEO_NIGHT;

	return ret;
}

/* must call after [QCamVideoInput_Init] */
void QCamSetIRMode(QCAM_IR_MODE mode)
{
	if ((mode < -1) || (mode > 2)) {
		RTS_ERR("[QCamSetIRMode] sanity check fail\n");
		return;
	}

	if (mode == QCAM_IR_MODE_AUTO) {
		__restart_auto_ir();
		ir.ir_mode = QCAM_IR_MODE_AUTO;
	}

	if (mode == QCAM_IR_MODE_ON) {
		__pause_auto_ir();
		__switch_daynight_mode(NIGHT);
		ir.ir_mode = QCAM_IR_MODE_ON;
	}

	if (mode == QCAM_IR_MODE_OFF) {
		__pause_auto_ir();
		__switch_daynight_mode(DAY);
		ir.ir_mode = QCAM_IR_MODE_OFF;
	}
}

QCAM_IR_MODE QCamGetIRMode()
{
	return ir.ir_mode;
}

int QCamVideoInput_SetQualityLvl(int channel, int low_enable)
{
	int ret = 0;
	struct rts_middle_stream *pstm = NULL;

	if (low_enable < 0)
		goto exit;

	if ((channel < 0) || (channel > STREAM_COUNT)
			|| (vstream[channel].stat == RTS_STM_STAT_UNINIT))
		goto exit;

	pstm = &vstream[channel];

	ret = __update_h264_ctrl(pstm, -1, (low_enable ? QP_LOW : QP_HIGH));

	return ret;

exit:
	RTS_ERR("[QCamVideoInput_SetQualityLvl] sanity check fail\n");
	return -1;
}

