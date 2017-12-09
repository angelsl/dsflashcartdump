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
#include <cstdarg>

#include <nds.h>
#include <nds/arm9/dldi.h>
#include <fat.h>

#include <ncgcpp/ntrcard.h>

#include "../flashcart_core/device.h"

namespace {
const std::uint8_t dummy_blowfish_key[0x1048] = {0};
}

namespace flashcart_core {
namespace platform {
int logMessage(log_priority priority, const char *fmt, ...) {
    if (priority < LOG_INFO) return 0;
    va_list args;
    va_start(args, fmt);
    int result = viprintf(fmt, args);
    va_end(args);
    iprintf("\n");
    return result;
}

auto getBlowfishKey(BlowfishKey key) -> const std::uint8_t(&)[0x1048] {
    static_cast<void>(key);
    // we will never do KEY1 init
    return dummy_blowfish_key;
}
}
}

namespace {
PrintConsole topScreen;
PrintConsole bottomScreen;

void showProgress(std::uint32_t current, std::uint32_t total, const char *status) {
    static const char *last_status = NULL;
    static int last_percent = -1;

    int now_percent = 100*current/total;
    if (now_percent > last_percent || last_status != status) {
        consoleSelect(&bottomScreen);
        consoleClear();
        iprintf("%s\n"
                "%ld/%ld\n"
                "%d%%\n",
                status, current, total, now_percent);
        consoleSelect(&topScreen);

        last_status = status;
        last_percent = now_percent;
    }
}

std::uint32_t wait_for_any_key(std::uint32_t keys) {
	while (true) {
		scanKeys();
        std::uint32_t down = keysDown();
		if(down & keys) {
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

std::uint8_t buffer[0x200000];
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
    iprintf("Dumping to backup.bin.\n");

    if (fatInitDefault()) {
        unlink("fat:/backup.bin");
        fatUnmount("fat:/");
    } else {
        iprintf("Failed to mount flashcart SD.\n");
        return;
    }

    const std::size_t bsz = sizeof(buffer);
    const std::size_t sz = flashcart->getMaxLength();
    std::size_t cur = 0;
    while (cur < sz) {
        const std::size_t cbsz = std::min(sz - cur, bsz);

        if (!flashcart->initialize(&card)) {
            iprintf("Flashcart initialisation failed.\n"
                    "Please power off.\n");
            while (true) {
                swiWaitForVBlank();
            }
        }

        if (!flashcart->readFlash(cur, cbsz, buffer)) {
            iprintf("Flash read failed.\n");
            return;
        }

        cur += cbsz;

        showProgress(cur, sz, "Writing to SD");
        bool fail = false;
        if (fatInitDefault()) {
            FILE *f = fopen("fat:/backup.bin", "ab");
            if (f) {
                if (fwrite(buffer, 1, cbsz, f) != cbsz) {
                    iprintf("File write failed.\n");
                    fail = true;
                }
                fclose(f);
            } else {
                iprintf("File open failed.\n");
                fail = true;
            }
            fatUnmount("fat:/");
        } else {
            iprintf("Failed to mount flashcart SD.\n");
            fail = true;
        }

        if (fail) {
            return;
        }
    }

    iprintf("Success.\n");
    consoleSelect(&bottomScreen);
    consoleClear();
    consoleSelect(&topScreen);
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

    iprintf("DLDI name:\n%s\n", io_dldi_data->friendlyName);
    iprintf("Select your flashcart.\n");
    select_flashcart();

    if (!flashcart) {
        // should not happen
        iprintf("Something failed.\n");
        goto fail;
    }

    card.state(ncgc::NTRState::Key2); // assume :)
    dump();

fail:
    consoleSelect(&topScreen);
    iprintf("Press B to exit.\n");
    wait_for_keys(KEY_B);
    return 0;
}

namespace flashcart_core {
namespace platform {
void showProgress(std::uint32_t current, std::uint32_t total, const char *status) {
    ::showProgress(current, total, status);
}
}
}
