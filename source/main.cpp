#include <wups.h>

#include <wupsxx/logger.hpp>

#include <coreinit/title.h>

#include "cfg.hpp"
#include "patches.hpp"

WUPS_PLUGIN_NAME("overlayappbase_patch");
WUPS_PLUGIN_DESCRIPTION("");
WUPS_PLUGIN_VERSION("v1.0.0");
WUPS_PLUGIN_AUTHOR("gf2p8affineqb");
WUPS_PLUGIN_LICENSE("GPLv3");

WUPS_USE_WUT_DEVOPTAB();
WUPS_USE_STORAGE("overlayappbase_patch");

INITIALIZE_PLUGIN() {
    wups::logger::guard guard{"overlayappbase_patch"};

    wups::logger::initialize("overlayappbase_patch");

    wups::logger::printf("INITIALIZE_PLUGIN\n");

    cfg::init();
}

DEINITIALIZE_PLUGIN() {
    wups::logger::printf("DEINITIALIZE_PLUGIN\n");

    wups::logger::finalize();
}

ON_APPLICATION_START() {
    auto title = OSGetTitleID();

    wups::logger::printf("ON_APPLICATION_START: got title %16llX\n", title);
    if (cfg::patch_men) {
        if (title == 0x5001010040000 || title == 0x5001010040100 || title == 0x5001010040200) {
            patches::perform_men_patches();
        }
    }
}
