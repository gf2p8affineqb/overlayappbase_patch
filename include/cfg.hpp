/*
 * Wii U Time Sync - A NTP client plugin for the Wii U.
 *
 * Copyright (C) 2024  Daniel K. O.
 * Copyright (C) 2024  Nightkingale
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef CFG_HPP
#define CFG_HPP

namespace cfg {

extern bool patch_men;
extern bool patch_hbm;

void init();

void load();
void reload();
void save();

} // namespace cfg

#endif
