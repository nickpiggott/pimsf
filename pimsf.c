/******************************************************************************
  60kHz MSF Time Signal Transmitter for Raspberry Pi
    by Mark Street <marksmanuk@gmail.com>
      Stanley, Falkland Islands

   MSF Time Signal Baseband Encoder for Raspberry Pi
    by Nick Piggott <nick@piggott.eu>
    
  MSF output is modulated on pin X
******************************************************************************/

#pragma GCC diagnostic ignored "-Wstrict-aliasing"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

int verbose	= 0;
const static int TIME_WAITMIN = 3330;
const static int TIME_SKEWTX  = -13;

// I/O access
volatile unsigned *gpio;
volatile unsigned *allof7e;

#define BCM2708_PERI_BASE	0x20000000
#define GPIO_BASE	(BCM2708_PERI_BASE + 0x200000)

// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) *(gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))

#define GPIO_SET *(gpio+7)	// sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR *(gpio+10)	// clears bits which are 1 ignores bits which are 0
#define GPIO_GET *(gpio+13)	// sets   bits which are 1 ignores bits which are 0

#define ACCESS(base) *(volatile int*)((int)allof7e+base-0x7e000000)
#define SETBIT(base, bit) ACCESS(base) |= 1<<bit
#define CLRBIT(base, bit) ACCESS(base) &= ~(1<<bit)

struct MSF {
	int a;
	int b;	
};

void nsleep(unsigned long int period)
{
	struct timespec ts;
	ts.tv_sec = period / 1000000;
	ts.tv_nsec = (period % 1000000) * 1000;
	while (nanosleep(&ts, &ts) && errno == EINTR);
}

void setup_gpio()
{
    int mem_fd;

    /* Open /dev/mem */
    if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC)) < 0)
	{
        printf("Failed to open /dev/mem\n");
        exit(-1);
    }
    
    allof7e = (unsigned *)mmap(
                  NULL,
                  0x01000000,		// Len
                  PROT_READ|PROT_WRITE,
                  MAP_SHARED,
                  mem_fd,
                  BCM2708_PERI_BASE	// Base
              );

    if ((int)allof7e == -1)
		exit(-1);

    gpio = allof7e + 128*(4*1024);

    // Configure GPIO4 AS OUTPUT
    INP_GPIO(4);
    OUT_GPIO(4);

    close(mem_fd);
}

void clock_startstop(int state)
{
	if (state)
		GPIO_SET = 1 << 4;
	else
		GPIO_CLR = 1 << 4;
}

void encode_timecode(struct MSF *msf, time_t t_now)
{
	int bcd[] = { 80, 40, 20, 10, 8, 4, 2, 1 };
	memset(msf, 0, 60 * sizeof(struct MSF));	// Zero all values

	t_now += 60;	// Encode for following minute
	struct tm *tm_now = localtime(&t_now);
	if (verbose)
		printf("%s() %s", __func__, asctime(tm_now));

	// Year 17-24
    int temp = tm_now->tm_year - 100;	// Year from 1900
    int sum = 0, bcdindex = 0;
    for (int i=17; i<=24; i++)
	{
        if (temp >= bcd[bcdindex])
		{
            msf[i].a = 1;
            sum++;
            temp -= bcd[bcdindex];
		}
        bcdindex++;
	}

    if (!(sum % 2))		// 17A-24A
        msf[54].b = 1;

	// Month 25-29
    temp = tm_now->tm_mon + 1;	// tm_mon 0-11
	bcdindex = 3;	// 10
    sum = 0;
    for (int i=25; i<=29; i++)
	{
        if (temp >= bcd[bcdindex])
		{
            msf[i].a = 1;
            sum++;
            temp -= bcd[bcdindex];
		}
        bcdindex++;
	}

	// Day 30-35
    temp = tm_now->tm_mday;	// tm_day 1-31
	bcdindex = 2;	// 20
    for (int i=30; i<=35; i++)
	{
        if (temp >= bcd[bcdindex])
		{
            msf[i].a = 1;
            sum++;
            temp -= bcd[bcdindex];
		}
        bcdindex++;
	}

    if (!(sum % 2))		// 25A-35A
        msf[55].b = 1;

	// Day of Week 36-38
    temp = tm_now->tm_wday;	// tm_wday 0-6 Sunday
	bcdindex = 5;	// 04
    sum = 0;
    for (int i=36; i<=38; i++)
	{
        if (temp >= bcd[bcdindex])
		{
            msf[i].a = 1;
            sum++;
            temp -= bcd[bcdindex];
		}
        bcdindex++;
	}

	if (!(sum % 2))		// 36A-38A
		msf[56].b = 1;

	// Hour 39-44
    temp = tm_now->tm_hour;	// tm_hour 0-23
	bcdindex = 2;	// 20
    sum = 0;
    for (int i=39; i<=44; i++)
	{
        if (temp >= bcd[bcdindex])
		{
            msf[i].a = 1;
            sum++;
            temp -= bcd[bcdindex];
		}
        bcdindex++;
	}

	// Minute 45-51
    temp = tm_now->tm_min;	// tm_min 0-59
	bcdindex = 1;	// 40
    for (int i=45; i<=51; i++)
	{
        if (temp >= bcd[bcdindex])
		{
            msf[i].a = 1;
            sum++;
            temp -= bcd[bcdindex];
		}
        bcdindex++;
	}

	if (!(sum % 2))		// 39A-51A
		msf[57].b = 1;

	// DST 58
	msf[58].b = tm_now->tm_isdst;

	// A bits 53-58 are always 1
	for (int i=53; i<=58; i++)
		msf[i].a = 1;
}

void key(int code, int offset=0)
{
	// printf("%s() Keying code %02X\n", __func__, code);

	// Minute marker
	if (code == 0xff)
	{
		clock_startstop(1);
		nsleep(500*1000);
		clock_startstop(0);
		nsleep((500+offset)*1000);	// Apply timing offset
	}

	// Code 00
	if (code == 0x00)
	{
		clock_startstop(1);
		nsleep(100*1000);
		clock_startstop(0);
		nsleep(900*1000);
	}

	// Code 01
	if (code == 0x01)
	{
		clock_startstop(1);
		nsleep(100*1000);
		clock_startstop(0);
		nsleep(100*1000);
		clock_startstop(1);
		nsleep(100*1000);
		clock_startstop(0);
		nsleep(700*1000);
	}

	// Code 10
	if (code == 0x10)
	{
		clock_startstop(1);
		nsleep(200*1000);
		clock_startstop(0);
		nsleep(800*1000);
	}

	// Code 11
	if (code == 0x11)
	{
		clock_startstop(1);
		nsleep(300*1000);
		clock_startstop(0);
		nsleep(700*1000);
	}
}

void send_timecode()
{
	MSF timecode[60];

	// Precision clock:
	struct tm *tm_now;
	struct timeval tv;
	char buffer[36];

	gettimeofday(&tv, NULL);

	while (true)
	{
		// Align with system clock:
		gettimeofday(&tv, NULL);
		tm_now = localtime(&tv.tv_sec);
		strftime(buffer, 36, "%A %b %d %Y %H:%M:%S", tm_now);
		printf("Timecode starting %s.%06ld\n", buffer, tv.tv_usec);

		// Align to minute boundary:
		if (tm_now->tm_sec != 0 || (tm_now->tm_sec == 0 && (tv.tv_usec/1000.0) > 250))
		{
			unsigned long int delta = 60000000 - ((tm_now->tm_sec*1000000) + tv.tv_usec);
			printf("Waiting %.3f seconds for clock alignment\n", delta/1000000.0); 
			if (delta > TIME_WAITMIN)
				nsleep(delta - TIME_WAITMIN);
			continue;
		}

		// Encode a new message for the following minute:
		encode_timecode(timecode, tv.tv_sec);

		// Apply time correction should we lag system time:
		int offset = TIME_SKEWTX - (tv.tv_usec/1000);
		if (verbose)
			printf("Time delta %ld us, Applying offset %d us\n", tv.tv_usec, offset);
		
		// Transmit message over 60s period:
		key(0xff, offset);
		for (int i=1; i<60; i++)
		{
			// printf("  Bit: %02d   A:%d B:%d\n", i, timecode[i].a, timecode[i].b);
			key((timecode[i].a << 4) + timecode[i].b);
		}

		// Sending complete
		gettimeofday(&tv, NULL);
		if (verbose)
		{
			tm_now = localtime(&tv.tv_sec);
			strftime(buffer, 36, "%A %b %d %Y %H:%M:%S", tm_now);
			printf("Timecode finished %s.%06ld\n", buffer, tv.tv_usec);
		}
	}
}

void signal_handler(int signo)
{
	if (signo == SIGINT)
	{
		printf("\nSIGINT received, shutting down\n");
		clock_startstop(1);
		exit(0);
	}
}

int main(int argc, char **argv)
{

	if (signal(SIGINT, signal_handler) == SIG_ERR)
		printf("Error! Unable to catch SIGINT\n");

	int args;
	while ((args = getopt(argc, argv, "vset:")) != EOF)
	{
		switch(args)
		{
			case 'v':
				verbose = 1;
				break;
			default:
				fprintf(stderr, "Usage: pimsf [options]\n" \
					"\t-v Verbose\n"
				);
				clock_startstop(1);
				return 1;
		}
	}
        setup_gpio();
	clock_startstop(1);
	send_timecode();
	clock_startstop(1);
	return 0;
}

