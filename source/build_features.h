/*
 * MultiRexZ80
 *
 * Multi-system Z80 emulator based on SMS Plus GX by Eke-Eke, itself based on
 * SMS Plus by Charles MacDonald.
 *
 * Default project license: GPL-2.0-or-later.  File-specific notices below
 * are retained and take precedence for imported or derived components,
 * including MAME-derived code and other third-party modules.
 */

#ifndef MULTIREXZ80_BUILD_FEATURES_H_
#define MULTIREXZ80_BUILD_FEATURES_H_

/*
 * Build-time component selection.
 *
 * These switches are intentionally centralised: host ports should not add
 * platform-specific preprocessor logic in their own source files.  Makefiles set
 * the values once and compile either the real component implementation or a
 * small stub layer where a disabled component still needs a public ABI.
 */
#ifndef MULTIREXZ80_ENABLE_ARCADE
#define MULTIREXZ80_ENABLE_ARCADE 1
#endif

#ifndef MULTIREXZ80_ENABLE_COLECO
#define MULTIREXZ80_ENABLE_COLECO 1
#endif

#ifndef MULTIREXZ80_ENABLE_SORDM5
#define MULTIREXZ80_ENABLE_SORDM5 1
#endif

/* Backwards-compatible alias for legacy code that still used SORDM5_EMU. */
#if MULTIREXZ80_ENABLE_SORDM5
#ifndef SORDM5_EMU
#define SORDM5_EMU 1
#endif
#endif

static inline int multirexz80_arcade_enabled(void)
{
    return MULTIREXZ80_ENABLE_ARCADE != 0;
}

static inline int multirexz80_coleco_enabled(void)
{
    return MULTIREXZ80_ENABLE_COLECO != 0;
}

static inline int multirexz80_sordm5_enabled(void)
{
    return MULTIREXZ80_ENABLE_SORDM5 != 0;
}

#endif /* MULTIREXZ80_BUILD_FEATURES_H_ */
