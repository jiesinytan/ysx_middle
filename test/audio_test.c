#include <qcam_audio_input.h>
#include <qcam_audio_output.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <unistd.h>
#include <signal.h>

#include "../rts_middle_media.h"

#define audio_vol_min		0
#define inputRate_default	(8*1000)
#define mic_volume_default	90
#define inputBit_default	16
#define gain_default		20

#define outputRate_default	(8*1000)
#define spk_volume_default	90
#define outputBit_default	16

#define buffer_total		(10*1000)

#define PCM_FRAME_BUF	320
#define Wr_Audio_File		"a.pcm"
#define Rd_Audio_File		"b.pcm"

int g_exit;

void sig_handle(int sig)
{
	g_exit = 1;
	printf("Received Ctrl+c\n");
}

FILE *f_wr_audio;
FILE *f_rd_audio;

void audio_cb(const struct timeval *tv, const void *pcm_buf,
	const int pcm_len, const void *spk_buf)
{
	static uint64_t count;
	int ret = 0;

	if (!f_wr_audio)
		printf("file not exist, no space to save audio stream\n");

	ret = fwrite(pcm_buf, sizeof(uint8_t), pcm_len, f_wr_audio);
	if (ret != pcm_len)
		printf("lost data when save data, write[%d] get[%d]\n",
			pcm_len, ret);
	if (!(++count % 100))
		printf("get pcm[%llu] frames\n", count);
}

int main(int argc, char *argv[])
{
	int ret = 0;
	int ch;
	uint8_t audio_in = 0, audio_out = 0;
	uint8_t enaec = 0;
	char *pcmdata;
	uint32_t  Length = 0, pcm_count = 0;
	int rate = inputRate_default;
	int volume_set = mic_volume_default;
	int sample_bit = inputBit_default;
	int sample_gain = gain_default;
	int o_test = 0;

	QCamAudioInputAttr_aec AudioInput;
	QCamAudioOutputAttribute AudioOutput;
	QCamAudioOutputBufferStatus BufferStat;

	memset(&AudioInput, 0, sizeof(QCamAudioInputAttr_aec));
	memset(&AudioOutput, 0, sizeof(QCamAudioOutputAttribute));
	memset(&BufferStat, 0, sizeof(QCamAudioOutputBufferStatus));

	while ((ch = getopt(argc, argv, "r:av:b:g:ioth")) != -1) {
		switch (ch) {
		case 'r':
			rate = strtol(optarg, NULL, 10);
			break;
		case 'a':
			enaec = 1;
			break;
		case 'v':
			volume_set = strtol(optarg, NULL, 10);
			break;
		case 'b':
			sample_bit = strtol(optarg, NULL, 10);
			break;
		case 'g':
			sample_gain = strtol(optarg, NULL, 10);
			break;
		case 'i':
			audio_in = 1;
			break;
		case 'o':
			audio_out = 1;
			break;
		case 't':
			o_test = 1;
			break;
		case 'h':
			printf("Usage:\n");
			printf("\t-r set sample rate:\n");
			printf("\t-v set volume\n");
			printf("\t-b set sample bit:8 or 16\n");
			printf("\t-g set gain:\n");
			printf("\t-a turn on aec\n");
			printf("\t-i turn on audio_in test\n");
			printf("\t-o turn on audio_out test\n");
			printf("\t-t turn on testing, use the default paran\n");
			/* fall through */
		default:
			return -1;
		}

	}

	signal(SIGTERM, sig_handle);
	signal(SIGINT, sig_handle);

	if (!o_test) {
		AudioInput.sampleRate = inputRate_default;
		AudioInput.sampleBit = inputBit_default;
		AudioInput.volume = mic_volume_default;
		AudioInput.cb = audio_cb;

		AudioOutput.sampleRate = outputRate_default;
		AudioOutput.sampleBit = outputBit_default;
		AudioOutput.volume = spk_volume_default;
	} else{
		AudioInput.sampleRate = rate;
		AudioInput.sampleBit = sample_bit;
		AudioInput.volume = volume_set;
		AudioInput.cb = audio_cb;
		AudioOutput.sampleRate = rate;
		AudioOutput.sampleBit = sample_bit;
		AudioOutput.volume = volume_set;
	}

	QCamAV_Context_Init();

	if (audio_in ||  enaec) {
		printf("Testing QcamAudio input dev mono...\n");

		f_wr_audio = fopen(Wr_Audio_File, "w+");
		if (f_wr_audio == NULL) {
			printf("open wr_audio file  fail\n");
			goto out;
		}

		ret = QCamAudioInputOpen_ysx(&AudioInput);
		if (ret) {
			printf("QCamAudioInputOpen_ysx failed\n");
			goto out;
		}

		ret = QCamAudioInputSetVolume(AudioInput.volume);
		if (ret) {
			printf("QCamAudioInputSetVolume failed\n");
			goto out;
		}

		QCamAudioInputSetGain(sample_gain);

		ret = QCamAudioInputStart();
		if (ret)
			printf("QCamAudioInputStart failed\n");
	}

	if (enaec) {
		ret = QCamAudioAecEnable(enaec);
		if (ret)
			printf("QCamAudioAecEnable failed\n");
	}


	if (audio_out || enaec) {
		printf("Testing QcamAudio output dev ...\n");

		f_rd_audio = fopen(Rd_Audio_File, "rb");
		if (f_rd_audio == NULL) {
			printf("open rd_audio file  fail\n");
			goto out;
		}

		ret = QCamAudioOutputOpen(&AudioOutput);
		if (ret) {
			printf("QCamAudioOutputOpen failed\n");
			goto out;
		}

		ret = QCamAudioOutputSetVolume(AudioOutput.volume);
		if (ret) {
			printf("QCamAudioOutputSetVolume failed\n");
			goto out;
		}

		pcmdata = (char *)malloc(PCM_FRAME_BUF);
		while (!g_exit && !feof(f_rd_audio)) {
			Length = fread(pcmdata, 1, PCM_FRAME_BUF, f_rd_audio);
			if (Length != PCM_FRAME_BUF) {
				printf("read the pcm frame = %d\n", pcm_count);
				g_exit = 0;
			}

			ret = QCamAudioOutputPlay_ysx(pcmdata, PCM_FRAME_BUF);
			if (ret) {
				printf("QCamAudioOutputPlay_ysx failed\n");
				goto out;
			}

			if (!(pcm_count % 100)) {
				ret = QCamAudioOutputQueryBuffer(&BufferStat);
				printf("bufferTotal = %d, BufferBusy = %d\n",
					BufferStat.total, BufferStat.busy);
				if (ret)
					printf("QueryBuffer failed\n");
			}
			pcm_count++;
			usleep(5000);
		}
		free(pcmdata);
		QCamAudioOutputClose();
	}

	if (audio_in  ||  enaec) {
		if (!(audio_out || enaec))
			sleep(20);

		printf("Stop audio input\n");
		ret = QCamAudioInputStop();
		if (ret)
			printf("QCamAudioInputStop failed\n");

		ret = QCamAudioInputClose_ysx();
		if (ret)
			printf("QCamAudioInputClose_ysx failed\n");
	}

out:
	if (f_rd_audio != NULL) {
		fclose(f_rd_audio);
		f_rd_audio = NULL;
	}

	if (f_wr_audio != NULL) {
		fclose(f_wr_audio);
		f_wr_audio = NULL;
	}

	QCamAV_Context_Release();

	printf("exit code %d\n", ret);
	return ret;
}
