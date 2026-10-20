/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __RTW_S9082_TRACE_H__
#define __RTW_S9082_TRACE_H__
#ifdef CONFIG_S9082
#include <linux/atomic.h>
#include <linux/jiffies.h>
#include <linux/printk.h>
extern bool s9082_scan_trace;
extern bool s9082_ack_failfast;
extern atomic_t s9082_trace_remaining;
/* At most 600 metadata-only records per module load. */
#define S9082_TRACE(format, ...) do { \
	if (s9082_scan_trace && atomic_dec_if_positive(&s9082_trace_remaining) >= 0) \
		pr_info("S9082-TRACE t=%u " format "\n", \
			jiffies_to_msecs(jiffies), ##__VA_ARGS__); \
} while (0)
#else
#define S9082_TRACE(format, ...) do { } while (0)
#endif
#endif
