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
#include <qcam_sys.h>

int g_exit;

void sig_handle(int sig)
{
	g_exit = 1;
}

int main(int argc, char const *argv[])
{
        /* code */
        return 0;
}
