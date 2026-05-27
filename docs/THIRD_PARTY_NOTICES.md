# Third-party notices

## MAME SN76496 / SN76489-family PSG

`source/sound/psg/mame_sn76489/sn76489.c` and `.h` are a compact C adaptation of
behavior from MAME `src/devices/sound/sn76496.cpp`. The upstream MAME file is
BSD-3-Clause and lists Nicola Salmoria as a copyright holder, with later
contributions by MAME developers.

## Sord M5 hardware behavior

The Sord M5 memory, CTC, VDP interrupt, keyboard, joystick, and I/O port mapping
implemented in `source/platform/sord_m5/sord_m5.c` and mapper setup in `source/sms.c` follows MAME `src/mame/sord/m5.cpp` as the
hardware reference. The SMS Plus GX implementation is a reduced C model written
for this codebase.

## 93C46 EEPROM mapper

`source/other/eeprom/93c46/eeprom93c46.c` and `.h`, and the mapper hooks around `$8000`,
`$8008-$8087`, and `$FFFC-$FFFF`, are derived from CrabEmu's
`consoles/sms/mapper-93c46.c` implementation. CrabEmu is GPLv2; the original
mapper file is Copyright (C) 2005, 2006, 2007, 2008, 2009, 2011, 2012 Lawrence
Sebald.

## ColecoVision MegaCart mapper

`source/sms.c` implements ColecoVision MegaCart behavior from MAME's
`src/devices/bus/coleco/cartridge/megacart.cpp`. MAME marks that file
BSD-3-Clause and lists Mark/Space Inc. as the copyright holder. The SMS Plus GX
implementation is a compact C adaptation for this codebase: `$8000-$BFFF` maps
the final 16 KiB ROM bank, `$C000-$FFFF` maps an active 16 KiB bank, and reads
from `$FFC0-$FFFF` select the active bank.

## MAME SNK arcade hardware reference

The compact SNK Psycho Soldier/TNK III/Athena/Ikari Warriors arcade hardware implementation in
`source/video/snk_ikari_psychos/snk_psychos.c`, `source/video/snk_ikari_psychos/snk_psychos.h`, and the SNK ZIP ROM definitions
in `source/loadrom.c` use hardware maps, graphics layouts, palette equations and
ROM region definitions derived from MAME's BSD-3-Clause SNK driver,
`src/mame/snk/snk.cpp`, `src/mame/snk/snk.h`, and `src/mame/snk/snk_v.cpp`.
MAME credits that driver to Ernesto Corvi, Tim Lindquist, Carlos A. Lozano,
Bryan McPhail, Jarek Parchanski, Nicola Salmoria, Tomasz Slanina,
Phil Stroffolino, Acho A. Tang and Victor Trucco, with thanks to Marco Cassili.

# YM-cores for OPLL/OPL1

YM2413 is from CrabEmu, OPL1/Y8950 from MAME, though the code have been mixed with others etc...
