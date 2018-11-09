#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/input.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include <stddef.h>

#include <rtscamkit.h>
#include <rts_io_gpio.h>

#include <qcam_sys.h>

#include "rts_middle_media.h"


static struct button_tid g_button_tid;
static struct led_tid g_led_tid;
static int g_key_monitor_exit;
static int g_led_exit;
/* key0:BUTTON_0; key1:BUTTON_1; key2:BUTTON_2;*/
static int g_key_stat[KEY_NUM];
QCam_Key_Status_cb __key_cb;
static struct led led[LED_NUM];


static int __set_non_blocking(int fd)
{
	int flags = -1;
	int ret = 0;

	flags = fcntl(fd, F_GETFL);
	if (flags < 0)
		goto exit;

	ret = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	if (ret < 0)
		goto exit;

	return 0;

exit:
	RTS_ERR("Set key input device NONBLOCK failed\n");
	return -1;
}

static void *__get_key_value(void *arg)
{
	int fd, rc;
	struct input_event event;
	int ret = 0;

	fd = open("/dev/input/event0", O_RDWR, 0);
	if (fd < 0) {
		RTS_ERR("Open key input device fail\n");
		return NULL;
	}

	ret = __set_non_blocking(fd);
	if (ret < 0)
		goto exit;

	while (!g_key_monitor_exit) {
		rc = read(fd, &event, sizeof(event));
		if (rc < 0) {
			if ((errno == EWOULDBLOCK)
				|| (errno == EAGAIN) || (errno == EINTR)) {
				sleep(2);
				continue;
			} else {
				RTS_ERR("Read key input device fail\n");
				goto exit;
			}
		}

		if (event.type == EV_KEY) {
			switch (event.code) {
			case BTN_0:
				g_key_stat[0] = (event.value) ? KEY_PRESSED
								: KEY_RELEASED;
				break;
			case BTN_1:
				g_key_stat[1] = (event.value) ? KEY_PRESSED
								: KEY_RELEASED;
				break;
			case BTN_2:
				g_key_stat[2] = (event.value) ? KEY_PRESSED
								: KEY_RELEASED;
				break;
			}

			if ((__key_cb != NULL) && (event.code == BTN_2))
				__key_cb((const int)g_key_stat[2]);
		}

	}

exit:
	if (fd != -1) {
		close(fd);
		fd = -1;
	}

	return NULL;
}

/* static void __attribute__((constructor(AV_INIT_NO))) */
static void __init_key_monitor(void)
{
	int ret = 0;
	int i;

	g_button_tid.stat = false;

	for (i = 0; i < KEY_NUM; i++)
		g_key_stat[i] = KEY_INVALID;
	__key_cb = NULL;

	ret = pthread_create(&g_button_tid.tid, NULL, __get_key_value, NULL);
	if (ret < 0) {
		RTS_ERR("Create button monitor thread fail\n ");
		return;
	}
	g_button_tid.stat = true;

	RTS_INFO("Start key monitor\n");
}

/* static void __attribute__((destructor(AV_INIT_NO))) */
static void __release_key_monitor(void)
{
	g_key_monitor_exit = 1;

	if (g_button_tid.stat == true) {
		pthread_join(g_button_tid.tid, NULL);
		g_button_tid.stat = false;
	}
	RTS_INFO("Stop key monitor\n");
}

static void *__led_controller(void *arg)
{
	int i;

	while (!g_led_exit) {
		for (i = 0; i < LED_NUM; i++) {
			if (led[i].blink)
				rts_io_gpio_set_value(led[i].gpio, 0);
			else
				rts_io_gpio_set_value(led[i].gpio, 1);
		}

		usleep(BLINK_DUTY/2);

		for (i = 0; i < LED_NUM; i++)
			rts_io_gpio_set_value(led[i].gpio, 1);

		usleep(BLINK_DUTY/2);
	}

	return NULL;
}

static void __init_led_controller(void)
{
	int ret = 0;
	int i;

	g_led_tid.stat = false;

	for (i = 0; i < LED_NUM; i++) {
		led[i].gpio = NULL;
		led[i].blink = 0;
	}

	led[LED_YELLOW].gpio = rts_io_gpio_request(SYSTEM_GPIO,
							GPIO_LED_YELLOW);
	if (led[LED_YELLOW].gpio == NULL)
		goto exit;
	rts_io_gpio_set_direction(led[LED_YELLOW].gpio, GPIO_OUTPUT);

	led[LED_BLUE].gpio = rts_io_gpio_request(SYSTEM_GPIO, GPIO_LED_BLUE);
	if (led[LED_BLUE].gpio == NULL)
		goto exit;
	rts_io_gpio_set_direction(led[LED_BLUE].gpio, GPIO_OUTPUT);

	/* power off led on start */
	for (i = 0; i < LED_NUM; i++)
		rts_io_gpio_set_value(led[i].gpio, 1);

	ret = pthread_create(&g_led_tid.tid, NULL, __led_controller, NULL);
	if (ret < 0) {
		RTS_ERR("Create led controller thread fail\n ");
		goto exit;
	}
	g_led_tid.stat = true;

	RTS_INFO("Start led controller\n");

	return;
exit:
	RTS_ERR("Request led gpio fail\n");
}

static void __release_led_controller(void)
{
	g_led_exit = 1;

	if (g_led_tid.stat == true) {
		pthread_join(g_led_tid.tid, NULL);
		g_led_tid.stat = false;
	}

	if (led[LED_YELLOW].gpio != NULL)
		rts_io_gpio_free(led[LED_YELLOW].gpio);

	if (led[LED_BLUE].gpio != NULL)
		rts_io_gpio_free(led[LED_BLUE].gpio);

	RTS_INFO("Stop key monitor\n");
}

void __init_sys_daemon(void)
{
	__init_key_monitor();
	__init_led_controller();
}

void __release_sys_daemon(void)
{
	__release_key_monitor();
	__release_led_controller();
}

int QCamLedSet(QCAM_LED_MODE mode, int blink)
{
	switch (mode) {
	case LED_MODE_OFF:
		break;
	case LED_MODE_YELLOW:
		/* TODO */
		led[LED_YELLOW].blink = blink;
	case LED_MODE_GREEN:
		break;
	case LED_MODE_BLUE:
		led[LED_BLUE].blink = blink;
		break;
	case LED_MODE_RED:
		break;
	case LED_MODE_NORMAL_MAX:
		break;
	case LED_MODE_GREEN_Q:
		break;
	case LED_MODE_BLUE_Q:
		break;
	case LED_MODE_RED_Q:
		break;
	case LED_MODE_MAX:
		break;
	default:
		break;
	}

	return 0;
}

int QCamGetKeyStatus()
{
	return g_key_stat[0];
}

int QCamGetKey2Status()
{
	return g_key_stat[1];
}

void QCamRegKeyListener(QCam_Key_Status_cb cb)
{
	__key_cb = cb;
}

int QCamFlashBurn(const char *firmwarePath)
{
	/*TODO*/
	return 0;
}
