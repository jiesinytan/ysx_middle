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
#include <rts_io_gpio.h>

#include <qcam_audio_input.h>
#include <qcam_audio_output.h>

#include "rts_middle_media.h"


struct audio_server audio_svr;
struct rts_m_spk_gpio spk_gpio;


static int __create_audio_chn(Mchannel *pchn,
					struct rts_audio_attr *attr)
{
	int chn = -1;
	int ret = 0;

	switch (pchn->type) {
	case RTS_MIDDLE_AUDIO_CHN_PLAYBACK:
		chn = rts_av_create_audio_playback_chn(attr);
		break;
	case RTS_MIDDLE_AUDIO_CHN_CAPTURE:
		chn = rts_av_create_audio_capture_chn(attr);
		break;
	case RTS_MIDDLE_AUDIO_CHN_ENCODE_ALAW:
		chn = rts_av_create_audio_encode_chn(RTS_AUDIO_TYPE_ID_ALAW,
							attr->rate);
		break;
	case RTS_MIDDLE_AUDIO_CHN_ENCODE_AAC:
		chn = rts_av_create_audio_encode_chn(RTS_AUDIO_TYPE_ID_AAC,
							attr->rate);
		break;
	case RTS_MIDDLE_AUDIO_CHN_AEC:
		chn = rts_av_create_audio_aec_chn();
		break;
	case RTS_MIDDLE_AUDIO_CHN_RESAMPLE:
		chn = rts_av_create_audio_resample_chn(attr->rate,
							attr->format,
							attr->channels);
		break;
	case RTS_MIDDLE_AUDIO_CHN_DECODE:
		chn = rts_av_create_audio_decode_chn();
		break;
	case RTS_MIDDLE_AUDIO_CHN_MIXER:
		chn = rts_av_create_audio_mixer_chn();
		break;

	default:
		return -1;
	}

	if (chn < 0) {
		pchn->err = RTS_CHN_E_CREATE_FAIL;
		ret = chn;
		goto exit;
	}

	pchn->id = chn;
	pchn->stat = RTS_CHN_STAT_OK;
	pchn->err = RTS_CHN_E_OK;
	if (attr != NULL) {
		pchn->sample_rate = attr->rate;
		pchn->bitfmt = attr->format;
		pchn->channels = attr->channels;
	}

	YSX_LOG(LOG_MW_INFO, "Create audio[type:%d] chn success\n", pchn->type);

	return 0;

exit:
	YSX_LOG(LOG_MW_ERROR, "Create audio[type:%d] chn fail, err[%d], ret[%d]\n",
						pchn->type, pchn->err, ret);
	pchn->stat = RTS_CHN_STAT_FAIL;
	return -1;
}

static int __init_audio_in(struct rts_audio_attr *attr)
{
	int ret = 0;

	strcpy(attr->dev_node, AUDIO_DEVICE1);

	ret = __create_audio_chn(&audio_svr.ai, attr);
	if (ret < 0) {
		audio_svr.pai = NULL;
		goto exit;
	} else
		audio_svr.pai = &audio_svr.ai;

	return 0;

exit:
	YSX_LOG(LOG_MW_ERROR, "Create audio input channel fail\n");
	return -1;
}

static int __init_audio_out(struct rts_audio_attr *attr)
{
	int suc = 0;

	strcpy(attr->dev_node, AUDIO_DEVICE1);

	do {
		if (__create_audio_chn(&audio_svr.ao_resample, attr))
			break;

		if (__create_audio_chn(&audio_svr.mixer, NULL))
			break;

		if (__create_audio_chn(&audio_svr.ao, attr))
			break;

		audio_svr.ao.sample_rate = attr->rate;
		audio_svr.ao.bitfmt = attr->format;
		audio_svr.ao.channels = attr->channels;

		/* TODO bind ao_resample---mixer when start ao */

		if (__bind_chn(&audio_svr.mixer, &audio_svr.ao))
			break;

		suc = 1;
	} while (0);

	if (!suc)
		audio_svr.pao = NULL;
	else
		audio_svr.pao = &audio_svr.ao_resample;

	if (suc)
		return 0;

	YSX_LOG(LOG_MW_ERROR, "Create audio output channel list fail\n");
	return -1;
}

static void __calc_audio_period_frames(struct rts_audio_attr *attr)
{
	int period_frame_len_bits = 0;

	period_frame_len_bits = (attr->rate * attr->format * attr->channels)
					 / 1000 * DFT_AD_IN_PERIOD_TM;

	attr->period_frames = period_frame_len_bits / attr->format;
}

static int __init_ai_device(void)
{
	int ret = 0;

	if (!CHN_UNINIT(audio_svr.ai)) {
		YSX_LOG(LOG_MW_INFO, "Audio input channel already init\n");
		return 0;
	}

	memset(&audio_svr.ai_attr, 0, sizeof(audio_svr.ai_attr));
	audio_svr.ai_attr.channels = DFT_AD_IN_CHANNELS;
	audio_svr.ai_attr.format = DFT_AD_IN_BITFMT;
	audio_svr.ai_attr.rate = DFT_AD_IN_RATE_8K;

	__calc_audio_period_frames(&audio_svr.ai_attr);
	ret = __init_audio_in(&audio_svr.ai_attr);
	if (ret < 0)
		goto exit;

	return 0;

exit:
	YSX_LOG(LOG_MW_ERROR, "Init audio input device fail\n");
	return -1;
}

static int __init_ao_device(void)
{
	int ret = 0;

	if (audio_svr.pao && (!CHN_UNINIT(*audio_svr.pao))) {
		YSX_LOG(LOG_MW_INFO, "Audio input channel already init\n");
		return 0;
	}

	memset(&audio_svr.ao_attr, 0, sizeof(audio_svr.ao_attr));
	audio_svr.ao_attr.channels = DFT_AD_OUT_CHANNELS;
	audio_svr.ao_attr.format = DFT_AD_OUT_BITFMT;
	audio_svr.ao_attr.rate = DFT_AD_OUT_RATE_8K;

	/* TODO set playback resample chn buffer profile when send frame to res chn*/
	memset(&audio_svr.ao_profile, 0, sizeof(audio_svr.ao_profile));
	audio_svr.ao_profile.fmt = RTS_A_FMT_AUDIO;
	audio_svr.ao_profile.audio.channels = 1;

	ret = __init_audio_out(&audio_svr.ao_attr);

	return ret;
}

static void __init_audio_chn_info(void)
{
	struct audio_server *svr = NULL;

	svr = &audio_svr;
	memset(&audio_svr, 0, sizeof(audio_svr));

	audio_svr.svr_init = 1;

	__init_chn_info(&svr->ai, RTS_MIDDLE_AUDIO_CHN_CAPTURE);
	__init_chn_info(&svr->aec, RTS_MIDDLE_AUDIO_CHN_AEC);
	__init_chn_info(&svr->enc_alaw, RTS_MIDDLE_AUDIO_CHN_ENCODE_ALAW);
	__init_chn_info(&svr->enc_aac, RTS_MIDDLE_AUDIO_CHN_ENCODE_AAC);
	__init_chn_info(&svr->ao, RTS_MIDDLE_AUDIO_CHN_PLAYBACK);
	__init_chn_info(&svr->ao_resample, RTS_MIDDLE_AUDIO_CHN_RESAMPLE);
	__init_chn_info(&svr->ai_resample, RTS_MIDDLE_AUDIO_CHN_RESAMPLE);
	__init_chn_info(&svr->aec_resample, RTS_MIDDLE_AUDIO_CHN_RESAMPLE);
	__init_chn_info(&svr->ptoa_resample, RTS_MIDDLE_AUDIO_CHN_RESAMPLE);
	__init_chn_info(&svr->mixer, RTS_MIDDLE_AUDIO_CHN_MIXER);
}

static int __init_aec_module(void)
{
	int suc = 0;
	struct rts_audio_attr attr;

	if (audio_svr.softaec)
		return 0;

	if (!CHN_UNINIT(audio_svr.aec)) {
		YSX_LOG(LOG_MW_INFO, "Audio aec module already init\n");
		return 0;
	}

	if (!CHN_OK(audio_svr.ao) || !CHN_OK(audio_svr.ai)) {
		YSX_LOG(LOG_MW_ERROR, "Start ai ao before enable aec\n");
		return -1;
	}

	do {
		if (__create_audio_chn(&audio_svr.ptoa_resample,
					&audio_svr.ai_attr))
			break;

		if (__create_audio_chn(&audio_svr.aec, NULL))
			break;

		/* TODO
		 * @fix aec_resample_chn rate to 16k
		 * @aec_resample_chn is used to switch capture rate
		 * between 8k & 16k
		 * @16k:bind this chn with aec_chn
		 * @8k:unbind this chn from aec_chn
		 */
		attr.format = audio_svr.ai_attr.format;
		attr.channels = audio_svr.ai_attr.channels;
		attr.rate = AEC_RESAMPLE_RATE_16K;

		if (__create_audio_chn(&audio_svr.aec_resample,
					&attr))
			break;

		if (__bind_chn(&audio_svr.ao, &audio_svr.ptoa_resample))
			break;

		if (__bind_chn(&audio_svr.ptoa_resample, &audio_svr.aec))
			break;

		if (__bind_chn(&audio_svr.ai, &audio_svr.aec))
			break;

		suc = 1;
	} while (0);

	if (suc) {
		audio_svr.softaec = 1;
		audio_svr.pai = &audio_svr.aec;

		return 0;
	} else {
		YSX_LOG(LOG_MW_ERROR, "AEC module init failed\n");

		__unbind_chn(&audio_svr.ai, &audio_svr.aec);
		__unbind_chn(&audio_svr.ao, &audio_svr.ptoa_resample);
		__unbind_chn(&audio_svr.ptoa_resample, &audio_svr.aec);

		__destroy_chn(&audio_svr.aec_resample);
		__destroy_chn(&audio_svr.aec);
		__destroy_chn(&audio_svr.ptoa_resample);

		audio_svr.softaec = 0;

		return -1;
	}
}

static int __enable_audio_all_chn(void)
{
	int suc = 0;

	do {
		if (__enable_chn(&audio_svr.ao_resample))
			break;

		if (__enable_chn(&audio_svr.mixer))
			break;

		if (__enable_chn(&audio_svr.ao))
			break;

		if (__enable_chn(&audio_svr.ptoa_resample))
			break;

		if (__enable_chn(&audio_svr.ai))
			break;

		if (__enable_chn(&audio_svr.aec))
			break;

		if (__enable_chn(&audio_svr.aec_resample))
			break;

		suc = 1;
	} while (0);

	if (!suc) {
		YSX_LOG(LOG_MW_ERROR, "Enable audio all channels fail\n");
		return -1;
	}

	return 0;
}

static int __set_aec_denoise(int chn, int aec, int denoise)
{
	int ret = 0;
	struct rts_aec_control *paec;

	if (chn == -1) {
		ret = -1;
		goto exit;
	}

	ret = rts_av_query_aec_ctrl(chn, &paec);
	if (ret < 0 || !paec)
		goto exit;

	paec->aec_enable = aec;
	paec->ns_enable = denoise > 0 ? 1 : 0;
	paec->ns_level = denoise;

	ret = rts_av_set_aec_ctrl(paec);
	rts_av_release_aec_ctrl(paec);

	if (ret < 0)
		goto exit;

	return 0;

exit:
	YSX_LOG(LOG_MW_ERROR, "Set aec denoise fail, ret[%d]\n", ret);
	return ret;
}

static void *__audio_capture(void *arg)
{
	struct rts_av_buffer *buffer = NULL;
	struct timeval time;
	int ret = 0;

	ret = __start_chn(audio_svr.pai, RTS_RECV);
	if (ret < 0)
		return NULL;

	while (!audio_svr.capture_exit) {
		usleep(1000);
		pthread_mutex_lock(&audio_svr.m_capture);

		if (rts_av_poll(audio_svr.pai->id)) {
			pthread_mutex_unlock(&audio_svr.m_capture);
			continue;
		}
		if (rts_av_recv(audio_svr.pai->id, &buffer)) {
			pthread_mutex_unlock(&audio_svr.m_capture);
			continue;
		}

		if (buffer && audio_svr.capture_start) {
			/*TODO monotonic time is recommend*/
			time.tv_sec = (__time_t)(buffer->timestamp / 1000000);
			time.tv_usec =
				(__suseconds_t)(buffer->timestamp % 1000000);

			/* TODO spk buf is not used */
			audio_svr.aec_cb((const struct timeval *)&time,
					(const void *)(buffer->vm_addr),
					(const int)(buffer->bytesused),
					NULL);

			rts_av_put_buffer(buffer);
		}

		pthread_mutex_unlock(&audio_svr.m_capture);
	}

	__stop_chn(audio_svr.pai, RTS_RECV);

	return NULL;
}

/* static void __attribute__((constructor(AUDIO_INIT_NO + 10))) */
void __init_audio_server(void)
{
	int ret = 0;

	__init_audio_chn_info();

	__init_ai_device();
	__init_ao_device();
	__init_aec_module();
	__enable_audio_all_chn();

	__set_aec_denoise(audio_svr.aec.id, 0, 35);

	ret = rts_audio_set_capture_volume(DFT_AD_CAP_DGAIN);
	if (ret != RTS_OK)
		YSX_LOG(LOG_MW_ERROR, "set capture volume fail [ret:%d]\n", ret);

	ret = rts_audio_set_playback_volume(DFT_AD_PLY_DGAIN);
	if (ret != RTS_OK)
		YSX_LOG(LOG_MW_ERROR, "set capture volume fail [ret:%d]\n", ret);

	YSX_LOG(LOG_MW_INFO, "RTS INIT MIDDLE AUDIO\n");
}

/*static void __attribute__((destructor(AUDIO_INIT_NO + 1))) */
void __release_audio_server(void)
{
	if (audio_svr.capture_run) {
		audio_svr.capture_exit = 1;
		pthread_join(audio_svr.t_capture, NULL);
		pthread_mutex_destroy(&audio_svr.m_capture);
	}

	__set_aec_denoise(audio_svr.aec.id, 0, 0);

	__stop_chn(audio_svr.pai, RTS_RECV);
	__stop_chn(audio_svr.pao, RTS_SEND);

	__disable_chn(&audio_svr.ao_resample);
	__disable_chn(&audio_svr.mixer);
	__disable_chn(&audio_svr.ao);
	__disable_chn(&audio_svr.ptoa_resample);
	__disable_chn(&audio_svr.ai);
	__disable_chn(&audio_svr.aec);
	__disable_chn(&audio_svr.aec_resample);
	__disable_chn(&audio_svr.enc_alaw);
	__disable_chn(&audio_svr.enc_aac);

	__unbind_chn(&audio_svr.ao_resample, &audio_svr.mixer);
	__unbind_chn(&audio_svr.mixer, &audio_svr.ao);
	__unbind_chn(&audio_svr.ao, &audio_svr.ptoa_resample);
	__unbind_chn(&audio_svr.ptoa_resample, &audio_svr.aec);
	__unbind_chn(&audio_svr.ai, &audio_svr.aec);
	__unbind_chn(&audio_svr.aec, &audio_svr.aec_resample);

	__destroy_chn(&audio_svr.ao_resample);
	__destroy_chn(&audio_svr.mixer);
	__destroy_chn(&audio_svr.ao);
	__destroy_chn(&audio_svr.ptoa_resample);
	__destroy_chn(&audio_svr.ai);
	__destroy_chn(&audio_svr.aec);
	__destroy_chn(&audio_svr.aec_resample);
	__destroy_chn(&audio_svr.enc_alaw);
	__destroy_chn(&audio_svr.enc_aac);

	YSX_LOG(LOG_MW_INFO, "RTS RELEASE MIDDLE AUDIO\n");
}

static int __audio_input_open_sanity_check(QCamAudioInputAttr_aec *pAttr)
{
	if ((pAttr->sampleRate != 8000)
			&& (pAttr->sampleRate != 16000)
			&& (pAttr->sampleRate != 48000))
		goto exit;

	if ((pAttr->sampleBit != 8)
			&& (pAttr->sampleBit != 16))
		goto exit;

	if ((pAttr->volume < -1)
			|| (pAttr->volume > 100))
		goto exit;

	if (pAttr->cb == NULL)
		goto exit;

	return 0;

exit:
	YSX_LOG(LOG_MW_ERROR, "[QCamAudioInputOpen_ysx] sanity check fail\n");
	return -1;
}

/* dir:0(capture), 1(playback) */
static int __set_audio_volume(int volume, int dir)
{
	int vol;
	int ret = 0;

	if (volume == -1)
		vol = dir ? DFT_AD_PLY_DGAIN : DFT_AD_CAP_DGAIN;

	if (!dir) {
		ret = rts_audio_set_capture_volume(vol);
		if (ret < 0) {
			YSX_LOG(LOG_MW_ERROR, "Set audio capture volume fail\n");
			return -1;
		}
	} else {
		ret = rts_audio_set_playback_volume(vol);
		if (ret < 0) {
			YSX_LOG(LOG_MW_ERROR, "Set audio playback volume fail\n");
			return -1;
		}
	}

	return 0;
}

int QCamAudioInputOpen_ysx(QCamAudioInputAttr_aec *pAttr)
{
	int ret = 0;
	static int first_run = 1;
/*
	if (!audio_svr.svr_init)
		__init_audio_server();
*/

	ret = __audio_input_open_sanity_check(pAttr);
	if (ret < 0)
		goto exit;

	audio_svr.aec_cb = pAttr->cb;

	__set_audio_volume(pAttr->volume, 0);

	if (first_run) {
		first_run = 0;
		/* TODO check if need change samplerate*/

		audio_svr.capture_exit = 0;
		audio_svr.capture_start = 1;
		audio_svr.capture_run = 1;

		pthread_mutex_init(&audio_svr.m_capture, NULL);
		ret = pthread_create(&audio_svr.t_capture, NULL,
							__audio_capture, NULL);
		if (ret) {
			YSX_LOG(LOG_MW_ERROR, "could not create audio capture thread\n");
			goto exit;
		}
	}

	/* TODO check if need change samplereate*/

	return 0;

exit:
	YSX_LOG(LOG_MW_ERROR, "Open audio input fail\n");
	return -1;
}

int QCamAudioInputClose_ysx()
{
	/* TODO do nothing */
	return 0;
}

int QCamAudioInputStart()
{
	pthread_mutex_lock(&audio_svr.m_capture);

	audio_svr.capture_start = 1;

	pthread_mutex_unlock(&audio_svr.m_capture);

	return 0;
}

int QCamAudioInputStop()
{
	pthread_mutex_lock(&audio_svr.m_capture);

	audio_svr.capture_start = 0;

	pthread_mutex_unlock(&audio_svr.m_capture);

	return 0;
}

int QCamAudioInputSetVolume(int vol)
{
	int ret = 0;

	if ((vol < -1) || (vol > 100)) {
		YSX_LOG(LOG_MW_ERROR, "[QCamAudioInputSetVolume] sanity check fail\n");
		goto exit;
	}

	ret = __set_audio_volume(vol, 0);
	if (ret < 0)
		goto exit;

	return 0;

exit:
	return -1;
}

int QCamAudioAecEnable(int enable)
{
	int ret = 0;

	ret = __set_aec_denoise(audio_svr.aec.id, enable, 35);
	if (ret < 0)
		return -1;

	return 0;
}

void QCamAudioInputSetGain(int gain)
{
	int ret = 0;
	char cmd[64] = {'\0'};

	if ((gain < 0) || (gain > 69)) {
		YSX_LOG(LOG_MW_ERROR, "[QCamAudioInputSetGain] sanity check fail\n");
		return;
	}

	ret = access("/bin/amixer", F_OK);
	if (ret < 0) {
		YSX_LOG(LOG_MW_ERROR, "Could not found tool[amixer]\n");
		return;
	}

	sprintf(cmd, "amixer cset numid=11 %d", gain);

	system(cmd);
}

static void __get_spk_gpio(void)
{
	int ret = 0;

	spk_gpio.io = rts_io_gpio_request(SYSTEM_GPIO, GPIO_SPK);
	if (spk_gpio.io == NULL)
		goto exit;

	ret |= rts_io_gpio_set_direction(spk_gpio.io, GPIO_OUTPUT);
	ret |= rts_io_gpio_set_value(spk_gpio.io, GPIO_SPK_HIGH);
	if (ret < 0)
		goto exit;

	audio_svr.amp_stat = true;

	return;

exit:
	YSX_LOG(LOG_MW_ERROR, "SPK gpio pull high fail, spkeaker will be mute\n");
}

static void __free_spk_gpio(void)
{
	int ret = 0;

	if (spk_gpio.io != NULL) {
		rts_io_gpio_set_value(spk_gpio.io, GPIO_SPK_LOW);
		audio_svr.amp_stat = false;
		ret = rts_io_gpio_free(spk_gpio.io);
	}
	if (ret < 0)
		YSX_LOG(LOG_MW_ERROR, "Free speaker gpio fail\n");
	else
		spk_gpio.io = NULL;
}

static void __wait_for_ao_idle(void)
{
	int count = 0;

	if (!CHN_RUN(*audio_svr.pao))
		return;

	while (count < 10) {
		if (rts_av_is_idle(audio_svr.ao.id))
			count++;
		else
			count = 0;
		usleep(10000);
	}
}

static void __enable_amp_ex(bool enable)
{
	if ((spk_gpio.io != NULL) && (audio_svr.amp_stat != enable)) {
		rts_io_gpio_set_value(spk_gpio.io, enable ? GPIO_SPK_HIGH : GPIO_SPK_LOW);
		audio_svr.amp_stat = enable;
	}
}

static void __enable_amp(void)
{
	__enable_amp_ex(true);
}

static void __disable_amp(void)
{
	__enable_amp_ex(false);
}

static void *__amp_observer(void *arg)
{
	while (!audio_svr.amp_exit) {
		while (audio_svr.ao_idle_cnt < 10) {
			audio_svr.ao_idle_cnt++;
			usleep(100000);
		}
		audio_svr.ao_idle_cnt = 0;

		__wait_for_ao_idle();

		__disable_amp();
	}

	return NULL;
 }

int QCamAudioOutputOpen(QCamAudioOutputAttribute *pAttr)
{
	int ret = 0;
/*
	if (!audio_svr.svr_init)
		__init_audio_server();
*/

	audio_svr.ao_profile.audio.samplerate = pAttr->sampleRate;
	audio_svr.ao_profile.audio.bitfmt = pAttr->sampleBit;

	/* TODO check if need change samplerate*/
	ret = __bind_chn(&audio_svr.ao_resample, &audio_svr.mixer);
	if (ret < 0)
		goto exit;

	 ret = __start_chn(audio_svr.pao, RTS_SEND);
	 if (ret < 0)
		 goto exit;

	__get_spk_gpio();

	if (!audio_svr.t_amp_stat) {
		ret = pthread_create(&audio_svr.t_amp, NULL,
							__amp_observer, NULL);
		if (ret) {
			RTS_ERR("could not create amplify observer thread\n");
			goto exit;
		}
		audio_svr.t_amp_stat = true;
	}

	 return 0;

exit:
	 YSX_LOG(LOG_MW_ERROR, "Open audio output fail\n");
	 return -1;
}

int QCamAudioOutputQueryBuffer(QCamAudioOutputBufferStatus *pStat)
{
	/*TODO if playback buf len is const then could get the total buf len*/
	Mchannel *pchn = NULL;
	int bytes_1s = 0;

	if (pStat == NULL) {
		YSX_LOG(LOG_MW_ERROR, "[QCamAudioOutputQueryBuffer] sanity check fail\n");
		goto exit;
	}

	pchn = &audio_svr.ao;
	bytes_1s = pchn->sample_rate * pchn->bitfmt * pchn->channels / 8;

	pStat->total = (AD_PLY_CACHE_BUFS * AD_PLY_BUF_LEN)
						/ (bytes_1s / 1000);
	pStat->busy = (audio_svr.pao->buf_num * AD_PLY_BUF_LEN)
						/ (bytes_1s / 1000);

	return 0;

exit:
	return -1;
}

int QCamAudioOutputClose()
{
	int ret = 0;

	if (audio_svr.t_amp_stat) {
		audio_svr.amp_exit = 1;
		pthread_join(audio_svr.t_amp, NULL);
		audio_svr.t_amp_stat = false;
 	}
	__free_spk_gpio();

	ret = __stop_chn(audio_svr.pao, RTS_SEND);
	if (ret < 0)
		goto exit;

	ret = __unbind_chn(&audio_svr.ao_resample, &audio_svr.mixer);
	if (ret < 0)
		goto exit;

	return 0;

exit:
	YSX_LOG(LOG_MW_ERROR, "Close audio output fail\n");
	return -1;

}

static void __rm_buffer(void *master, struct rts_av_buffer *buf)
{
	Mchannel *pchn = (Mchannel *)master;

	RTS_SAFE_RELEASE(buf, rts_av_delete_buffer);

	if (pchn->buf_num)
		atomic_dec(&pchn->buf_num);
}

int QCamAudioOutputPlay_ysx(char *pcm_data, int len)
{
	struct rts_av_buffer *buf = NULL;
	Mchannel *pchn = NULL;
	int count = 50;
	int sent = 0;
	int ret = 0;

	audio_svr.ao_idle_cnt = 0;
	__enable_amp();

	if ((pcm_data == NULL) || (len <= 0)) {
		YSX_LOG(LOG_MW_ERROR, "[QCamAudioOutputPlay_ysx] sanity check fail\n");
		return -1;
	}

	buf = rts_av_new_buffer(len);
	if (buf == NULL) {
		YSX_LOG(LOG_MW_ERROR, "Malloc rts buffer fail\n");
		return -1;
	}

	pchn = audio_svr.pao;

	memcpy(buf->vm_addr, pcm_data, len);
	buf->bytesused = len;
	buf->timestamp = 0;
	rts_av_set_buffer_profile(buf, &audio_svr.ao_profile);

	atomic_inc(&pchn->buf_num);
	rts_av_get_buffer(buf);
	rts_av_set_buffer_callback(buf, pchn, __rm_buffer);

	while (count--) {
		do {
			if (pchn->buf_num >= AD_PLY_CACHE_BUFS) {
				usleep(5000);
				continue;
			}

			ret = rts_av_send(pchn->id, buf);
			if (ret < 0)
				break;

			sent = 1;
		} while (0);

		if (sent)
			break;

		usleep(10000);
	}

	RTS_SAFE_RELEASE(buf, rts_av_put_buffer);

	if (sent)
		return 0;

	YSX_LOG(LOG_MW_ERROR, "Audio playback send fail, ret[%d]\n", ret);
	return ret;
}

int QCamAudioOutputSetVolume(int vol)
{
	int ret = 0;

	if ((vol < -1) || (vol > 100)) {
		YSX_LOG(LOG_MW_ERROR, "[QCamAudioOutputSetVolume] sanity check fail\n");
		goto exit;
	}

	ret = __set_audio_volume(vol, 1);
	if (ret < 0)
		goto exit;

	return 0;

exit:
	return -1;

}
