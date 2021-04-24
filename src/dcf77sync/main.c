#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>

#include <pulse/pulseaudio.h>
#include <pulse/simple.h>

#include "common.h"

#include "play_0105.c"
#include "play_0205.c"
#include "play_0810.c"
#include "play_0910.c"
#include "play_1010.c"

#define SIGNAL_TIMEZONE		"Europe/Berlin"

/*
 * Each minute ends up with longest signal from play_1010,
 * thus maximal size for minute is when each second filled
 * with play_1010.
 */
#define PLAY_MINUTES		(6)
#define PLAY_SECONDS		(PLAY_MINUTES * 60)
#define PLAY_BUF_SIZE		(sizeof(play_1010) * PLAY_SECONDS)
/*
 * Each minute transmission costs 10584000 bytes, ie 176400
 * byte per second (or 176400 / 4 = 44100 floats).
 */
#define BYTES_PER_MINUTE	(10584000)
#define BYTES_PER_SECOND	(BYTES_PER_MINUTE / 60)

static size_t bcd(unsigned char *dst, unsigned int v, size_t size)
{
	static const char *map[10] = {
		[0]	= "00000000",
		[1]	= "10000000",
		[2]	= "01000000",
		[3]	= "11000000",
		[4]	= "00100000",
		[5]	= "10100000",
		[6]	= "01100000",
		[7]	= "11100000",
		[8]	= "00010000",
		[9]	= "10010000",
	};

	if (size > 4) {
		unsigned int m = v % 10;
		memcpy(&dst[0], map[m], 4);
		m = v / 10;
		memcpy(&dst[4], map[m], size - 4);
	} else {
		memcpy(dst, map[v], size);
	}
	return size;
}

static char even_parity(unsigned char *dst, size_t i, size_t j, int *acc)
{
	int v = 0;
	for (size_t pos = i; pos < j; pos++)
		v += dst[pos] == '1' ? 1 : 0;
	if (acc)
		*acc = v;
	return v % 2 ? '1' : '0';
}

static ssize_t fill_bits(unsigned char *buf, size_t buf_size,
			 struct tm *tm)
{
	size_t pos = 0, i, j;
	int acc = 0;

	// first 17 zeros bits
	memset(buf, '0', 17);
	pos += 17;

	// dst data
	buf[pos++] = tm->tm_isdst ? '1' : '0';
	buf[pos++] = tm->tm_isdst ? '0' : '1';

	// start minute opcode
	buf[pos++] = '0';
	buf[pos++] = '1';

	// minutes + parity bit
	i = pos;
	pos += bcd(&buf[pos], tm->tm_min, 7);
	j = pos;
	buf[pos++] = even_parity(buf, i, j, NULL);

	// hours + parity bit
	i = pos;
	pos += bcd(&buf[pos], tm->tm_hour, 6);
	j = pos;
	buf[pos++] = even_parity(buf, i, j, NULL);

	// day of month
	i = pos;
	pos += bcd(&buf[pos], tm->tm_mday, 6);
	j = pos;
	even_parity(buf, i, j, &acc);

	// day of week
	pos += bcd(&buf[pos], tm->tm_wday + 1, 3);
	j = pos;
	even_parity(buf, i, j, &acc);

	// month number
	pos += bcd(&buf[pos], tm->tm_mon, 5);
	j = pos;
	even_parity(buf, i, j, &acc);

	// year (within century) + parity bit for all date bits
	i = pos;
	pos += bcd(&buf[pos], tm->tm_year - 100, 8);
	j = pos;

	even_parity(buf, i, j, &acc);
	buf[pos++] = acc % 2 ? '1' : '0';

	// special "59th" second (no amplitude modulation)
	buf[pos++] = '-';

	BUG_ON(pos > buf_size);
	return pos;
}

static ssize_t generate_wave(char *play, size_t play_size, struct tm *tm)
{
	unsigned char buf[256];
	size_t pos = 0, i;

	size_t play_pos = 0;

	pos = fill_bits(buf, sizeof(buf), tm);
	BUG_ON(pos < 0);

	for (i = 0; i < pos; i++) {
		switch (buf[i]) {
		case '0':
			memcpy(&play[play_pos], play_0105, sizeof(play_0105));
			play_pos += sizeof(play_0105);
			memcpy(&play[play_pos], play_0910, sizeof(play_0910));
			play_pos += sizeof(play_0910);
			break;
		case '1':
			memcpy(&play[play_pos], play_0205, sizeof(play_0205));
			play_pos += sizeof(play_0205);
			memcpy(&play[play_pos], play_0810, sizeof(play_0810));
			play_pos += sizeof(play_0810);
			break;
		case '-':
			memcpy(&play[play_pos], play_1010, sizeof(play_1010));
			play_pos += sizeof(play_1010);
			break;
		default:
			BUG_ON(1);
			return -1;
		}
	}

	BUG_ON(play_pos > play_size);
	return play_pos;
}

static pa_simple *stream = NULL;
static char *play_buf = (void *)MAP_FAILED;
static size_t play_buf_size = PLAY_BUF_SIZE;

static void cleanup(bool flush)
{
	if (stream != NULL) {
		int err;
		if (flush)
			pa_simple_flush(stream, &err);
		pa_simple_free(stream);
	}

	if (play_buf != (void *)MAP_FAILED)
		munmap(play_buf, play_buf_size);
}

static void sighandler(int sig)
{
	cleanup(true);
	exit(1);
}

/*
 * See https://en.wikipedia.org/wiki/DCF77 for packet description.
 */
int main(int argc, char *argv[])
{
	pa_sample_spec spec = {
		.format		= PA_SAMPLE_FLOAT32,
		.channels	= 1,
		.rate		= 44100,
	};
	pa_simple *stream = NULL;
	int err = 0, ret;

	struct tm tm;
	time_t from_time, start_time, end_time;
	struct timespec req;

	ssize_t play_buf_off = -1;
	ssize_t play_buf_pos = 0;

	ssize_t off;

	if (signal(SIGINT, &sighandler) == SIG_ERR) {
		pr_perror("can't setup signal handler\n");
		return 1;
	}

	/*
	 * We precalculate waves and use float32 format so
	 * make sure the sizes are matching.
	 */
	BUILD_BUG_ON(sizeof(float) != sizeof(uint32_t));

	play_buf = (void *)mmap(NULL, play_buf_size, PROT_READ | PROT_WRITE,
				MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE,
				-1, 0);
	if (play_buf == (void *)MAP_FAILED) {
		pr_perror("can't create play buffer");
		return -1;
	}

	if (setenv("TZ", SIGNAL_TIMEZONE, 1)) {
		pr_perror("can't set timezeone");
		goto err;
	}
	tzset();

	start_time = time(NULL) + 60;

	pr_info("start time %zd\n", start_time);
	from_time = start_time + 10;
	pr_info("transmission will start in 10 seconds\n");

	if (!localtime_r(&from_time, &tm)) {
		pr_perror("can't obtain localtime");
		goto err;
	}
	off = tm.tm_sec * 176400;

	for (size_t i = 0; i < PLAY_MINUTES; i++) {
		play_buf_off = generate_wave(&play_buf[play_buf_pos],
					     play_buf_size - play_buf_pos,
					     &tm);
		BUG_ON(play_buf_off < 0);
		play_buf_pos += play_buf_off;
		from_time += 60;

		if (!localtime_r(&from_time, &tm)) {
			pr_perror("can't obtain localtime");
			goto err;
		}
	}

	stream = pa_simple_new(NULL, "dcf77sync", PA_STREAM_PLAYBACK, NULL,
			       "DCF synchro signals", &spec, NULL, NULL, &err);
	if (!stream) {
		pr_err("Can't create new stream: %d\n", err);
		goto err;
	}
	end_time = time(NULL) + 60;

	req.tv_sec = 10 - (end_time - start_time);
	req.tv_nsec = 0;

	if (req.tv_sec > 10) {
		pr_err("generating too long\n");
		goto err;
	}

	pr_info("sleep for %zd seconds\n", (size_t)req.tv_sec);
	if (nanosleep(&req, NULL)) {
		pr_perror("can't sleep\n");
		goto err;
	}
	pr_info("transmitting\n");
	ret = pa_simple_write(stream, &play_buf[off], play_buf_pos - off, &err);
	pr_info("write %d err %d\n", ret, err);

	ret = pa_simple_drain(stream, &err);
	pr_info("drain %d err %d\n", ret, err);

	cleanup(true);
	return 0;
err:
	cleanup(false);
	return 1;
}
