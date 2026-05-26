#ifndef SMSPLUS_BUILD_FEATURES_H_
#define SMSPLUS_BUILD_FEATURES_H_

/*
 * Build-time component selection.
 *
 * These switches are intentionally centralised: host ports should not add
 * platform-specific preprocessor logic in their own source files.  Makefiles set
 * the values once and compile either the real component implementation or a
 * small stub layer where a disabled component still needs a public ABI.
 */
#ifndef SMSPLUS_ENABLE_ARCADE
#define SMSPLUS_ENABLE_ARCADE 1
#endif

#ifndef SMSPLUS_ENABLE_COLECO
#define SMSPLUS_ENABLE_COLECO 1
#endif

#ifndef SMSPLUS_ENABLE_SORDM5
#define SMSPLUS_ENABLE_SORDM5 1
#endif

/* Backwards-compatible alias for legacy code that still used SORDM5_EMU. */
#if SMSPLUS_ENABLE_SORDM5
#ifndef SORDM5_EMU
#define SORDM5_EMU 1
#endif
#endif

static inline int smsplus_arcade_enabled(void)
{
    return SMSPLUS_ENABLE_ARCADE != 0;
}

static inline int smsplus_coleco_enabled(void)
{
    return SMSPLUS_ENABLE_COLECO != 0;
}

static inline int smsplus_sordm5_enabled(void)
{
    return SMSPLUS_ENABLE_SORDM5 != 0;
}

#endif /* SMSPLUS_BUILD_FEATURES_H_ */
