/* include <asm/system.h> */
#include <linux/smp.h>
#include "cpu_pmu.h"
#include "v6_pmu_hw.h"
#include "v7_pmu_name.h"
#include "v8_pmu_name.h"	/* for 32-bit build of arm64 cpu */

#define ARMV7_PMCR_E		(1 << 0)	/* enable all counters */
#define ARMV7_PMCR_P		(1 << 1)
#define ARMV7_PMCR_C		(1 << 2)
#define ARMV7_PMCR_D		(1 << 3)
#define ARMV7_PMCR_X		(1 << 4)
#define ARMV7_PMCR_DP		(1 << 5)
#define ARMV7_PMCR_N_SHIFT		11	/* Number of counters supported */
#define ARMV7_PMCR_N_MASK		0x1f
#define ARMV7_PMCR_MASK			0x3f	/* mask for writable bits */


enum ARM_TYPE {
	CORTEX_A7 = 0xC07,
	CORTEX_A9 = 0xC09,
	CORTEX_A12 = 0xC0D,
	CORTEX_A15 = 0xC0F,
	CORTEX_A17 = 0xC0E,
	CORTEX_A53 = 0xD03,
	CORTEX_A57 = 0xD07,
	CHIP_UNKNOWN = 0xFFF
};

struct chip_pmu {
	enum ARM_TYPE type;
	struct pmu_desc *desc;
	unsigned int count;
	const char *cpu_name;
};

static struct chip_pmu chips[] = {
	{CORTEX_A7, a7_pmu_desc, A7_PMU_DESC_COUNT, "Cortex-A7"},
	{CORTEX_A9, a9_pmu_desc, A9_PMU_DESC_COUNT, "Cortex-A9"},
	{CORTEX_A12, a7_pmu_desc, A7_PMU_DESC_COUNT, "Cortex-A12"},
	{CORTEX_A15, a7_pmu_desc, A7_PMU_DESC_COUNT, "Cortex-A15"},
	{CORTEX_A17, a7_pmu_desc, A7_PMU_DESC_COUNT, "Cortex-A17"},
	{CORTEX_A53, a53_pmu_desc, A53_PMU_DESC_COUNT, "Cortex-A53"},
	{CORTEX_A57, a7_pmu_desc, A7_PMU_DESC_COUNT, "Cortex-A57"},
};
static struct chip_pmu chip_unknown = { CHIP_UNKNOWN, NULL, 0, "Unknown CPU" };

#define CHIP_PMU_COUNT (sizeof(chips) / sizeof(struct chip_pmu))

static struct chip_pmu *chip;

static enum ARM_TYPE armv7_get_ic(void)
{
	unsigned int value;
	/* Read Main ID Register */
	asm volatile ("mrc p15, 0, %0, c0, c0, 0":"=r" (value));

	value = (value & 0xffff) >> 4;	/* primary part number */
	return value;
}

static inline void armv7_pmu_counter_select(unsigned int idx)
{
	asm volatile ("mcr p15, 0, %0, c9, c12, 5"::"r" (idx));
	isb();
}

static inline void armv7_pmu_type_select(unsigned int idx, unsigned int type)
{
	armv7_pmu_counter_select(idx);
	asm volatile ("mcr p15, 0, %0, c9, c13, 1"::"r" (type));
}

static inline unsigned int armv7_pmu_read_count(unsigned int idx)
{
	unsigned int value;

	if (idx == 31) {
		asm volatile ("mrc p15, 0, %0, c9, c13, 0":"=r" (value));
	} else {
		armv7_pmu_counter_select(idx);
		asm volatile ("mrc p15, 0, %0, c9, c13, 2":"=r" (value));
	}
	return value;
}

static inline void armv7_pmu_write_count(int idx, u32 value)
{
	if (idx == 31) {
		asm volatile ("mcr p15, 0, %0, c9, c13, 0"::"r" (value));
	} else {
		armv7_pmu_counter_select(idx);
		asm volatile ("mcr p15, 0, %0, c9, c13, 2"::"r" (value));
	}
}

static inline void armv7_pmu_enable_count(unsigned int idx)
{
	asm volatile ("mcr p15, 0, %0, c9, c12, 1"::"r" (1 << idx));
}

static inline void armv7_pmu_disable_count(unsigned int idx)
{
	asm volatile ("mcr p15, 0, %0, c9, c12, 2"::"r" (1 << idx));
}

static inline void armv7_pmu_enable_intr(unsigned int idx)
{
	asm volatile ("mcr p15, 0, %0, c9, c14, 1"::"r" (1 << idx));
}

static inline void armv7_pmu_disable_intr(unsigned int idx)
{
	asm volatile ("mcr p15, 0, %0, c9, c14, 2"::"r" (1 << idx));
}

static inline unsigned int armv7_pmu_overflow(void)
{
	unsigned int val;

	asm volatile ("mrc p15, 0, %0, c9, c12, 3":"=r" (val));	/* read */
	asm volatile ("mcr p15, 0, %0, c9, c12, 3"::"r" (val));
	return val;
}

static inline unsigned int armv7_pmu_control_read(void)
{
	u32 val;

	asm volatile ("mrc p15, 0, %0, c9, c12, 0":"=r" (val));
	return val;
}

static inline void armv7_pmu_control_write(unsigned int val)
{
	val &= ARMV7_PMCR_MASK;
	isb();
	asm volatile ("mcr p15, 0, %0, c9, c12, 0"::"r" (val));
}

static int armv7_pmu_hw_get_counters(void)
{
	int count = armv7_pmu_control_read();
	/* N, bits[15:11] */
	count = ((count >> ARMV7_PMCR_N_SHIFT) & ARMV7_PMCR_N_MASK);
	return count;
}

static void armv7_pmu_hw_reset_all(int generic_counters)
{
	int i;

	armv7_pmu_control_write(ARMV7_PMCR_C | ARMV7_PMCR_P);
	/* generic counter */
	for (i = 0; i < generic_counters; i++) {
		armv7_pmu_disable_intr(i);
		armv7_pmu_disable_count(i);
	}
	/* cycle counter */
	armv7_pmu_disable_intr(31);
	armv7_pmu_disable_count(31);
	armv7_pmu_overflow();	/* clear overflow */
}

static int armv7_pmu_hw_get_event_desc(int i, int event, char *event_desc)
{
	if (event_desc == NULL)
		return -1;

	for (i = 0; i < chip->count; i++) {
		if (chip->desc[i].event == event) {
			strncpy(event_desc, chip->desc[i].name, MXSIZE_PMU_DESC - 1);

			/* if exceed MXSIZE_PMU_DESC size, truncate event name */
			event_desc[MXSIZE_PMU_DESC - 1] = '\0';

			break;
		}
	}
	if (i == chip->count)
		return -1;

	return 0;
}

static int armv7_pmu_hw_check_event(struct met_pmu *pmu, int idx, int event)
{
	int i;

	/* Check if event is duplicate */
	for (i = 0; i < idx; i++) {
		if (pmu[i].event == event)
			break;
	}
	if (i < idx) {
		/* pr_debug("++++++ found duplicate event 0x%02x i=%d\n", event, i); */
		return -1;
	}

	for (i = 0; i < chip->count; i++) {
		if (chip->desc[i].event == event)
			break;
	}

	if (i == chip->count)
		return -1;

	return 0;
}

static void armv7_pmu_hw_start(struct met_pmu *pmu, int count)
{
	int i;
	int generic = count - 1;

	armv7_pmu_hw_reset_all(generic);
	for (i = 0; i < generic; i++) {
		if (pmu[i].mode == MODE_POLLING) {
			armv7_pmu_type_select(i, pmu[i].event);
			armv7_pmu_enable_count(i);
		}
	}
	if (pmu[count - 1].mode == MODE_POLLING) {	/* cycle counter */
		armv7_pmu_enable_count(31);
	}
	armv7_pmu_control_write(ARMV7_PMCR_E);
}

static void armv7_pmu_hw_stop(int count)
{
	int generic = count - 1;

	armv7_pmu_hw_reset_all(generic);
}

static unsigned int armv7_pmu_hw_polling(struct met_pmu *pmu, int count, unsigned int *pmu_value)
{
	int i, cnt = 0;
	int generic = count - 1;

	for (i = 0; i < generic; i++) {
		if (pmu[i].mode == MODE_POLLING) {
			pmu_value[cnt] = armv7_pmu_read_count(i);
			cnt++;
		}
	}
	if (pmu[count - 1].mode == MODE_POLLING) {
		pmu_value[cnt] = armv7_pmu_read_count(31);
		cnt++;
	}
	armv7_pmu_control_write(ARMV7_PMCR_C | ARMV7_PMCR_P | ARMV7_PMCR_E);

	return cnt;
}


struct cpu_pmu_hw armv7_pmu = {
	.name = "armv7_pmu",
	.get_event_desc = armv7_pmu_hw_get_event_desc,
	.check_event = armv7_pmu_hw_check_event,
	.start = armv7_pmu_hw_start,
	.stop = armv7_pmu_hw_stop,
	.polling = armv7_pmu_hw_polling,
};

struct cpu_pmu_hw *cpu_pmu_hw_init(void)
{
	int i;
	enum ARM_TYPE type;
	struct cpu_pmu_hw *pmu = NULL;

	type = armv7_get_ic();
	for (i = 0; i < CHIP_PMU_COUNT; i++) {
		if (chips[i].type == type) {
			chip = &(chips[i]);
			break;
		}
	}

	if (chip != NULL) {
		armv7_pmu.nr_cnt = armv7_pmu_hw_get_counters() + 1;
		armv7_pmu.cpu_name = chip->cpu_name;
		pmu = &armv7_pmu;
	} else {
		pmu = v6_cpu_pmu_hw_init(type);
	}

	if ((chip == NULL) && (pmu == NULL)) {
		chip = &chip_unknown;
		return NULL;
	}

	return pmu;
}
