#include <Arduino.h>
#include <vector>
#include <Keypad.h>

#include "malkuth_fs.h"
#include "malkuth_display.h"
#include "malkuth_audio.h"

namespace Theme {
    constexpr uint16_t C_BG             = TFT_BLACK;
    constexpr uint16_t C_CARD           = TFT_DARKGREY;

    constexpr uint16_t C_ACCENT         = 0x5e1a;
    constexpr uint16_t C_ACCENT_DARK    = 0x11a7;
    constexpr uint16_t C_ACCENT_MUTED   = 0x1a09;

    constexpr uint16_t C_TEXT_PRIMARY   = TFT_WHITE;
    constexpr uint16_t C_TEXT_MUTED     = 0x7BEF;
    
    constexpr uint16_t C_SUCCESS        = TFT_GREEN;
    constexpr uint16_t C_WARNING        = TFT_YELLOW;
    constexpr uint16_t C_ERROR          = TFT_RED;

    constexpr uint16_t C_WHITE          = TFT_WHITE;
    constexpr uint16_t C_BLACK          = TFT_BLACK;

    constexpr uint8_t  R_SMALL  = 8;
    constexpr uint8_t  R_MEDIUM = 12;
    constexpr uint8_t  R_LARGE  = 16;

    const uint8_t*  FONT_SMALL      = Koruri_Regular8;
    const uint8_t*  FONT_MEDIUM     = Koruri_Regular12;
    const uint8_t*  FONT_LARGE      = Koruri_Regular18;
    const uint8_t*  FONT_HUGE       = Koruri_Regular24;
}

enum class Page : uint8_t {
    NONE,
    BOOT_SCREEN,
    PLAYER,
    DIRECTORY,
    SETTINGS,
};

Page    current_page = Page::NONE;
static char current_directory[128]  = "/";
static char selected_directory[128] = {};

static char vol_buf[16]     = {};
static char time_buf[12]    = {};
static char notif_buf[64]   = {};

static uint32_t last_update = 0;
constexpr uint32_t FRAME_RATE = 1000;

MalkuthDisplay  display;
MalkuthFs       filesystem;
MalkuthAudio    audio;

AudioMetadata metadata;

void page(Page p, bool redraw = false);
void page_boot();
void page_settings();
void page_player();
void page_directory();
void page_directory_widget(uint8_t start_idx);

void show_statusbar();
void show_menubar(Page active);
void show_notification(const char* text, uint16_t color, uint16_t delay_ms);

void check_keypress();

void duration_format(char* time_str, float time) {
    uint32_t secs_total = static_cast<uint32_t>(time);
    uint8_t mins = secs_total / 60;
    uint8_t secs = secs_total % 60;

    snprintf(time_buf, sizeof(time_buf), "%u:%02u", mins, secs);
}

void setup() {
    Serial.begin(115200);

    display.init(0);
    page(Page::BOOT_SCREEN);

    delay(1);
    if (!filesystem.init())
        display.text(Anchor::BOTTOM_CENTER, false, "SD Card is failed to be mounted!", Theme::FONT_SMALL, Theme::C_ERROR, 0, -10);
    else
        display.text(Anchor::BOTTOM_CENTER, false, "SD Card is successfully mounted!", Theme::FONT_SMALL, Theme::C_SUCCESS, 0, -10);
    
    audio.set_sdfs(filesystem.get_sdfs());
    display.set_sdfs(filesystem.get_sdfs());

    if (!audio.init())
        display.text(Anchor::BOTTOM_CENTER, false, "Audio is failed to be intialized!", Theme::FONT_SMALL, Theme::C_ERROR, 0, -30);
    else 
        display.text(Anchor::BOTTOM_CENTER, false, "Audio is successfully intialized!", Theme::FONT_SMALL, Theme::C_SUCCESS, 0, -30);

}

void loop() {
skip:
    audio.loop();
    display.buttons_check();
    keypress_check();

    if (audio.get_update()) {
        metadata = audio.get_metadata();
        audio.yeah_i_have_updated();

        if (!audio.is_actually_audio()) {
            goto skip;
        }

        if (current_page == Page::PLAYER) {
            page(Page::PLAYER, true);
        }
    }

    if (current_page == Page::PLAYER && audio.get_status()) {
        uint32_t now = millis();

        if (now - last_update >= FRAME_RATE) {
            last_update = now;
            float position = audio.get_position();
            float duration = metadata.duration;

            if (duration > 0) {
                uint8_t progress = constrain(static_cast<uint8_t>((position / duration) * 100), 0, 100);
                
                duration_format(time_buf, position);
                display.bar(Anchor::BOTTOM_CENTER, 280, 10, Theme::C_CARD, Theme::C_ACCENT, 5, 0, -130, progress, nullptr);
                display.object(Anchor::BOTTOM_LEFT, 25, 12, Theme::C_BLACK, 5, 21, -113);
                display.text(Anchor::BOTTOM_LEFT, true, time_buf, Theme::FONT_SMALL, Theme::C_WHITE, 20, -110);
            }
        }
    }
}

void page(Page p, bool redraw) {
    if (current_page == p && !redraw) return;

    current_page = p;
    display.buttons_clear();
    display.clear();

    if (current_page != Page::BOOT_SCREEN && current_page != Page::PLAYER) {
        show_statusbar();
        show_menubar(current_page);
    }

    switch (p) {
        case Page::BOOT_SCREEN: page_boot(); break;
        case Page::PLAYER:      page_player(); break;
        case Page::DIRECTORY:   page_directory(); break;
        case Page::SETTINGS:    page_settings(); break;
        default: break;
    }
}

void page_boot() {
    display.image(ImageType::FLASH, BootBg, sizeof(BootBg));
    display.text(Anchor::TOP_CENTER,    true, "おかえり~~~ :3",       Theme::FONT_HUGE,  Theme::C_BG, 0, 20);
    display.text(Anchor::BOTTOM_CENTER, true, " > Touch to start <", Theme::FONT_LARGE, Theme::C_TEXT_PRIMARY, 0, -60);

    display.button(Anchor::MIDDLE_CENTER, 320, 480, 0, 0, [](void*) {
        page(Page::PLAYER);
    });
}

void page_player() {
    // Maybe i will fix later (Cover image loading)
    // if (audio.get_covertype() == ImageType::NONE)
    //     display.object(Anchor::MIDDLE_CENTER, 200, 200, Theme::C_CARD, 10, 0, -80);
    //     display.image(ImageType::FLASH, PlayerBg, sizeof(BootBg));
    // else
    //     display.image(audio.get_covertype(), audio.get_coverpath(), 200, 200, 60, 40);

    display.image(ImageType::FLASH, PlayerBg_b, sizeof(PlayerBg_b));
    show_statusbar();
    show_menubar(current_page);

    display.text(Anchor::MIDDLE_CENTER, true, metadata.title.c_str(), Theme::FONT_HUGE, Theme::C_TEXT_PRIMARY, 0, 50);
    display.text(Anchor::MIDDLE_CENTER, true, metadata.artist.c_str(), Theme::FONT_LARGE, Theme::C_TEXT_PRIMARY, 0, 80);

    duration_format(time_buf, metadata.duration);
    display.object(Anchor::BOTTOM_RIGHT, 25, 12, Theme::C_BLACK, 5, -20, -113);
    display.text(Anchor::BOTTOM_RIGHT, true, time_buf, Theme::FONT_SMALL, Theme::C_WHITE, -20, -110);

    display.button(Anchor::BOTTOM_CENTER, 30, 30, Theme::C_ACCENT_MUTED, 10, 1, -82, [](void*){
        audio.toggle();
        show_notification(audio.get_status() ? "Playing..." : "Paused...", Theme::C_SUCCESS, 2000);
    });

    display.button(Anchor::BOTTOM_CENTER, 30, 30, Theme::C_ACCENT_MUTED, 10, -41, -82, [](void*){
        audio.previous();
        show_notification("Previous Track", Theme::C_ACCENT, 500);
    });

    display.button(Anchor::BOTTOM_CENTER, 30, 30, Theme::C_ACCENT_MUTED, 10, 42, -82, [](void*){
        audio.next();
        show_notification("Next Track", Theme::C_ACCENT, 500);
    });

    display.text(Anchor::BOTTOM_CENTER, true, "<      ||      >", Theme::FONT_LARGE, Theme::C_WHITE, 0, -85);

    if (metadata.duration <= 0) {
        display.text(Anchor::BOTTOM_CENTER, true, "Select a Directory to Play!", Theme::FONT_LARGE, Theme::C_TEXT_PRIMARY, 0, -150);
    }
}

void page_directory() {
    page_directory_widget(0);
}

void page_directory_widget(uint8_t start_idx) {
    display.buttons_clear();
    display.object(Anchor::TOP_CENTER, 320, 380, TFT_BLACK, 0, 0, 40);
    display.text(Anchor::TOP_LEFT, true, "Directory Listing", Theme::FONT_HUGE, Theme::C_WHITE, 10, 40);
    show_menubar(Page::DIRECTORY);

    std::vector<String> files = filesystem.get_directory_files(current_directory);
    uint8_t files_count = files.size();
    constexpr uint8_t VISIBLE_ITEMS = 5;

    uint8_t end_idx = start_idx + VISIBLE_ITEMS;
    if (end_idx > files_count) 
        end_idx = files_count;

    display.button(Anchor::TOP_CENTER, 300, 40, Theme::C_TEXT_MUTED, 10, 0, 75, [start_idx, &files](void*) {
        size_t len = strlen(current_directory);
        if (len > 1) {
            for (int i = len - 2; i >= 0; --i) {
                if (current_directory[i] == '/') {
                    current_directory[i + 1] = '\0';
                    break;
                }
            }
            page_directory_widget(0);
        }
    });
    display.text(Anchor::TOP_LEFT, true, "../ (Previous Directory)", Theme::FONT_LARGE, Theme::C_BG, 20, 84);

    for (uint8_t i = 0; i < VISIBLE_ITEMS; ++i) {
        uint8_t idx = start_idx + i;
        if (idx >= files_count) break;

        int16_t y_offset = 75 + ((i + 1) * 45);
        const String& file = files[idx];

        display.button(Anchor::TOP_CENTER, 300, 40, 0, y_offset, [file, start_idx](void*) {
            if (file.endsWith("/")) {
                strncat(current_directory, file.c_str(), sizeof(current_directory) - strlen(current_directory) - 1);
                page_directory_widget(0);
            }
        });
        display.text(Anchor::TOP_LEFT, true, file.c_str(), Theme::FONT_LARGE, Theme::C_TEXT_PRIMARY, 20, 84 + (i + 1) * 44);
    }

    if (start_idx > 0) {
        display.button(Anchor::BOTTOM_RIGHT, 60, 30, Theme::C_TEXT_MUTED, 10, -70, -80, [start_idx](void*) {
            page_directory_widget(start_idx - 1);
        });
        display.text(Anchor::BOTTOM_RIGHT, true, "Λ", Theme::FONT_HUGE, Theme::C_ACCENT_DARK, -93, -77);
    }

    if (end_idx < files_count) {
        display.button(Anchor::BOTTOM_RIGHT, 60, 30, Theme::C_TEXT_MUTED, 10, -20, -80, [start_idx](void*) {
            page_directory_widget(start_idx + 1);
        });
        display.text(Anchor::BOTTOM_RIGHT, true, "V", Theme::FONT_HUGE, Theme::C_ACCENT_DARK, -33, -77);
    }

    display.object(Anchor::BOTTOM_RIGHT, 10, 30, Theme::C_BG, 0, -70, -80);

    bool selected = (strcmp(selected_directory, current_directory) == 0);
    display.button(Anchor::BOTTOM_LEFT, 120, 30, selected ? Theme::C_ACCENT : Theme::C_ACCENT_DARK, 10, 20, -80, [](void*) {
        strcpy(selected_directory, current_directory);

        audio.process_directory(selected_directory);
        page(Page::DIRECTORY, true);
    });
    display.text(Anchor::BOTTOM_LEFT, true, "Choose", Theme::FONT_LARGE, selected ? Theme::C_BG : Theme::C_TEXT_MUTED, 42, -81);
    display.text(Anchor::BOTTOM_LEFT, true, current_directory, Theme::FONT_SMALL, Theme::C_TEXT_MUTED, 0, -120);
}

void page_settings() {
    display.text(Anchor::TOP_LEFT, true, "Settings", Theme::FONT_HUGE, Theme::C_WHITE, 10, 40);

    constexpr uint16_t slider_width = 280;
    constexpr uint16_t slider_height = 20;

    uint8_t current_brightness = display.get_brightness();
    display.text(Anchor::TOP_LEFT, true, "Brightness", Theme::FONT_LARGE, Theme::C_WHITE, 10, 80);
    display.bar(Anchor::TOP_CENTER, slider_width, slider_height, Theme::C_TEXT_MUTED, Theme::C_WARNING, 10, 0, 110, current_brightness,
        [](void*) {
            TouchData p = display.get_touchdata();
            if (p.z == 0) return;
            uint8_t val = map(constrain(p.x, 0, slider_width), 0, slider_width, 1, 100);
            if (val != display.get_brightness()) {
                display.set_brightness(val);
                display.bar(Anchor::TOP_CENTER, slider_width, slider_height, Theme::C_TEXT_MUTED, Theme::C_WARNING, 10, 0, 110, val, nullptr);
            }
        }, nullptr);

    uint8_t current_volume = audio.get_volume();
    display.text(Anchor::TOP_LEFT, true, "Volume", Theme::FONT_LARGE, TFT_WHITE, 10, 150);
    display.bar(Anchor::TOP_CENTER, slider_width, slider_height, Theme::C_TEXT_MUTED, Theme::C_ACCENT, 10, 0, 180, current_volume,
        [](void*) {
            TouchData p = display.get_touchdata();
            if (p.z == 0) return;
            uint8_t val = map(constrain(p.x, 0, slider_width), 0, slider_width, 1, 100);
            if (val != audio.get_volume()) {
                audio.set_volume(val);
                display.bar(Anchor::TOP_CENTER, slider_width, slider_height, Theme::C_TEXT_MUTED, Theme::C_ACCENT, 10, 0, 180, val, nullptr);
                show_statusbar();
            }
        }, nullptr);

    display.button(Anchor::MIDDLE_CENTER, 280, 50, Theme::C_TEXT_MUTED, 15, 0, 5, [](void*) {
        page(Page::BOOT_SCREEN);
    });
    display.text(Anchor::MIDDLE_CENTER, true, "Boot Screen", Theme::FONT_LARGE, Theme::C_BG, 0, 5);

    display.button(Anchor::MIDDLE_CENTER, 135, 50, Theme::C_ACCENT_DARK, 15, -75, 70, [](void*) {
        char *task_list_buf = (char*)malloc(1024);
        char *runtime_buf   = (char*)malloc(1024);

        Serial.println("=========== TASK LIST ===========");
        vTaskList(task_list_buf);
        Serial.printf("%s\n", task_list_buf);

        Serial.println("========= CPU USAGE (%) =========");
        vTaskGetRunTimeStats(runtime_buf);
        Serial.printf("%s\n", runtime_buf);

        Serial.println("=========== HEAP INFO ===========");
        Serial.printf("Free Heap: %d bytes\n", heap_caps_get_free_size(MALLOC_CAP_8BIT));
        Serial.printf("Min Free Heap: %d bytes\n", heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));

        free(task_list_buf);
        free(runtime_buf);
        show_notification("Resources dumped to Serial", Theme::C_ACCENT, 2000);

    });
    display.text(Anchor::MIDDLE_CENTER, true, "Dump", Theme::FONT_LARGE, Theme::C_WHITE, -75, 70);

    display.button(Anchor::MIDDLE_CENTER, 135, 50, Theme::C_ACCENT, 15, 73, 70, [](void*) {
        show_notification("Mounting SD Card...", Theme::C_WARNING, 500);
        if (!filesystem.get_state()) {
            if (!filesystem.init()) {
                show_notification("Failed to mount!", Theme::C_ERROR, 2000);
            } else {
                show_notification("Successfully mounted!", Theme::C_SUCCESS, 2000);
            }
        } else {
            show_notification("Already Mounted!", Theme::C_ACCENT, 2000);
        }
    });
    display.text(Anchor::MIDDLE_CENTER, true, "Mount", Theme::FONT_LARGE, Theme::C_BG, 73, 70);

    display.button(Anchor::BOTTOM_CENTER, 280, 50, Theme::C_ERROR, 15, 0, -80, [](void*) {
        show_notification("Restarting...", Theme::C_ERROR, 1000);
        ESP.restart();
    });
    display.text(Anchor::BOTTOM_CENTER, true, "Restart Device", Theme::FONT_LARGE, Theme::C_WHITE, 0, -89);
}

void show_statusbar(){
    display.object(Anchor::TOP_LEFT, 115, 26, Theme::C_ACCENT_DARK, 10, 5, 5);
    if (!audio.get_status())
        display.text(Anchor::TOP_LEFT, true, "Paused...", Theme::FONT_MEDIUM, Theme::C_TEXT_PRIMARY, 10, 9);
    else
        display.text(Anchor::TOP_LEFT, true, "おかえり~~~ :3", Theme::FONT_MEDIUM, Theme::C_TEXT_PRIMARY, 10, 9);

    snprintf(vol_buf, sizeof(vol_buf), "Vol: %d%%", audio.get_volume());
    display.object(Anchor::TOP_RIGHT, 80, 26, Theme::C_ACCENT_DARK, 10, -5, 5);
    display.text(Anchor::TOP_RIGHT, true, vol_buf, Theme::FONT_MEDIUM, Theme::C_TEXT_PRIMARY, -10, 9);
}

void show_menubar(Page active) {
    // display.object(Anchor::BOTTOM_CENTER, 320, 64, Theme::C_ACCENT_DARK, 0);

    auto tab = [&](Anchor anchor, Page p, const char* label, int offset_x, int text_offset) {
        bool isActive = (active == p);
        uint16_t color = isActive ? Theme::C_ACCENT : Theme::C_ACCENT_DARK;

        display.button(anchor, 90, 44, color, Theme::R_MEDIUM, offset_x, -10, [p](void*) { page(p); });
        display.text(anchor, true, label, Theme::FONT_LARGE, isActive ? Theme::C_BG : Theme::C_TEXT_MUTED, offset_x + text_offset, -18);
    };

    tab(Anchor::BOTTOM_LEFT,   Page::PLAYER,    "Player",    10, 12);
    tab(Anchor::BOTTOM_CENTER, Page::DIRECTORY, "Files",     0,  0);
    tab(Anchor::BOTTOM_RIGHT,  Page::SETTINGS,  "Settings", -10, -5);
}

void show_notification(const char* text, uint16_t color, uint16_t delay_ms) {
    display.object(Anchor::TOP_LEFT, 115, 26, Theme::C_ACCENT_DARK, 10, 5, 5);
    display.text(Anchor::TOP_LEFT, true, text, Theme::FONT_MEDIUM, color, 10, 9);
    if (delay_ms > 0) {
        vTaskDelay(delay_ms / portTICK_PERIOD_MS);
        show_statusbar();
    }
}


// ======================================================================== //
// ============================ Keypad Handler ============================ //
// ======================================================================== //

constexpr uint8_t ROWS = 3;
constexpr uint8_t COLS = 3;
char keys[ROWS][COLS] = {
    {'>', 'p', '<'},
    {'+', '-', 'x'},
    {')', '(', 's'}
};
uint8_t pin_rows[ROWS] = {4, 5, 14};
uint8_t pin_cols[COLS] = {47, 48, 13};
uint8_t previous_brightness;

Keypad keypad = Keypad(makeKeymap(keys), pin_rows, pin_cols, ROWS, COLS); 

void keypress_check(){
    char key = keypad.getKey();

    if (key){

        uint8_t current_volume = audio.get_volume();
        uint8_t current_brightness = display.get_brightness();
        switch(key){
            ////////////////////////////////////////////////////////////////////
            //                          System Command                        //
            ////////////////////////////////////////////////////////////////////
            case 'x': 
                show_notification("Restarting...", Theme::C_ERROR, 1000);
                ESP.restart();
                break;

            ////////////////////////////////////////////////////////////////////
            //                          Player Command                        //
            ////////////////////////////////////////////////////////////////////
            case '>': 
                audio.next();
                show_notification("Next Track", Theme::C_ACCENT, 500);    
                break;

            case '<': 
                audio.previous();
                show_notification("Previous Track", Theme::C_ACCENT, 500);
                break;

            case 'p':
                audio.toggle();
                show_notification(audio.get_status() ? "Playing..." : "Paused...", Theme::C_SUCCESS, 2000);
                break;

            case '+':
                if (current_volume >= 95)
                    current_volume = 100;
                else
                    current_volume += 5;

                audio.set_volume(current_volume);
                show_statusbar();
                break;

            case '-':
                if (current_volume <= 5)
                    current_volume = 0;
                else
                    current_volume -= 5;

                audio.set_volume(current_volume);
                show_statusbar();
                break;

            ////////////////////////////////////////////////////////////////////
            //                         Display Command                        //
            ////////////////////////////////////////////////////////////////////
            case '(':
                if (current_brightness <= 5)
                    current_brightness = 0;
                else
                    current_brightness -= 5;

                display.set_brightness(current_brightness);  
                break;
            case ')':
                if (current_brightness >= 95)
                     current_brightness = 100;
                else
                    current_brightness += 5;

                display.set_brightness(current_brightness);  
                break;
            case 's':
                if (current_brightness == 0)
                    display.set_brightness(previous_brightness);
                else {
                    previous_brightness = current_brightness;
                    display.set_brightness(0);
                }
                break;
        }
        if (current_page == Page::SETTINGS)
            page(Page::SETTINGS, true);
    }
}
