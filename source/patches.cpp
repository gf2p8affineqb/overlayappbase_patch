/*  Copyright 2023 Pretendo Network contributors <pretendo.network>
    Copyright 2023 Ash Logan <ash@heyquark.com>
    Copyright 2019 Maschell

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <wups.h>

#include <wupsxx/logger.hpp>

#include <kernel/kernel.h>

#include <coreinit/cache.h>
#include <coreinit/debug.h>
#include <coreinit/dynload.h>
#include <coreinit/filesystem.h>
#include <coreinit/memorymap.h>

#include <cstring>
#include <string>
#include <vector>

#include "cfg.hpp"

namespace patches {

    const char original_discovery_url[] = "discovery.olv.nintendo.net/v1/endpoint";
    const char new_discovery_url[]      = "discovery.olv.pretendo.cc/v1/endpoint";

    const char root_rpx_check[] = "fs:/vol/external01/wiiu/payload.elf";

    OSDynLoad_NotifyData men_rpx;
    OSDynLoad_NotifyData hbm_rpx;

    uint32_t find_mem(uint32_t start, uint32_t size, const char *original_val,
                      size_t original_val_sz) {
        for (uint32_t addr = start; addr < start + size - original_val_sz; addr++) {
            if (memcmp(original_val, (void *) addr, original_val_sz) == 0) {
                return addr;
            }
        }
        return 0;
    }

    bool replace_mem(uint32_t start, uint32_t size, const char *original_val,
                     size_t original_val_sz, const char *new_val, size_t new_val_sz) {
        uint32_t addr = find_mem(start, size, original_val, original_val_sz);
        if (addr) {
            wups::logger::printf("replace: writing to %08X (%s) with %s\n", addr,
                                 original_val, new_val);
            KernelCopyData(OSEffectiveToPhysical(addr),
                           OSEffectiveToPhysical((uint32_t) new_val), new_val_sz);
            return true;
        }
        return false;
    }

    bool patch_instruction(void *instr, uint32_t original, uint32_t replacement) {
        uint32_t current = *(uint32_t *) instr;

        if (current != original)
            return current == replacement;

        wups::logger::printf("patch_instruction: writing to %08X (%08X) with %08X\n",
                             (uint32_t) instr, current, replacement);

        KernelCopyData(OSEffectiveToPhysical((uint32_t) instr),
                       OSEffectiveToPhysical((uint32_t) &replacement),
                       sizeof(replacement));
        DCFlushRange(instr, 4);
        ICInvalidateRange(instr, 4);

        current = *(uint32_t *) instr;

        return true;
    }

    bool patch_dynload_instructions() {
        uint32_t *patch1 = ((uint32_t *) &OSDynLoad_GetNumberOfRPLs) + 6;
        uint32_t *patch2 = ((uint32_t *) &OSDynLoad_GetRPLInfo) + 22;

        if (!patch_instruction(patch1, 0x41820038 /* beq +38 */,
                               0x60000000 /* nop */))
            return false;
        if (!patch_instruction(patch2, 0x41820100 /* beq +100 */,
                               0x60000000 /* nop */))
            return false;

        return true;
    }

    bool get_rpl_info(std::vector<OSDynLoad_NotifyData> &rpls) {
        int num_rpls = OSDynLoad_GetNumberOfRPLs();

        wups::logger::printf("get_rpl_info: %d RPL(s) running\n", num_rpls);

        if (num_rpls == 0) {
            return false;
        }

        rpls.resize(num_rpls);

        bool ret = OSDynLoad_GetRPLInfo(0, num_rpls, rpls.data());
        return ret;
    }

    bool find_rpl(OSDynLoad_NotifyData &found_rpl, const std::string &name) {
        if (!patch_dynload_instructions()) {
            wups::logger::printf("find_rpl: failed to patch dynload functions\n");
            return false;
        }

        std::vector<OSDynLoad_NotifyData> rpl_info;
        if (!get_rpl_info(rpl_info)) {
            wups::logger::printf("find_rpl: failed to get rpl info\n");
            return false;
        }

        for (const auto &rpl : rpl_info) {
            if (std::string_view(rpl.name).ends_with(name)) {
                found_rpl = rpl;
                return true;
            }
        }

        return false;
    }

    void perform_men_patches() {
        if (!find_rpl(men_rpx, "men.rpx")) {
            wups::logger::printf("perform_men_patches: couldnt find men.rpx\n");
            return;
        }

        if (find_mem(men_rpx.dataAddr, men_rpx.dataSize, root_rpx_check, sizeof(root_rpx_check))) {
            wups::logger::printf("perform_men_patches: men.rpx has been replaced by root.rpx, skipping patches ...\n");
            return;
        }

        patch_instruction((uint8_t *) men_rpx.textAddr + 0x1e0b10, 0x5403d97e,
                          0x38600001); // v277
        patch_instruction((uint8_t *) men_rpx.textAddr + 0x1e0a20, 0x5403d97e,
                          0x38600001); // v257
    }

    void osdynload_notify_callback(OSDynLoad_Module module, void *ctx,
                                   OSDynLoad_NotifyReason reason, OSDynLoad_NotifyData *rpl) {
        if (reason != OS_DYNLOAD_NOTIFY_LOADED)
            return;
        if (!rpl->name || !std::string_view(rpl->name).ends_with("nn_olv.rpl"))
            return;

        replace_mem(rpl->dataAddr, rpl->dataSize, original_discovery_url,
                    sizeof(original_discovery_url), new_discovery_url, sizeof(new_discovery_url));
    }

    DECL_FUNCTION(int, FSOpenFile, FSClient *pClient, FSCmdBlock *pCmd,
                  const char *path, const char *mode, int *handle, int error) {
        if (strcmp("/vol/content/Common/Package/Hbm2-2.pack", path) == 0) {
            if (find_rpl(hbm_rpx, "hbm.rpx")) {
                if (cfg::patch_hbm) {
                    patch_instruction((uint8_t *) hbm_rpx.textAddr + 0x0ec430, 0x5403d97e,
                                      0x38600001); // v197
                    patch_instruction((uint8_t *) hbm_rpx.textAddr + 0x0ec434, 0x7c606110,
                                      0x38600001); // v180
                } else {
                    patch_instruction((uint8_t *) hbm_rpx.textAddr + 0x0ec430, 0x38600001,
                                      0x5403d97e); // v197
                    patch_instruction((uint8_t *) hbm_rpx.textAddr + 0x0ec434, 0x38600001,
                                      0x7c606110); // v180
                }
            } else {
                wups::logger::printf("FSOpenFile: couldnt find hbm.rpx\n");
            }
        }

        if (strcmp("/vol/content/vino_config.txt", path) == 0) {
            OSDynLoad_AddNotifyCallback(&osdynload_notify_callback, nullptr);
        }

        int result = real_FSOpenFile(pClient, pCmd, path, mode, handle, error);
        return result;
    }
    WUPS_MUST_REPLACE_FOR_PROCESS(FSOpenFile, WUPS_LOADER_LIBRARY_COREINIT,
                                  FSOpenFile, WUPS_FP_TARGET_PROCESS_ALL);
} // namespace patches
