#include <qcam_sys.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "../rts_middle_media.h"

#define LED_HZ	1	//not sure the default frequency
#define LED_HZ_MAX	1000 	//1KHz
#define FPath 			"/lib/firmware/isp.fw" 

FILE *f_firmware = NULL;

int g_exit;

void sig_handle(int sig)
{
	g_exit = 1;
}

void key_cb(const int status)
{
	switch(status){
		case KEY_INVALID:
			printf("The Key is unvalid.\n"); 
		break;
		case KEY_RELEASED:
			printf("The Key has been released.\n"); 
		break;
		case KEY_PRESSED:
			printf("The Key has been pressed.\n"); 
		break;
	}			
}

int main(int argc, char *argv[])
{
	int ret = 0;
	int ch;
	int status;
	int Length;
	char led_change = 0;
	char frez_change = 0;
	int mode = LED_MODE_OFF;
	int led_frez = LED_HZ;
	const char *firmware_data;
	while ((ch = getopt(argc, argv, "m:f:l:n:h")) != -1) {
		switch (ch) {
		case 'm':
			mode = strtol(optarg, NULL, 10);
			break;
		case 'f':
			led_frez = strtol(optarg, NULL, 10);
			break;	
		case 'l':
			led_change = 1;
			break;
		case 'n':
			frez_change = 1;
			break;					
		case 'h':
			printf("Usage:\n");
			printf("\t-m Set Led Mode:\n");
			printf("\t-f Set Led frequency:\n");
			printf("\t-l Change The Led Mode Test\n");
			printf("\t-n Change The frequency Test\n");			
			/* fall through */
		default:
			return -1;
		}

	}

	signal(SIGTERM, sig_handle);
	signal(SIGINT, sig_handle);

	f_firmware = fopen(FPath, "rb");
	if (f_firmware == NULL) {
		printf("open rd_audio file  fail.\n");
		goto out;
	}
	fseek(f_firmware, 0, SEEK_END);
	Length = ftell(f_firmware);
	firmware_data = (const char *)malloc(Length);

	ret = QCamFlashBurn(firmware_data);
	if(ret){
		printf("QCamFlashBurn failed.\n");
		goto out;
	}

	if(led_change){
		printf("Test LED MODE change.\n");
		for(int i = LED_MODE_OFF; i <= LED_MODE_MAX; i++){
			ret = QCamLedSet(i, led_frez);
			if(ret){
				printf("QCamLedSet MODE = [%d] failed\n",i);
				goto out;
			}
			sleep(10);
		}
	}

	if(frez_change){
		printf("Test LED frequency change.\n");
		for(int i = 1; i <= LED_MODE_MAX; i = i+10){
			ret = QCamLedSet(mode, i);
			if(ret){
				printf("QCamLedSet frequency = [%d] failed\n",i);
				goto out;
			}
			sleep(10);
		}

		for(int i = LED_HZ; i >= 0; i = i-0.1){
			ret = QCamLedSet(mode, i);
			if(ret){
				printf("QCamLedSet frequency = [%d] failed\n",i);
				goto out;
			}
			sleep(10);
		}		
	}	

	ret = QCamLedSet(mode, led_frez);
	if(ret){
		printf("QCamLedSet failed.\n");
		goto out;		
	}

	while(!g_exit){
		QCamRegKeyListener(key_cb);

		status = QCamGetKeyStatus();
		printf("-1 unvalid, 0 released, 1 pressed %d\n", status);
		status = QCamGetKey2Status();
		printf("-1 unvalid, 0 released, 1 pressed %d\n", status);

		usleep(10);
	}

out:
	free(firmware_data);
	printf("exit code %d\n", ret);
	return ret;

}