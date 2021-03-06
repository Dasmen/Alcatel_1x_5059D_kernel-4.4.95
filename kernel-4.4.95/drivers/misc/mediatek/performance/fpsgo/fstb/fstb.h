
#ifndef FSTB_H
#define FSTB_H

#ifdef CONFIG_MTK_FPSGO_FSTB
int fbt_reset_asfc(int);
int is_fstb_enable(void);
int switch_fstb(int);
int switch_sample_window(long long time_usec);
int switch_fps_range(int nr_level, struct fps_level *level);
int switch_fps_error_threhosld(int threshold);
int switch_percentile_frametime(int ratio);
int switch_force_vag(int arg);
int switch_vag_fps(int arg);
#else
static inline int fbt_reset_asfc(int level) { return 0; }
static inline int is_fstb_enable(void) { return 0; }
static inline int switch_fstb(int en) { return 0; }
static inline int switch_sample_window(long long time_usec) { return 0; }
static inline int switch_fps_range(int nr_level, struct fps_level *level) { return 0; }
static inline int switch_fps_error_threhosld(int threshold) { return 0; }
static inline int switch_percentile_frametime(int ratio) { return 0; }
static inline int switch_force_vag(int arg) { return 0; }
static inline int switch_vag_fps(int arg) { return 0; }
#endif

#endif

