#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <unistd.h>
#include <getopt.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "common.h"

#define FREQUENCY		15500
#define RATE			44100

struct idat {
	char	*file_name;
	char	*var_name;
	float	length;
	float	strength;
};

static int write_wave(FILE *f, double *wave, size_t len, char *var_name)
{
	int ret = 0;

	BUILD_BUG_ON(sizeof(float) != sizeof(uint32_t));

	ret = fprintf(f, "static uint32_t %s[] = {\n", var_name);
	if (ret > 0) {
		for (size_t pos = 0; pos < len; pos++) {
			double dv = floor(wave[pos] * (double)32767);
			float fv = (dv / (double)32767);
			uint32_t *v = (void *)&fv;
			ret = fprintf(f, "0x%08x,\n", *v);
			if (ret < 0)
				break;
		}
	}
	if (ret > 0)
		ret = fprintf(f, "};");

	return ret > 0 ? 0 : -1;
}

static inline ssize_t wave_len(float length)
{
	return (ssize_t)(length * RATE);
}

static void fill_wave(double *wave, size_t len, float strength)
{
	const double factor = ((double)FREQUENCY * M_PI * 2) / (double)RATE;
	for (size_t i = 0; i < len; i++)
		wave[i] = sin((double)i * factor) * (double)strength;
}

static int process_input(struct idat *d)
{
	ssize_t len = wave_len(d->length);
	double *wave = NULL;
	size_t size = sizeof(wave[0]) * len;
	FILE *f = NULL;
	int ret;

	pr_info("process %s: len %zd size %zu\n",
		d->file_name, len, size);

	wave = malloc(size);
	if (!wave) {
		pr_perror("Can't allocate wave");
		return -1;
	}

	f = fopen(d->file_name, "w");
	if (!f) {
		pr_perror("Can't open %s", d->file_name);
		free(wave);
		return -1;
	}

	fill_wave(wave, len, d->strength);
	ret = write_wave(f, wave, len, d->var_name);
	if (ret < 0)
		pr_perror("Can't write %s", d->file_name);
	fclose(f);
	free(wave);

	return ret < 0 ? -1 : 0;
}

int main(int argc, char *argv[])
{
	int ret = 0;

	struct idat input[] = {
		{
			.file_name	= "src/dcf77sync/play_0205.c",
			.var_name	= "play_0205",
			.length		= 0.2,
			.strength	= 0.5,
		}, {
			.file_name	= "src/dcf77sync/play_0810.c",
			.var_name	= "play_0810",
			.length		= 0.8,
			.strength	= 1.0,
		}, {
			.file_name	= "src/dcf77sync/play_0105.c",
			.var_name	= "play_0105",
			.length		= 0.1,
			.strength	= 0.5,
		}, {
			.file_name	= "src/dcf77sync/play_0910.c",
			.var_name	= "play_0910",
			.length		= 0.9,
			.strength	= 1.0,
		}, {
			.file_name	= "src/dcf77sync/play_1010.c",
			.var_name	= "play_1010",
			.length		= 1.0,
			.strength	= 1.0,
		},
	};

	if (argc < 2) {
		pr_err("define filename to generate\n");
		return 1;
	}

	for (size_t i = 0; i < ARRAY_SIZE(input); i++) {
		if (strcmp(argv[1], input[i].file_name))
			continue;
		ret = process_input(&input[i]);
		if (ret)
			break;
		return 0;
	}

	pr_err("no match found\n");
	return 1;
}
