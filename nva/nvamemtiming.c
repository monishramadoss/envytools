/*
 * Copyright 2011 Martin Peres
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <signal.h>

#include "nva.h"
#include "nvamemtiming.h"

void signal_handler(int sig)
{
	if (sig == SIGSEGV)
		fprintf(stderr, "Received a sigsegv. Exit nicely...\n");
	else
		fprintf(stderr, "Received a termination request. Exit nicely...\n");

	system("killall X");
	exit(99);
}

void usage(int argc, char **argv)
{
	fprintf(stderr, "Usage: %s [-c cnum ...] vbios.rom timing_table_offset timing_entry perflvl\n", argv[0]);
	fprintf(stderr, "\n\nOptional args:\n");
	fprintf(stderr, "\t-c cnum: Specify the card number\n");
	fprintf(stderr, "\t-t: Generate a mmiotrace of all meaningful operations\n");
	fprintf(stderr, "\t-e: Only modify the specified entry (to be used with -v)\n");
	fprintf(stderr, "\t-v: Set the specified value to the specified entry\n");
	fprintf(stderr, "\t-b: Consider the specified entry as a bitfield and RE it\n");
	exit(-1);
}

int read_timings(struct nvamemtiming_conf *conf)
{
	uint8_t *header;

	printf("timing table at: %x\n", conf->vbios.timing_table_offset);
	header = &conf->vbios.data[conf->vbios.timing_table_offset];
	if (header[0] != 0x10) {
		fprintf(stderr, "unknow table version %x\n", header[0]);
		return -1;
	}

	if (conf->timing.entry >= header[2]) {
		fprintf(stderr, "timing entry %i is higher than count(%i)\n", conf->timing.entry, header[2]);
		return -1;
	}

	conf->vbios.timing_entry_length = header[3];

	conf->vbios.timing_entry_offset = conf->vbios.timing_table_offset + header[1] + conf->timing.entry * header[3];

	return 0;
}

int parse_cmd_line(int argc, char **argv, struct nvamemtiming_conf *conf)
{
	int c;

	conf->cnum = 0;
	conf->mmiotrace = false;
	conf->mode = MODE_AUTO;
	conf->vbios.file = NULL;

	while ((c = getopt (argc, argv, "htb:c:e:v:")) != -1)
		switch (c) {
			case 'e':
				if (conf->mode != MODE_AUTO && conf->mode != MODE_MANUAL)
					usage(argc, argv);
				conf->mode = MODE_MANUAL;
				sscanf(optarg, "%d", &conf->manual.index);
				break;
			case 'v':
				if (conf->mode != MODE_AUTO && conf->mode != MODE_MANUAL)
					usage(argc, argv);
				conf->mode = MODE_MANUAL;
				sscanf(optarg, "%d", &conf->manual.value);
				break;
			case 'b':
				if (conf->mode != MODE_AUTO && conf->mode != MODE_BITFIELD)
					usage(argc, argv);
				conf->mode = MODE_BITFIELD;
				sscanf(optarg, "%d", &conf->bitfield.index);
				break;
			case 'c':
				sscanf(optarg, "%d", &conf->cnum);
				break;
			case 't':
				conf->mmiotrace = true;
				break;
			case 'h':
				usage(argc, argv);
		}
	if (conf->cnum >= nva_cardsnum) {
		if (nva_cardsnum)
			fprintf (stderr, "No such card.\n");
		else
			fprintf (stderr, "No cards found.\n");
		return 1;
	}

	if (argc != optind + 4)
		usage(argc, argv);

	conf->vbios.file = argv[optind];
	sscanf(argv[optind + 1], "%x", &conf->vbios.timing_table_offset);
	conf->timing.entry = atoi(argv[optind + 2]);
	conf->timing.perflvl = atoi(argv[optind + 3]);

	return 0;
}

int main(int argc, char** argv)
{
	struct nvamemtiming_conf conf;
	int ret;

	signal(SIGINT, signal_handler);
	//signal(SIGTERM, signal_handler);
	signal(SIGSEGV, signal_handler);

	if (nva_init()) {
		fprintf (stderr, "PCI init failure!\n");
		return 1;
	}
	
	parse_cmd_line(argc, argv, &conf);

	/* read the vbios */
	if (!vbios_read(conf.vbios.file, &conf.vbios.data, &conf.vbios.length)) {
		fprintf(stderr, "Error while reading the vbios\n");
		return 1;
	}

	/* parse the timing table */
	ret = read_timings(&conf);
	if (ret) {
		fprintf(stderr, "read_timings failed!\n");
		return ret;
	}

	switch (conf.mode) {
	case MODE_AUTO:
		return complete_dump(&conf);
	case MODE_BITFIELD:
		return bitfield_check(&conf);
	case MODE_MANUAL:
		return manual_check(&conf);
	}
}
