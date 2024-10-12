/*
 * Wii U Time Sync - A NTP client plugin for the Wii U.
 *
 * Copyright (C) 2024  Daniel K. O.
 * Copyright (C) 2024  Nightkingale
 *
 * SPDX-License-Identifier: MIT
 */

#include <wupsxx/bool_item.hpp>
#include <wupsxx/category.hpp>
#include <wupsxx/init.hpp>
#include <wupsxx/logger.hpp>
#include <wupsxx/storage.hpp>

#include "cfg.hpp"

namespace cfg {

    namespace keys {
        const char *patch_men = "patch_men";
        const char *patch_hbm = "patch_hbm";
    } // namespace keys

    namespace labels {
        const char *patch_men = "Patch Wii U Menu";
        const char *patch_hbm = "Patch HOME Menu";
    } // namespace labels

    namespace defaults {
        const bool patch_men = true;
        const bool patch_hbm = true;
    } // namespace defaults

    bool patch_men = defaults::patch_men;
    bool patch_hbm = defaults::patch_hbm;

    wups::config::category make_config_screen() {
        wups::config::category cat{"Configuration"};

        cat.add(wups::config::bool_item::create(
                cfg::labels::patch_men, cfg::patch_men, cfg::defaults::patch_men,
                "true", "false"));

        cat.add(wups::config::bool_item::create(
                cfg::labels::patch_hbm, cfg::patch_hbm, cfg::defaults::patch_hbm,
                "true", "false"));

        return cat;
    }

    void menu_open(wups::config::category &root) {
        cfg::reload();

        root.add(make_config_screen());
    }

    void menu_close() { cfg::save(); }

    void init() {
        try {
            wups::config::init("overlayappbase_patch", menu_open, menu_close);

            cfg::load();
        } catch (std::exception &e) {
            wups::logger::printf("Init error: %s\n", e.what());
        }
    }

    void load() {
        try {
#define LOAD(x) wups::storage::load_or_init(keys::x, x, defaults::x)
            LOAD(patch_men);
            LOAD(patch_hbm);
#undef LOAD
        } catch (std::exception &e) {
            wups::logger::printf("Error loading config: %s\n", e.what());
        }
    }

    void reload() {
        try {
            wups::storage::reload();
            load();
        } catch (std::exception &e) {
            wups::logger::printf("Error reloading config: %s\n", e.what());
        }
    }

    void save() {
        try {
#define STORE(x) wups::storage::store(keys::x, x)
            STORE(patch_men);
            STORE(patch_hbm);
#undef STORE
            wups::storage::save();
        } catch (std::exception &e) {
            wups::logger::printf("Error saving config: %s\n", e.what());
        }
    }

} // namespace cfg
