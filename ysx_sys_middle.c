#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/input.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include <stddef.h>
#include <inttypes.h>
#include <endian.h>
#include <linux/types.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <mtd/mtd-user.h>
#include <mtd_swab.h>
#include <linux/watchdog.h>
#include <sys/ioctl.h>

#include <rtscamkit.h>

#include <qcam_sys.h>

#include "rts_middle_media.h"


static struct button_tid g_button_tid;
static int g_key_monitor_exit;
/* key0:BUTTON_0; key1:BUTTON_1; key2:BUTTON_2;*/
static int g_key_stat[KEY_NUM];
QCam_Key_Status_cb __key_cb;

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
	YSX_LOG(LOG_MW_ERROR, "Set key input device NONBLOCK failed\n");
	return -1;
}

static void *__get_key_value(void *arg)
{
	int fd, rc;
	struct input_event event;
	int ret = 0;

	fd = open("/dev/input/event0", O_RDWR, 0);
	if (fd < 0) {
		YSX_LOG(LOG_MW_ERROR, "Open key input device fail\n");
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
				usleep(100000);
				continue;
			} else {
				YSX_LOG(LOG_MW_ERROR, "Read key input device fail\n");
				goto exit;
			}
		}

		if (event.type == EV_KEY) {
			switch (event.code) {
			case BTN_0:
				g_key_stat[0] = (event.value) ? QCAM_KEY_PRESSED
								: QCAM_KEY_RELEASED;
				break;
			case BTN_1:
				g_key_stat[1] = (event.value) ? QCAM_KEY_PRESSED
								: QCAM_KEY_RELEASED;
				break;
			case BTN_2:
				g_key_stat[2] = (event.value) ? QCAM_KEY_PRESSED
								: QCAM_KEY_RELEASED;
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
		g_key_stat[i] = QCAM_KEY_INVALID;
	__key_cb = NULL;

	ret = pthread_create(&g_button_tid.tid, NULL, __get_key_value, NULL);
	if (ret < 0) {
		YSX_LOG(LOG_MW_ERROR, "Create button monitor thread fail\n ");
		return;
	}
	g_button_tid.stat = true;

	YSX_LOG(LOG_MW_INFO, "Start key monitor\n");
}

/* static void __attribute__((destructor(AV_INIT_NO))) */
static void __release_key_monitor(void)
{
	g_key_monitor_exit = 1;

	if (g_button_tid.stat == true) {
		pthread_join(g_button_tid.tid, NULL);
		g_button_tid.stat = false;
	}
	YSX_LOG(LOG_MW_INFO, "Stop key monitor\n");
}

static struct flock *file_lock(short type, short whence)
{
	static struct flock ret;

	ret.l_type = type;
	ret.l_start = 0;
	ret.l_whence = whence;
	ret.l_len = 0;
	ret.l_pid = getpid();

	return &ret;
}

/*OK:return 0;FAILED:return -1*/
static int __pwm_write(char *dev, char *data, int nbytes)
{
	int fd;
	int ret;

	fd = open(dev, O_WRONLY);
	if (fd < 0) {
		YSX_LOG(LOG_MW_ERROR, "Can't Open '%s'\n", dev);
		return -1;
	}

	fcntl(fd, F_SETLKW, file_lock(F_WRLCK, SEEK_SET));
	ret = write(fd, data, nbytes);
	if (ret < 0) {
		YSX_LOG(LOG_MW_ERROR, "PWM Write failed.\n");
		return -1;
	}

	fcntl(fd, F_SETLKW, file_lock(F_UNLCK, SEEK_SET));
	close(fd);

	return 0;
}

static int __pwm_request(char val, int pwm)
{
	int ret;
	char data[2];
	char dev[64];

	snprintf(data, sizeof(data), "%d", val);
	snprintf(dev, sizeof(dev), "%s%d/request", PWM_SETTING, pwm);

	ret = __pwm_write(dev, data, 2);
	if (ret) {
		YSX_LOG(LOG_MW_ERROR, "pwm[%d] request failed.\n", pwm);
		return ret;
	}

	return ret;
}

static int __pwm_time( int pwm, int duty, int period)
{
	int ret;
	char data[11];
	char dev[64];

	snprintf(data, sizeof(data), "%d", period);
	snprintf(dev, sizeof(dev), "%s%d/period_ns", PWM_SETTING, pwm);
	ret = __pwm_write(dev, data, 11);
	if (ret) {
		YSX_LOG(LOG_MW_ERROR, "pwm[%d] write period_ns failed.\n", pwm);
		return ret;
	}

	snprintf(data, sizeof(data), "%d", duty);
	snprintf(dev, sizeof(dev), "%s%d/duty_ns", PWM_SETTING, pwm);
	ret = __pwm_write(dev, data, 11);
	if (ret) {
		YSX_LOG(LOG_MW_ERROR, "pwm[%d] write duty_ns failed.\n", pwm);
		return ret;
	}

	return ret;
}

static int __pwm_enable(int val, int pwm)
{
	int ret;
	char data[2];
	char dev[64];

	snprintf(data, sizeof(data), "%d", val);
	snprintf(dev, sizeof(dev), "%s%d/enable", PWM_SETTING, pwm);

	ret = __pwm_write(dev, data, 2);
	if (ret < 0) {
		YSX_LOG(LOG_MW_ERROR, "pwm[%d] write period_ns failed.\n", pwm);
		return -1;
	}

	return 0;
}

static int __init_led_controller(void)
{
	int ret = 0;

	ret = __pwm_request(PWM_REQUEST, PWM_LED_YELLOW);
	if (ret)
		goto exit;

	ret = __pwm_request(PWM_REQUEST, PWM_LED_BLUE);
	if (ret)
		goto exit;

	YSX_LOG(LOG_MW_INFO, "Start led controller\n");

	return 0;
exit:
	YSX_LOG(LOG_MW_ERROR, "Request led pwm fail\n");
	return -1;
}

static int __pwm_control(struct led *pwm_led)
{
	int ret = 0;

	if (pwm_led->blink == LED_STATUS_OFF) {
		ret = __pwm_enable(PWM_DISABLE, pwm_led->pwm);
		if (ret < 0)
			goto exit;
	} else {
		if (pwm_led->blink == LED_STATUS_ON) {
			ret = __pwm_time(pwm_led->pwm, 0, BLINK_PERIOD);
			if (ret < 0)
				goto exit;
		} else if (pwm_led->blink == LED_STATUS_BLINK) {
			ret = __pwm_time(pwm_led->pwm, BLINK_PERIOD / 2,
								BLINK_PERIOD);
			if (ret < 0)
				goto exit;
		}

		ret = __pwm_enable(PWM_ENABLE, pwm_led->pwm);
		if (ret < 0)
			goto exit;
	}

	return 0;

exit:
	YSX_LOG(LOG_MW_ERROR, "Set led [pwm] status failed\n");

	return -1;
}

static void __release_led_controller(void)
{
	int ret = 0;

	ret = __pwm_enable(PWM_DISABLE, PWM_LED_YELLOW);
	if (ret)
		YSX_LOG(LOG_MW_ERROR, "disable the pwm[%d] failed\n", PWM_LED_YELLOW);

	ret = __pwm_enable(PWM_DISABLE, PWM_LED_BLUE);
	if (ret)
		YSX_LOG(LOG_MW_ERROR, "disable the pwm[%d] failed\n", PWM_LED_BLUE);

	ret = __pwm_request(PWM_FREE, PWM_LED_YELLOW);
	if (ret)
		YSX_LOG(LOG_MW_ERROR, "free the pwm[%d] failed\n", PWM_LED_YELLOW);

	ret = __pwm_request(PWM_FREE, PWM_LED_BLUE);
	if (ret)
		YSX_LOG(LOG_MW_ERROR, "free the pwm[%d] failed\n", PWM_LED_BLUE);

	YSX_LOG(LOG_MW_INFO, "Stop led controller\n");

	return;
}

int Ysx_Led_Init(void)
{
	return __init_led_controller();
}

int Ysx_Led_Uninit(void)
{
	__release_led_controller();

	return 0;
}

void __init_sys_daemon(void)
{
	__init_key_monitor();
}

void __release_sys_daemon(void)
{
	__release_key_monitor();
}

int QCamLedSet(QCAM_LED_MODE mode, QCAM_LED_STATUS blink, int strength)
{
	struct led pwm_led;
	int ret = 0;

	if ((blink < LED_STATUS_OFF) || (blink > LED_STATUS_BLINK))
		goto exit;

	switch (mode) {
	case LED_MODE_OFF:
		pwm_led.pwm = PWM_LED_YELLOW;
		pwm_led.blink = LED_STATUS_OFF;
		ret = __pwm_control(&pwm_led);
		if (ret) {
			YSX_LOG(LOG_MW_ERROR, "QCamLedSet LED[%d] failed\n", LED_YELLOW);
			return ret;
		}

		pwm_led.pwm = PWM_LED_BLUE;
		pwm_led.blink = LED_STATUS_OFF;
		ret = __pwm_control(&pwm_led);
		if (ret) {
			YSX_LOG(LOG_MW_ERROR, "QCamLedSet LED[%d] failed\n", LED_BLUE);
			return ret;
		}

		break;
	case LED_MODE_ON:
		pwm_led.pwm = PWM_LED_YELLOW;
		pwm_led.blink = LED_STATUS_ON;
		ret = __pwm_control(&pwm_led);
		if (ret) {
			YSX_LOG(LOG_MW_ERROR, "QCamLedSet LED[%d] failed\n", LED_YELLOW);
			return ret;
		}

		pwm_led.pwm = PWM_LED_BLUE;
		pwm_led.blink = LED_STATUS_ON;
		ret = __pwm_control(&pwm_led);
		if (ret) {
			YSX_LOG(LOG_MW_ERROR, "QCamLedSet LED[%d] failed\n", LED_BLUE);
			return ret;
		}

		break;
	case LED_MODE_GREEN:
		pwm_led.pwm = PWM_LED_YELLOW;
		pwm_led.blink = blink;
		ret = __pwm_control(&pwm_led);
		if (ret) {
			YSX_LOG(LOG_MW_ERROR, "QCamLedSet LED[%d] failed\n", LED_YELLOW);
			return ret;
		}
		break;
	case LED_MODE_BLUE:
		pwm_led.pwm = PWM_LED_BLUE;
		pwm_led.blink = blink;
		ret = __pwm_control(&pwm_led);
		if (ret) {
			YSX_LOG(LOG_MW_ERROR, "QCamLedSet LED[%d] failed\n", LED_BLUE);
			return ret;
		}
		break;
	case LED_MODE_GB:
		pwm_led.pwm = PWM_LED_YELLOW;
		pwm_led.blink = blink;
		ret = __pwm_control(&pwm_led);
		if (ret) {
			YSX_LOG(LOG_MW_ERROR, "QCamLedSet LED[%d] failed\n", LED_YELLOW);
			return ret;
		}

		pwm_led.pwm = PWM_LED_BLUE;
		pwm_led.blink = blink;
		ret = __pwm_control(&pwm_led);
		if (ret) {
			YSX_LOG(LOG_MW_ERROR, "QCamLedSet LED[%d] failed\n", LED_BLUE);
			return ret;
		}
		break;
	default:
		YSX_LOG(LOG_MW_ERROR, "LED mode unkonw\n");
		goto exit;
	}

	return 0;

exit:
	YSX_LOG(LOG_MW_ERROR, "[QCamLedSet] sanity check fail\n");

	return -1;
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

static int global_check(void)
{
	const char *file = "/sys/class/mtd/mtd0/name";
	const char *mtd0 = "global";
	char buf[30];
	int fd, rd;
	char name[20];

	fd = open(file, O_RDONLY | O_CLOEXEC);
	if (fd == -1) {
		YSX_LOG(LOG_MW_ERROR, "open %s failed!\n", file);
		return -1;
	}

	rd = read(fd, buf, 30);
	if (rd == -1) {
		YSX_LOG(LOG_MW_ERROR, "cannot read \"%s\"\n", file);
		goto out_error;
	}
	if (rd == 30) {
		YSX_LOG(LOG_MW_ERROR, "contents of \"%s\" is too long\n", file);
		goto out_error;
	}

	if (sscanf(buf, "%s\n", name) != 1) {
		YSX_LOG(LOG_MW_ERROR, "cannot read name from \"%s\"\n", file);
		goto out_error;
	}

	if (strcmp(mtd0, name)) {
		YSX_LOG(LOG_MW_ERROR, "mtd0 is not global\n");
		goto out_error;
	}

	if (close(fd)) {
		YSX_LOG(LOG_MW_ERROR, "close failed on \"%s\"", file);
		return -1;
	}

	return 0;

out_error:
	close(fd);
	return -1;
}

static void section_head_dump(const struct section_head *head)
{
	YSX_LOG(LOG_MW_INFO, "section: %08x %08x%016llx\n",
		head->magic, head->reserved1, head->reserved2);
	YSX_LOG(LOG_MW_INFO, "\t  %016llx %016llx\n", head->burnaddr, head->burnlen);
}

static int section_head_init(struct section_head *head, const void *data)
{
	const struct section_head *h = data;

	memset(head, 0, sizeof(*head));
	head->magic = be32_to_cpu(h->magic);
	if (head->magic == MTD_MAGIC_FEOF)
		return 0;
	head->reserved1 = be32_to_cpu(h->reserved1);
	head->reserved2 = be64_to_cpu(h->reserved2);
	head->burnaddr = be64_to_cpu(h->burnaddr);
	head->burnlen = be64_to_cpu(h->burnlen);
	section_head_dump(head);
	return 0;
}

static int section_head_valid(const struct section_head *head)
{
	if (head->burnaddr + head->burnlen > FLASH_SIZE) {
		YSX_LOG(LOG_MW_ERROR, "burn length out of range\n");
		return -1;
	}

	return 0;
}

static void section_tail_dump(const struct section_tail *tail)
{
	YSX_LOG(LOG_MW_INFO, "checksum %08x\n", tail->checksum);
}

static int section_tail_init(struct section_tail *tail, const void *data)
{
	const struct section_tail *t = data;

	tail->checksum = be32_to_cpu(t->checksum);
	section_tail_dump(tail);
	return 0;
}

void neu_data_free(struct neu_data *data)
{
	free(data->cache);
	data->cache = NULL;
}

static struct neu_data *neu_data_alloc(const char *filename)
{
	struct neu_data *data;
	struct stat st;

	data = calloc(1, sizeof(*data));
	if (!data)
		return NULL;

	memset(data, 0, sizeof(*data));

	data->filename = filename;
	data->burn_size = 1;
	if (data->filename) {
		if (stat(filename, &st) != 0) {
			YSX_LOG(LOG_MW_ERROR, "%s not exist\n", data->filename);
			neu_data_free(data);
			return NULL;
		}
		data->burn_size += st.st_size;
	}
	return data;
}

static int neu_burn_pass(const struct neu_data *data)
{
	return data->burn_status == NEU_STAT_SUCCESS;
}

static int cached_feof(struct neu_cache *cache)
{
	return cache->head.magic == MTD_MAGIC_FEOF;
}

static int align_ops_unit(int len)
{
	if (len % MTD_DEV_OPS_UNIT)
		return (len + MTD_DEV_OPS_UNIT) & ~(MTD_DEV_OPS_UNIT - 1);
	return len;
}

static int mtd_erase_node(const char *name, __u64 offset, __u64 len)
{
	int fd;
	struct erase_info_user erase = {
		.start = offset,
		.length = len
	};

	if (offset % MTD_DEV_OPS_UNIT || len % MTD_DEV_OPS_UNIT) {
		YSX_LOG(LOG_MW_ERROR, "erase(0x%llx, 0x%llx) not aligned\n", offset, len);
		return -1;
	}

	fd = open(name, O_RDWR);
	if (fd < 0)
		return -1;

	if (ioctl(fd, MEMERASE, &erase) < 0)
		goto close_dev;

	close(fd);
	return 0;
close_dev:
	close(fd);
	return -1;
}

static int __mtd_write_mtd0(const char *name, void *data, __u64 offset, __u64 len)
{
	int fd;

	fd = open(name, O_WRONLY);
	if (fd < 0) {
		YSX_LOG(LOG_MW_ERROR, "open %s failed\n", name);
		return fd;
	}

	if (lseek(fd, offset, SEEK_SET) < 0) {
		YSX_LOG(LOG_MW_ERROR, "seek %s to %lld failed\n", name, offset);
		close(fd);
		return -1;
	}

	if (write(fd, data, len) != len) {
		YSX_LOG(LOG_MW_ERROR, "write %s %lld bytes failed\n", name, len);
		close(fd);
		return -1;
	}

	close(fd);
	return 0;
}

static int neu_cache_do_flush(struct neu_data *data, int fd)
{
	struct neu_cache *cache = data->cache;
	int len, elen;

	len = cache->head.burnlen > READ_UNIT_SIZE ?
		READ_UNIT_SIZE : cache->head.burnlen;

	while (1) {
		elen = read(fd, cache->data, len);
		if (elen < 0)
			return -1;

		else if (elen == 0)
			break;

		if (mtd_erase_node("/dev/mtd0", cache->head.burnaddr,
				align_ops_unit(elen)) < 0) {
			YSX_LOG(LOG_MW_ERROR, "erase 0x%llx, 0x%x failed\n",
				cache->head.burnaddr, elen);
			return -1;
		}

		if (__mtd_write_mtd0("/dev/mtd0",
				cache->data, cache->head.burnaddr,
				align_ops_unit(elen)) < 0) {
			YSX_LOG(LOG_MW_ERROR, "write 0x%llx, 0x%x failed\n",
				cache->head.burnaddr, elen);
			return -1;
		}

		cache->head.burnaddr += elen;
		cache->head.burnlen -= elen;

		len = cache->head.burnlen > READ_UNIT_SIZE ?
			READ_UNIT_SIZE : cache->head.burnlen;
	}

	if (read(fd, cache->__tail, sizeof(struct section_tail)) < 0) {
		YSX_LOG(LOG_MW_ERROR, "read checksum error!\n");
		return -1;
	}
	return 0;
}

static __u64 sum_buffer(const unsigned char *buf, __u64 len)
{
	__u64 sum = 0;
	__u64 i = 0;

	while (i++ < len)
		sum += *buf++;

	return sum;
}

static int cache_section_header(struct neu_data *data, int fd)
{
	int len;
	struct neu_cache *cache = data->cache;

	len = read(fd, cache->__head, sizeof(struct section_head));
	if (len < 0) {
		YSX_LOG(LOG_MW_ERROR, "Read section head error!");
		return -1;
	}

	section_head_init(&cache->head, cache->__head);
	return len;
}

static int neu_cache_do_check(struct neu_data *data, int fd)
{
	struct neu_cache *cache = data->cache;
	int len, step;
	struct section_tail tail;

	len = cache->head.burnlen > READ_UNIT_SIZE ?
		READ_UNIT_SIZE : cache->head.burnlen;

	while (1) {
		step = read(fd, cache->data, len);
		if (step < 0)
			return -1;
		else if (step == 0)
			break;
		cache->tail.checksum += sum_buffer(cache->data, step);
		cache->head.burnaddr += step;
		cache->head.burnlen -= step;
		len = cache->head.burnlen > READ_UNIT_SIZE ?
			READ_UNIT_SIZE : cache->head.burnlen;
	}

	cache->tail.checksum %= NEU_CHECKSUM_ALIGN;
	cache->tail.checksum = NEU_CHECKSUM_ALIGN -
		cache->tail.checksum;
	if (read(fd, cache->__tail, sizeof(struct section_tail)) < 0) {
		YSX_LOG(LOG_MW_ERROR, "Read checksum error!\n");
		return -1;
	}

	section_tail_init(&tail, cache->__tail);

	if (cache->tail.checksum != tail.checksum) {
		YSX_LOG(LOG_MW_ERROR, "checksum 0x%x != 0x%x\n",
			cache->tail.checksum, tail.checksum);
		return -1;
	}
	cache->tail.checksum = 0;
	return 0;
}

static int neu_burn_status_set(struct neu_data *data, int status)
{
	if (data->burn_status != status)
		YSX_LOG(LOG_MW_INFO, "%d -> %d\n", data->burn_status, status);
	data->burn_status = status;
	return status;
}

static int neu_loop(struct neu_data *data,
		int (*iter)(struct neu_data *data, int file))
{
	int fd;
	int err = 0;

	if (!data->filename)
		return -1;

	fd = open(data->filename, O_RDONLY);
	if (fd < 0)
		return fd;

	if (!data->cache) {
		data->cache = calloc(1, sizeof(*data->cache));
		if (!data->cache) {
			close(fd);
			return -1;
		}
		data->cache->flush = iter;
	}

	if (iter == neu_cache_do_check)
		neu_burn_status_set(data, NEU_STAT_CHECKING);
	else if (iter == neu_cache_do_flush)
		neu_burn_status_set(data, NEU_STAT_BURNING);

	if (lseek(fd, NEU_HEADER_LEN, SEEK_SET) < 0) {
		YSX_LOG(LOG_MW_ERROR, "lseek error\n");
		err = -1;
	}

	while (1) {
		if (cache_section_header(data, fd) < 0) {
			err = -1;
			break;
		}

		if (cached_feof(data->cache)) {
			if (iter == neu_cache_do_flush)
				neu_burn_status_set(data, NEU_STAT_SUCCESS);
			err = 0;
			break;
		}

		if (section_head_valid(&data->cache->head) < 0) {
			err = -1;
			break;
		}

		if (data->cache->flush(data, fd) < 0) {
			neu_burn_status_set(data, NEU_STAT_FAILED);
			err = -1;
			break;
		}
	}

	free(data->cache);
	data->cache = NULL;
	close(fd);
	return err;
}

static int neu_burn(struct neu_data *data)
{
	if (neu_loop(data, neu_cache_do_check) < 0)
		return -1;

	if (neu_loop(data, neu_cache_do_flush) < 0)
		return -1;

	return 0;
}

static void *wdt_thread(void *arg)
{
	struct wdt_m_para *para = NULL;

	para = (struct wdt_m_para *)arg;

	while (para->livetime--) {
		sleep(1);
		ioctl(para->fd, WDIOC_KEEPALIVE, NULL);
	}

	free(para);

	return NULL;
}

/* watchdog only support 1s,2s,4s,8s livetime,
 * use pthread to realize time_x livetime(.e.g time_x = 20s) */
static int __reboot_via_wdt(int wdt_livetime)
{
	int ret = 0;
	int fd_wdt = -1;
	int wdt_option;
	struct wdt_m_para *para = NULL;
	pthread_t t_wdt;

	ret = access("/dev/watchdog", F_OK);
	if (ret < 0)
		goto exit;

	fd_wdt = open("/dev/watchdog", O_RDWR);
	if (fd_wdt < 0)
		goto exit;

	para = (struct wdt_m_para *)calloc(1, sizeof(*para));
	if (para == NULL) {
		YSX_LOG(LOG_MW_ERROR, "Malloc mem fail\n");
		goto exit;
	}

	wdt_option = 4;
	ioctl(fd_wdt, WDIOC_SETTIMEOUT, &wdt_option);

	wdt_option = WDIOS_ENABLECARD;
	ioctl(fd_wdt, WDIOC_SETOPTIONS, &wdt_option);

	para->fd = fd_wdt;
	para->livetime = wdt_livetime;

	ret = pthread_create(&t_wdt, NULL, wdt_thread, para);
	if (ret < 0) {
		wdt_option = WDIOS_DISABLECARD;
		ioctl(fd_wdt, WDIOC_SETOPTIONS, &wdt_option);
		free(para);
		YSX_LOG(LOG_MW_ERROR, "Create watchdog thread fail\n");
		goto exit;
	}

	return 0;

exit:
	YSX_LOG(LOG_MW_ERROR, "Start watchdog fail\n");
	if (fd_wdt >= 0) {
		close(fd_wdt);
		fd_wdt = -1;
	}

	return -1;
}

static uint64_t __sector_align_down(uint64_t addr)
{
	if (addr % MTD_DEV_OPS_UNIT)
		return addr & ~(MTD_DEV_OPS_UNIT - 1);

	return addr;
}

static int __flash_dump(char *partition, uint64_t offset_addr,
							void *data , int len)
{
	int fd = -1;
	int ret = 0;

	fd = open(partition, O_RDONLY);
	if (fd < 0) {
		YSX_LOG(LOG_MW_ERROR, "open %s failed\n", partition);
		return fd;
	}

	if (lseek(fd, offset_addr, SEEK_SET) < 0) {
		YSX_LOG(LOG_MW_ERROR, "seek %s to %lld failed\n",
				partition, offset_addr);
		ret = -1;
		goto exit;
	}

	if (read(fd, data, len) != len) {
		YSX_LOG(LOG_MW_ERROR, "write %s %lld bytes failed\n", partition, len);
		ret = -1;
	}

exit:
	close(fd);

	return ret;
}

static int __flash_write(char *partition, uint64_t offset_addr,
							void *data , int len)
{
	int fd = -1;
	int ret = 0;

	fd = open(partition, O_WRONLY);
	if (fd < 0) {
		YSX_LOG(LOG_MW_ERROR, "open %s failed\n", partition);
		return fd;
	}

	if (lseek(fd, offset_addr, SEEK_SET) < 0) {
		YSX_LOG(LOG_MW_ERROR, "seek %s to %lld failed\n",
				partition, offset_addr);
		ret = -1;
		goto exit;
	}

	if (write(fd, data, len) != len) {
		YSX_LOG(LOG_MW_ERROR, "write %s %lld bytes failed\n", partition, len);
		ret = -1;
		goto exit;
	}

exit:
	close(fd);

	return ret;
}

/* caution: flash erase size 64k */
static int __update_ota_flag(char *flag)
{
	int len = 0;
	int len_align = 0;
	uint64_t erase_offset_addr = 0;
	char *data = NULL;
	int ret = 0;

	len = strlen(flag);
	len_align = align_ops_unit(len);

	data = (char *)calloc(1, len_align);
	if (data == NULL) {
		YSX_LOG(LOG_MW_ERROR, "Malloc mem fail\n");
		return -1;
	}

	erase_offset_addr = __sector_align_down(OTA_FLAG_OFFSET);

	ret = __flash_dump(HCONF_PARTITION, erase_offset_addr,
							data, len_align);
	if (ret < 0) {
		YSX_LOG(LOG_MW_ERROR, "Flash dump fail\n");
		goto exit;
	}

	memset(data + (OTA_FLAG_OFFSET - erase_offset_addr),
					0xff, HCONF_MSG_MAX_LEN);
	memcpy(data + (OTA_FLAG_OFFSET - erase_offset_addr), flag, len);

	ret = mtd_erase_node(HCONF_PARTITION, erase_offset_addr, len_align);
	if (ret < 0) {
		YSX_LOG(LOG_MW_ERROR, "Flash erase fail\n");
		goto exit;
	}

	ret = __flash_write(HCONF_PARTITION, erase_offset_addr,
							data, len_align);
	if (ret < 0) {
		YSX_LOG(LOG_MW_ERROR, "Flash write fail\n");
		goto exit;
	}

	free(data);

	return 0;

exit:
	if (data != NULL)
		free(data);

	YSX_LOG(LOG_MW_ERROR, "Update OTA flag fail\n");

	return -1;
}

int QCamFlashBurn(const char *firmwarePath)
{
	struct neu_data *data;
	int ret = 0;

	ret = access(firmwarePath, F_OK);
	if (ret < 0) {
		YSX_LOG(LOG_MW_ERROR, "[QCamFlashBurn] sanity check fail\n");
		return -1;
	}

	ret = __reboot_via_wdt(UPDATE_TIME);
	if (ret < 0) {
		YSX_LOG(LOG_MW_ERROR, "Flash Brun is aborted\n");
		return -1;
	}

	if (global_check() < 0) {
		YSX_LOG(LOG_MW_ERROR, "please set global partition as mtd0!\n");
		return -1;
	}

	data = neu_data_alloc(firmwarePath);
	if (!data)
		return -1;

	ret = __update_ota_flag("OTA_FALSE");
	if (ret < 0) {
		YSX_LOG(LOG_MW_ERROR, "OTA pre mark fail\n");
		goto failed;
	}

	if (neu_burn(data) < 0)
		goto failed;

	ret = __update_ota_flag("OTA_TRUE");
	if (ret < 0)
		YSX_LOG(LOG_MW_ERROR, "OTA post mark fail\n");

	YSX_LOG(LOG_MW_ERROR, "OTA update %s pass\n", firmwarePath);

	neu_data_free(data);

	return 0;

failed:
	neu_data_free(data);

	YSX_LOG(LOG_MW_ERROR, "OTA update %s fail\n", firmwarePath);

	return -1;
}
