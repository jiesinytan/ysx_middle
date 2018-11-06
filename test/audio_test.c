#include <qcam_audio_input.h>
#include <qcam_audio_output.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "../rts_middle_media.h"

#define audio_default_set	-1
#define audio_vol_min		0
#define inputRate_default	8*1000		//unsure the value
#define mic_volume_default	50		//unsure the value
#define inputBit_default	8		//unsure the value
#define gain_default		10		//unsure the value

#define outputRate_default	8*1000		//unsure the value
#define spk_volume_default	50		//unsure the value
#define outputBit_default	8		//unsure the value

#define buffer_total		10*1000

#define PCM_FRAME_BUF	8*1000		//how to sure the size 
#define Wr_Audio_File		"a.pcm"	//input audio file
#define Rd_Audio_File		"b.pcm" 	//output audio file

int g_exit;

void sig_handle(int sig)
{
	g_exit = 1;
}

FILE *f_wr_audio = NULL;
FILE *f_rd_audio = NULL;

void audio_cb(const struct timeval *tv, const void *pcm_buf,
	const int pcm_len, const void *spk_buf)
{
	static uint64_t count;
	int ret = 0;
	if(!f_wr_audio) {
		printf("file not exist, no space to save audio stream\n");
	}
	ret = fread(pcm_buf, sizeof(uint8_t), pcm_len, f_wr_audio);
	if (ret != pcm_len)
		printf("lost data when save data, write[%d] get[%d]\n",
			pcm_len, ret);
	printf("get pcm[%llu] frames\n", ++count);
}

int main(int argc, char *argv[])
{
	int ret = 0;
	int ch;
	uint8_t audio_in = 0, audio_out = 0;
	uint8_t enaec = 0; 
	int count = 100;
	char *pcmdata;
	uint32_t  Length = 0, pcm_count = 0;
	int rate = inputRate_default;
	int volume_set = mic_volume_default; 
	int sample_bit = inputBit_default;
	int sample_gain = gain_default;
	
	QCamAudioInputAttr_aec AudioInput;
	QCamAudioOutputAttribute AudioOutput;
	QCamAudioOutputBufferStatus BufferStat;

	memset(&AudioInput, 0, sizeof(QCamAudioInputAttr_aec));
	memset(&AudioOutput, 0, sizeof(QCamAudioOutputAttribute));
	memset(&BufferStat, 0, sizeof(QCamAudioOutputBufferStatus));

	while ((ch = getopt(argc, argv, "r:a:v:b:g:i:o:h")) != -1) {
		switch (ch) {
		case 'r':
			rate = strtol(optarg, NULL, 10);
			break;
		case 'a':
			enaec = strtol(optarg, NULL, 10);
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
		case 'h':
			printf("Usage:\n");
			printf("\t-r set sample rate:\n");
			printf("\t-v use the default para if volume -1\n");
			printf("\t-b set sample bit:8 or 16\n");
			printf("\t-g set gain:\n");			
			printf("\t-a turn on aec\n");
			printf("\t-i turn on audio_in test\n");
			printf("\t-p turn on playback\n");
			printf("\t-e turn on G711Encode\n");
			printf("\t-t turn on testing\n");
			/* fall through */
		default:
			return -1;
		}

	}

	signal(SIGTERM, sig_handle);
	signal(SIGINT, sig_handle);

	if(volume_set == audio_default_set){
		AudioInput.sampleRate = inputRate_default;
		AudioInput.sampleBit = mic_volume_default;
		AudioInput.volume = inputBit_default;
		AudioInput.cb = audio_cb;		

		AudioOutput.sampleRate = outputRate_default;
		AudioOutput.sampleBit = outputBit_default;
		AudioOutput.volume = spk_volume_default;	
	}
	else{
		if(volume_set < audio_default_set)
			volume_set = audio_vol_min;
		else if(volume_set > MIC_VOLUME_RANGE)
			volume_set = MIC_VOLUME_RANGE;

		AudioInput.sampleRate = rate;	
		AudioInput.sampleBit = sample_bit;	
		AudioInput.volume = volume_set;	
		AudioInput.cb = audio_cb;
		AudioOutput.sampleRate = rate;		
		AudioOutput.sampleBit = sample_bit;
		AudioOutput.volume = volume_set;
	}

	f_wr_audio = fopen(Wr_Audio_File, "w+");
	if (f_wr_audio == NULL) {
		printf("open wr_audio file  fail\n");
		goto out;
	}

	f_rd_audio = fopen(Rd_Audio_File, "rb");
	if (f_wr_audio == NULL) {
		printf("open rd_audio file  fail\n");
		goto out;
	}

	if (audio_in) {
		printf("Testing QcamAudio input dev mono...\n");

		ret = QCamAudioInputOpen_ysx(&AudioInput);
		if(ret){
			printf("QCamAudioInputOpen_ysx failed\n");
			goto out;
		}

		ret = QCamAudioInputSetVolume(AudioInput.volume);
		if(ret){
			printf("QCamAudioInputSetVolume failed\n");
			goto out;	
		}	

		QCamAudioInputSetGain(sample_gain);
		//ensure the enaec = 0
		ret = QCamAudioAecEnable(enaec);
		if(ret){
			printf("QCamAudioAecEnable failed\n");
			goto out;
		}	

		while(!g_exit && count--){
			ret = QCamAudioInputStart();
			printf("show the callback count= %d\n",count);	
			if(ret){
				printf("QCamAudioInputStart failed\n");
				goto out;	
			}
		}

		ret = QCamAudioInputStop();
		if(ret){
			printf("QCamAudioInputStop failed\n");
			goto out;
		}

		ret = QCamAudioInputClose_ysx();
		if(ret){
			printf("QCamAudioInputClose_ysx failed\n");
			goto out;
		}
	}

	if(audio_out){
		printf("Testing QcamAudio output dev ...\n");
/*
		fseek(f_rd_audio, 0, SEEK_END);
		Length = ftell(f_rd_audio);
		pcm_count = Length / PCM_FRAME_BUF + 1;
		printf("pcm_count = %d\n", pcm_count);
*/		
		ret = QCamAudioOutputOpen(&AudioOutput);
		if(ret){
			printf("QCamAudioOutputOpen failed\n");
			goto out;
		}

		ret = QCamAudioOutputSetVolume(AudioOutput.volume);
		if(ret){
			printf("QCamAudioOutputSetVolume failed\n");
			goto out;			
		}

		pcmdata = (char *)malloc(PCM_FRAME_BUF);
		while(!g_exit){
			if(!feof(f_rd_audio)) {
				Length = fread(pcmdata, 1, PCM_FRAME_BUF, f_rd_audio);
				if(Length != PCM_FRAME_BUF){
					printf("read the pcm frame = %d\n", pcm_count);
					g_exit = 1;
				}

				ret = QCamAudioOutputPlay_ysx(pcmdata, PCM_FRAME_BUF);
				if(ret){
					printf("QCamAudioOutputPlay_ysx failed\n");
					goto out;
				}

				ret = QCamAudioOutputQueryBuffer(&BufferStat);
				printf("bufferTotal = %d, BufferBusy = %d",
				BufferStat.total, BufferStat.busy);
				if(ret){
					printf("QCamAudioOutputQueryBuffer failed\n");
				}
				pcm_count++;
				usleep(1000);
			} 
		}
		fclose(f_rd_audio);
		free(pcmdata);
		QCamAudioOutputClose();
	}

out:
	QCamAudioInputClose_ysx();
	QCamAudioOutputClose();
	if ((f_rd_audio != NULL) ||(f_wr_audio != NULL)) {
		fclose(f_rd_audio);
		fclose(f_wr_audio);
		f_rd_audio = NULL;
		f_wr_audio = NULL;
	}
	printf("exit code %d\n", ret);
	return ret;
}