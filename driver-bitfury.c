/*
 * device-bitfury.c - device functions for Bitfury chip/board library
 *
 * Copyright (c) 2013 bitfury
 * Copyright (c) 2013 legkodymov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
*/

#include "miner.h"
#include <unistd.h>
#include <sha2.h>
#include "libbitfury.h"
#include "util.h"

#define GOLDEN_BACKLOG 5

struct device_drv bitfury_drv;

// Forward declarations
static void bitfury_disable(struct thr_info* thr);
static bool bitfury_prepare(struct thr_info *thr);
int calc_stat(time_t * stat_ts, time_t stat, struct timeval now);
double shares_to_ghashes(int shares, int seconds);

static void bitfury_detect(void)
{
	int chip_n;
	int i;
	struct cgpu_info *bitfury_info;
	applog(LOG_INFO, "INFO: bitfury_detect");
	chip_n = libbitfury_detectChips(bitfury_info->devices);
	if (!chip_n) {
		applog(LOG_WARNING, "No Bitfury chips detected!");
		return;
	} else {
		applog(LOG_WARNING, "BITFURY: %d chips detected!", chip_n);
	}
//	chip_n = 1;
//	send_shutdown(1);
//	send_shutdown(2);
//	send_shutdown(3);

	bitfury_info = calloc(1, sizeof(struct cgpu_info));
	bitfury_info->drv = &bitfury_drv;
	bitfury_info->threads = 1;
	bitfury_info->chip_n = chip_n;
	add_cgpu(bitfury_info);
	exit(0);
}

static uint32_t bitfury_checkNonce(struct work *work, uint32_t nonce)
{
	applog(LOG_INFO, "INFO: bitfury_checkNonce");
}

static int64_t bitfury_scanHash(struct thr_info *thr)
{
	static struct bitfury_device *devices; // TODO Move somewhere to appropriate place
	int chip_n;
	int chip;
	uint64_t hashes = 0;
	static time_t short_out_t;
	static time_t long_out_t;
	struct timeval now;
	unsigned char line[256];
	int short_stat = 6;
	int long_stat = 600;
	static first = 0; //TODO Move to detect()
	int i;

	devices = thr->cgpu->devices;
	chip_n = thr->cgpu->chip_n;

	if (!first) {
		devices[0].osc6_bits = 54;
		devices[1].osc6_bits = 52;
		devices[2].osc6_bits = 52;
		devices[3].osc6_bits = 54;
		devices[4].osc6_bits = 54;
		devices[5].osc6_bits = 54;
		devices[6].osc6_bits = 54;
		devices[7].osc6_bits = 52;
		for (i = 0; i < chip_n; i++) {
			send_reinit(i, devices[i].osc6_bits);
			printf("AAA send_freq: %d\n", devices[i].osc6_bits);
		}
	}
	first = 1;

	for (chip = 0; chip < chip_n; chip++) {
		devices[chip].job_switched = 0;
		if(!devices[chip].work) {
			devices[chip].work = get_queued(thr->cgpu);
			if (devices[chip].work == NULL) {
				return 0;
			}
			work_to_payload(&(devices[chip].payload), devices[chip].work);
		}
	}

	libbitfury_sendHashData(devices, chip_n);
	nmsleep(5);

	cgtime(&now);
	chip = 0;
	for (;chip < chip_n; chip++) {
		if (devices[chip].job_switched) {
			int i,j;
			int *res = devices[chip].results;
			struct work *work = devices[chip].work;
			struct work *owork = devices[chip].owork;
			struct work *o2work = devices[chip].o2work;
			i = devices[chip].results_n;
			for (j = i - 1; j >= 0; j--) {
				if (owork) {
					submit_nonce(thr, owork, bswap_32(res[j]));
					devices[chip].stat_ts[devices[chip].stat_counter++] =
						now.tv_sec;
					if (devices[chip].stat_counter == BITFURY_STAT_N) {
						devices[chip].stat_counter = 0;
					}
				}
				if (o2work) {
					// TEST
					//submit_nonce(thr, owork, bswap_32(res[j]));
				}
			}
			if (devices[chip].old_nonce && o2work) {
					submit_nonce(thr, o2work, bswap_32(devices[chip].old_nonce));
					i++;
			}
			if (devices[chip].future_nonce) {
					submit_nonce(thr, work, bswap_32(devices[chip].future_nonce));
					i++;
			}

			if (o2work)
				work_completed(thr->cgpu, o2work);

			devices[chip].o2work = devices[chip].owork;
			devices[chip].owork = devices[chip].work;
			devices[chip].work = NULL;
			hashes += 0xffffffffull * i;
		}
	}

	if (now.tv_sec - short_out_t > short_stat) {
		int shares_first = 0, shares_last = 0, shares_total = 0;
		sprintf(line, "SHORT stat %ds: ", short_stat);
		for (chip = 0; chip < chip_n; chip++) {
			int shares_found = calc_stat(devices[chip].stat_ts, short_stat, now);
			sprintf(line + strlen(line), "%.1f|%.0f ", shares_to_ghashes(shares_found, short_stat), devices[chip].mhz);
			if(short_out_t && !shares_found) {
//				printf("AAA chip %d REINIT!!!!!!!!!\n", chip);
				send_freq(chip, devices[chip].osc6_bits - 1);
				nmsleep(10);
				send_freq(chip, devices[chip].osc6_bits);
			}
			shares_total += shares_found;
			shares_first += chip < 4 ? shares_found : 0;
			shares_last += chip > 3 ? shares_found : 0;
		}
		sprintf(line + strlen(line), "| 1st half: %.2f, 2nd half: %.2f, total: %.2fGhashes/s", shares_to_ghashes(shares_first, short_stat),
			shares_to_ghashes(shares_last, short_stat), shares_to_ghashes(shares_total, short_stat));
		applog(LOG_WARNING, line);
		short_out_t = now.tv_sec;
	}

	if (now.tv_sec - long_out_t > long_stat) {
		int shares_first = 0, shares_last = 0, shares_total = 0;
		sprintf(line, "!_____ LONG stat %ds: ", long_stat);
		for (chip = 0; chip < chip_n; chip++) {
			int shares_found = calc_stat(devices[chip].stat_ts, long_stat, now);
			sprintf(line + strlen(line), " %.2f(%.2fmhz), ", shares_to_ghashes(shares_found, long_stat), devices[chip].mhz);
			if (chip == 3) sprintf(line + strlen(line), "\n\t\t\t");
			shares_total += shares_found;
			shares_first += chip < 4 ? shares_found : 0;
			shares_last += chip > 3 ? shares_found : 0;
		}
		sprintf(line + strlen(line), "\n\t\t\t| 1st half: %.2f, 2nd half: %.2f, total: %.2fGhashes/s", shares_to_ghashes(shares_first, long_stat),
			shares_to_ghashes(shares_last, long_stat), shares_to_ghashes(shares_total, long_stat));
		applog(LOG_WARNING, line);
		long_out_t = now.tv_sec;
	}

	return hashes;
}

double shares_to_ghashes(int shares, int seconds) {
	return (double)shares / (double)seconds * 4.84387;  //orig: 4.77628
}

int calc_stat(time_t * stat_ts, time_t stat, struct timeval now) {
	int j;
	int shares_found = 0;
	for(j = 0; j < BITFURY_STAT_N; j++) {
		if (now.tv_sec - stat_ts[j] < stat) {
			shares_found++;
		}
	}
	return shares_found;
}

static void bitfury_statline_before(char *buf, struct cgpu_info *cgpu)
{
	applog(LOG_INFO, "INFO bitfury_statline_before");
}

static bool bitfury_prepare(struct thr_info *thr)
{
	struct timeval now;
	struct cgpu_info *cgpu = thr->cgpu;

	cgtime(&now);
	get_datestamp(cgpu->init, &now);

	applog(LOG_INFO, "INFO bitfury_prepare");
	return true;
}

static void bitfury_shutdown(struct thr_info *thr)
{
	int chip_n;
	int i;

	chip_n = thr->cgpu->chip_n;

	applog(LOG_INFO, "INFO bitfury_shutdown");
	applog(LOG_WARNING, "AAA INFO bitfury_shutdown");
	for (i = 0; i < chip_n; i++) {
		send_shutdown(i);
	}
	libbitfury_shutdownChips(thr->cgpu->devices);
}

static void bitfury_disable(struct thr_info *thr)
{
	applog(LOG_INFO, "INFO bitfury_disable");
}

struct device_drv bitfury_drv = {
	.drv_id = DRIVER_BITFURY,
	.dname = "bitfury",
	.name = "BITFURY",
	.drv_detect = bitfury_detect,
	.get_statline_before = bitfury_statline_before,
	.thread_prepare = bitfury_prepare,
	.scanwork = bitfury_scanHash,
	.thread_shutdown = bitfury_shutdown,
	.hash_work = hash_queued_work,
};
