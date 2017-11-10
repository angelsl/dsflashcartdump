/* dsfirmverify
 * Copyright (C) 2017 angelsl
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <cstdio>
#include <cstdint>
#include <cstddef>

#include <nds.h>
#include <nds/arm9/dldi.h>
#include <fat.h>

#include <ncgcpp/ntrcard.h>

#include "../flashcart_core/device.h"

namespace {
	const std::uint32_t dummy_blowfish_key[0x412] = {0};
}

namespace flashcart_core {
    namespace platform {
	    void showProgress(std::uint32_t current, std::uint32_t total, const char *status) {
	        static_cast<void>(current); static_cast<void>(total); static_cast<void>(status);
        }

        int logMessage(log_priority priority, const char *fmt, ...) {
            static_cast<void>(priority); static_cast<void>(fmt);
            return -1;
        }       

        auto getBlowfishKey(BlowfishKey key) -> const std::uint32_t(&)[0x412] {
            static_cast<void>(key);
            // we will never do KEY1 init
            return dummy_blowfish_key;
        }
    }
}

namespace {
    PrintConsole topScreen;
    PrintConsole bottomScreen;

    std::uint32_t wait_for_any_key(std::uint32_t keys) {
        while (true) {
            scanKeys();
            std::uint32_t down = keysDown();
            if (down & keys) {
                return down;
            }
            swiWaitForVBlank();
        }
    }

    void wait_for_keys(std::uint32_t keys) {
        while (true) {
            scanKeys();
            if(keysDown() == keys) {
                break;
            }
            swiWaitForVBlank();
        }
    }

    std::uint8_t buffer[0x4000];
    ncgc::NTRCard card(nullptr);
    flashcart_core::Flashcart *flashcart = nullptr;

    void select_flashcart() {
        consoleSelect(&bottomScreen);
        bool done = false;
        std::size_t i = 0;
        const std::size_t n = flashcart_core::flashcart_list->size();

        while (!done) {
            consoleClear();
            iprintf("<UP/DOWN> Scroll <A> Select\n"
                    "Current cart:\n"
                    "%s\n", flashcart_core::flashcart_list->at(i)->getName());

            switch (wait_for_any_key(KEY_UP | KEY_DOWN | KEY_A)) {
                case KEY_UP:
                    i = (i - 1) % n;
                    continue;
                case KEY_DOWN:
                    i = (i + 1) % n;
                    continue;
                case KEY_A:
                    done = true;
                    break;
            }
        }

        flashcart = flashcart_core::flashcart_list->at(i);
        consoleClear();
        consoleSelect(&topScreen);
    }

    void dump() {
        FILE *f = fopen("fat:/backup.bin", "wb");
        if (!f) {
            iprintf("File open failed.\n");
            return;
        }

        iprintf("Dumping to backup.bin.\n");

        const std::size_t bsz = sizeof(buffer);
        const std::size_t sz = flashcart->getMaxLength();
        std::size_t cur = 0;
        while (cur < sz) {
            const std::size_t cbsz = std::min(sz - cur, bsz);
            if (!flashcart->readFlash(cur, cbsz, buffer)) {
                iprintf("Flash read failed.\n");
                goto fail;
            }

            if (fwrite(buffer, 1, cbsz, f) != cbsz) {
                iprintf("File write failed.\n");
                goto fail;
            }

            cur += cbsz;

            consoleSelect(&bottomScreen);
            consoleClear();
            iprintf("Dumping flash\n"
                    "%d/%d\n"
                    "%d%%\n",
                    cur, sz, 100*cur/sz);
            consoleSelect(&topScreen);
        }

        iprintf("Success.\n");

fail:
        if (f) {
            fclose(f);
        }
        return;
    }
}

int main() {
    videoSetMode(MODE_0_2D);
	videoSetModeSub(MODE_0_2D);

	vramSetBankA(VRAM_A_MAIN_BG);
	vramSetBankC(VRAM_C_SUB_BG);

    consoleInit(&topScreen, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, true);
	consoleInit(&bottomScreen, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, false, true);
    consoleSelect(&topScreen);

    sysSetBusOwners(true, true);

    if (!fatInitDefault()) {
        iprintf("Failed to mount flashcart SD.\n");
        goto fail;
    }

    iprintf("DLDI name:\n%s\n", io_dldi_data->friendlyName);
    iprintf("Select your flashcart.\n");
    select_flashcart();

    if (!flashcart) {
        // should not happen
        iprintf("Something failed.\n");
        goto fail;
    }

    card.state(ncgc::NTRState::Key2); // assume :)
    if (!flashcart->initialize(card)) {
        iprintf("Flashcart initialisation failed.\n"
                "Please power off.\n");
        while (true) {
            swiWaitForVBlank();
        }
    }
    
    dump();

fail:
    fatUnmount("fat:/");
    consoleSelect(&topScreen);
    iprintf("Press B to exit.\n");
    wait_for_keys(KEY_B);
    return 0;
}
