/*  Copyright 2022 Pretendo Network contributors <pretendo.network>
        Copyright 2022 Ash Logan <ash@heyquark.com>

        Permission to use, copy, modify, and/or distribute this software for any
   purpose with or without fee is hereby granted, provided that the above
   copyright notice and this permission notice appear in all copies.

        THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
   WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
   MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
   SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
   RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
   CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
   CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <wups.h>

#include <wupsxx/logger.hpp>

#include <kernel/kernel.h>

#include <coreinit/cache.h>
#include <coreinit/dynload.h>
#include <coreinit/filesystem.h>
#include <coreinit/memorymap.h>

#include <cstring>
#include <string>
#include <vector>

#include "cfg.hpp"

namespace patches {

OSDynLoad_NotifyData men_rpx;
OSDynLoad_NotifyData hbm_rpx;

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

bool patch_instruction(void *instr, uint32_t original, uint32_t replacement) {
  uint32_t current = *(uint32_t *)instr;

  if (current != original)
    return current == replacement;

  wups::logger::printf("patch_instruction: writing to %08X (%08X) with %08X\n",
                       (uint32_t)instr, current, replacement);

  KernelCopyData(OSEffectiveToPhysical((uint32_t)instr),
                 OSEffectiveToPhysical((uint32_t)&replacement),
                 sizeof(replacement));
  DCFlushRange(instr, 4);
  ICInvalidateRange(instr, 4);

  current = *(uint32_t *)instr;

  return true;
}

bool patch_dynload_instructions() {
  uint32_t *patch1 = ((uint32_t *)&OSDynLoad_GetNumberOfRPLs) + 6;
  uint32_t *patch2 = ((uint32_t *)&OSDynLoad_GetRPLInfo) + 22;

  if (!patch_instruction(patch1, 0x41820038 /* beq +38 */,
                         0x60000000 /* nop */))
    return false;
  if (!patch_instruction(patch2, 0x41820100 /* beq +100 */,
                         0x60000000 /* nop */))
    return false;

  return true;
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

  if (cfg::patch_men) {
    patch_instruction((uint8_t *)men_rpx.textAddr + 0x1e0b10, 0x5403d97e,
                      0x38600001); // v277
    patch_instruction((uint8_t *)men_rpx.textAddr + 0x1e0a20, 0x5403d97e,
                      0x38600001); // v257
  } else {
    patch_instruction((uint8_t *)men_rpx.textAddr + 0x1e0b10, 0x38600001,
                      0x5403d97e); // v277
    patch_instruction((uint8_t *)men_rpx.textAddr + 0x1e0a20, 0x38600001,
                      0x5403d97e); // v257
  }
}

DECL_FUNCTION(int, FSOpenFile, FSClient *pClient, FSCmdBlock *pCmd,
              const char *path, const char *mode, int *handle, int error) {
  if (strcmp("/vol/content/Common/Package/Hbm2-2.pack", path) == 0) {
    if (find_rpl(hbm_rpx, "hbm.rpx")) {
      if (cfg::patch_hbm) {
        patch_instruction((uint8_t *)hbm_rpx.textAddr + 0x0ec430, 0x5403d97e,
                          0x38600001); // v197
        patch_instruction((uint8_t *)hbm_rpx.textAddr + 0x0ec434, 0x7c606110,
                          0x38600001); // v180
      } else {
        patch_instruction((uint8_t *)hbm_rpx.textAddr + 0x0ec430, 0x38600001,
                          0x5403d97e); // v197
        patch_instruction((uint8_t *)hbm_rpx.textAddr + 0x0ec434, 0x38600001,
                          0x7c606110); // v180
      }
    } else {
      wups::logger::printf("FSOpenFile: couldnt find hbm.rpx\n");
    }
  }
  int result = real_FSOpenFile(pClient, pCmd, path, mode, handle, error);
  return result;
}
WUPS_MUST_REPLACE_FOR_PROCESS(FSOpenFile, WUPS_LOADER_LIBRARY_COREINIT,
                              FSOpenFile, WUPS_FP_TARGET_PROCESS_ALL);
} // namespace patches
