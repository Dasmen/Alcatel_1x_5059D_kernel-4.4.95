
#ifndef _MTKDCS_DRV_H_
#define _MTKDCS_DRV_H_

/* enum should be aligned with tinysys middleware */

enum {
	IPI_DCS_MIGRATION,
	IPI_DCS_GET_MODE,
	IPI_DCS_SET_DUMMY_WRITE,
	IPI_DCS_SET_MD_NOTIFY,
	IPI_DCS_DUMP_REG,
	IPI_DCS_SET_MD_REGION,
	IPI_DCS_FORCE_ACC_LOW,
	NR_DCS_IPI,
};

enum migrate_dir {
	NORMAL,
	LOWPWR,
};

enum initial_status {
	CASCADE_NORMAL_INTERLEAVE_NORMAL,
	CASCADE_LOWPWR_INTERLEAVE_NORMAL,
	CASCADE_NORMAL_INTERLEAVE_LOWPWR,
	CASCADE_LOWPWR_INTERLEAVE_LOWPWR,
};

#ifdef CONFIG_MTK_EMI_MPU
void dump_emi_outstanding_for_md(void);
#endif

#endif /* _MTKDCS_DRV_H_ */
