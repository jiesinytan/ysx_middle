#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <qcam_sys.h>

#include "../rts_middle_media.h"

#define LED_HZ	1
#define LED_HZ_MAX	1000
#define FPath		"/lib/firmware/isp.fw"

FILE *f_firmware;

int g_exit;

void sig_handle(int sig)
{
	g_exit = 1;
}

void key_cb(const int status)
{
	switch (status) {
	case KEY_INVALID:
		printf("[listener] The Key is unvalid.\n");
	break;
	case KEY_RELEASED:
		printf("[listener] The Key has been released.\n");
	break;
	case KEY_PRESSED:
		printf("[listener] The Key has been pressed.\n");
	break;
	}
}

int main(int argc, char *argv[])
{
	int ret = 0;
	int ch;
	int status;
	int Length;
	char *firmware = NULL;
	int o_key = 0;
	int o_burn = 0;
	int o_led = 0;

	__init_middleware_context();

	while ((ch = getopt(argc, argv, "b:klh")) != -1) {
		switch (ch) {
		case 'b':
			o_burn = 1;
			firmware = optarg;
			break;
		case 'k':
			o_key = 1;
			break;
		case 'l':
			o_led = 1;
			break;
		case 'h':
			printf("Usage:\n");
			printf("\t-b burn flash\n");
			printf("\t-k start button monitor\n");
			printf("\t-l led test\n");
			/* fall through */
		default:
			return -1;
		}

	}

	signal(SIGTERM, sig_handle);
	signal(SIGINT, sig_handle);

	if (o_burn) {
		f_firmware = fopen(FPath, "rb");
		if (f_firmware == NULL) {
			printf("open rd_audio file  fail.\n");
			goto out;
		}
		fseek(f_firmware, 0, SEEK_END);
		Length = ftell(f_firmware);
		firmware = (const char *)malloc(Length);

		ret = QCamFlashBurn(firmware);
		if (ret) {
			printf("QCamFlashBurn failed.\n");
			goto out;
		}
	}

	if (o_led) {
		printf("Test LED MODE change.\n");
		QCamLedSet(LED_MODE_BLUE, 1);
		QCamLedSet(LED_MODE_YELLOW, 0);
		sleep(5);
		QCamLedSet(LED_MODE_BLUE, 0);
		QCamLedSet(LED_MODE_YELLOW, 1);
		sleep(5);
	}

	if (o_key) {
		QCamRegKeyListener(key_cb);
		while (!g_exit) {
			status = QCamGetKeyStatus();
			printf("key0[%d]\n", status);
			status = QCamGetKey2Status();
			printf("key1[%d]\n", status);
			sleep(1);
		}
	}

out:
	if (firmware != NULL) {
		free(firmware);
		firmware = NULL;
	}

	__release_middleware_context();
	printf("exit code %d\n", ret);
	return ret;

}
