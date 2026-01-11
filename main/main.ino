#include <Arduino.h>
#include <vector>
#include <Keypad.h>

#include "malkuth_fs.h"

#include "malkuth_display.h"
#include "malkuth_audio.h"

#include "fonts/Koruri-Regular12.h"
#include "fonts/Koruri-Regular8.h"
#include "fonts/RelaxedTypingMonoJP-Regular.ttf18.h"
#include "fonts/RelaxedTypingMonoJP-Regular.ttf24.h"

#include "fonts/Icons.h"
#include "fonts/IconsMenubar.h"

#include "flash_images/BootBg.h"
#include "flash_images/PlayerBg_a.h"
#include "flash_images/PlayerBg_b.h"
#include "flash_images/PlayerBg_c.h"
#include "flash_images/PlayerBg_d.h"

////////////////////////////////////////////////////////////////////
//                             Theming                            //
////////////////////////////////////////////////////////////////////
namespace Theme {
    constexpr uint16_t C_BG             = TFT_BLACK;
    constexpr uint16_t C_CARD           = TFT_DARKGREY;

    constexpr uint16_t C_ACCENT         = 0x5e1a;
    constexpr uint16_t C_ACCENT_DARK    = 0x11a7;
    constexpr uint16_t C_ACCENT_MUTED   = 0x1a09;
    constexpr uint16_t C_ACCENT_EXMUTED = 0x0904;

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
    const uint8_t*  FONT_LARGE      = RelaxedTypingMonoJP_Regular18;
    const uint8_t*  FONT_HUGE       = RelaxedTypingMonoJP_Regular24;

    const uint8_t*  FONT_ICON_LARGE       = IconsMenubar;
    const uint8_t*  FONT_ICON_SMALL       = Icons;

    const uint8_t*      IMG_BOOT        = BootBg;
    const uint8_t*      IMG_PLAYER      = PlayerBg_b;
    constexpr size_t    IMG_BOOT_SIZE   = sizeof(BootBg);
    constexpr size_t    IMG_PLAYER_SIZE = sizeof(PlayerBg_b);

}
////////////////////////////////////////////////////////////////////
//                        Global Variables                        //
////////////////////////////////////////////////////////////////////
MalkuthDisplay  display;
MalkuthFs       filesystem;
MalkuthAudio    audio;

AudioMetadata   metadata;

enum class Page : uint8_t {
    NONE,
    BOOT_SCREEN,
    PLAYER,
    FILES,
    SETTINGS,
};

Page    current_page = Page::NONE;
uint8_t current_volume      = 10;
uint8_t current_brightness  = 50;

char vol_buf[16]     = {};
char time_buf[12]    = {};
char notif_buf[64]   = {};
char idx_buf[12]     = {};    

// Player
float       position = 0.0f;
float       duration = 0.0f;
uint8_t     progress = 0;

// Files
char current_directory[128]  = "/";
char selected_directory[128] = "";
String  current_audiopath    = "";

std::vector<String> files;
uint16_t            previous_index      = 0;
uint16_t            files_count         = 0;
constexpr uint8_t   VISIBLE_ITEMS       = 5;
constexpr size_t    MAX_VISIBLE_STRING  = 29;
uint16_t            start_idx           = 0;
uint16_t            end_idx             = 0;

// Settings
constexpr uint16_t SLIDER_WIDTH = 280;
constexpr uint16_t SLIDER_HEIGHT = 20;

TimerHandle_t timer_notif = NULL;
TimerHandle_t timer_player = NULL;

////////////////////////////////////////////////////////////////////
//                        Function Prototype                      //
////////////////////////////////////////////////////////////////////
void page(Page p, bool redraw = false);
void page_boot();
void page_settings();
void page_player();
void page_files();
void page_files_listing(uint8_t index);

void show_statusbar();
void show_menubar(Page active);
void show_notification(const char* text, uint16_t color, uint16_t delay_ms);

void check_keypress();
void check_progress();
void check_metadata();

void start_player_timer();
void stop_player_timer();

void format_duration(char* out, size_t len, float seconds);
String format_elipsis(const String& text, uint8_t length);

bool is_audio_file(const String& f);
bool is_image_file(const String& f);
bool is_directory(const String& f);

////////////////////////////////////////////////////////////////////
//                         Actual Program                         //
////////////////////////////////////////////////////////////////////
void setup() {
    Serial.begin(115200);

    display.init(0);
    page(Page::BOOT_SCREEN);

    delay(1); // A bit weird fix because somehow the SD Card failed to be initialized if i dont add delay first

    if (!filesystem.init())
        display.text(Anchor::BOTTOM_CENTER, false, "SD Card is failed to be mounted!", Theme::FONT_SMALL, Theme::C_ERROR, 0, -10);
    else
        display.text(Anchor::BOTTOM_CENTER, false, "SD Card is successfully mounted!", Theme::FONT_SMALL, Theme::C_SUCCESS, 0, -10);

    audio.set_sdfs(filesystem.get_sdfs());
    // display.set_sdfs(filesystem.get_sdfs());

    if (!audio.init())
        display.text(Anchor::BOTTOM_CENTER, false, "Audio is failed to be intialized!", Theme::FONT_SMALL, Theme::C_ERROR, 0, -30);
    else 
        display.text(Anchor::BOTTOM_CENTER, false, "Audio is successfully intialized!", Theme::FONT_SMALL, Theme::C_SUCCESS, 0, -30);

    // Initial value
    display.set_brightness(current_brightness);
    audio.set_volume(current_volume);
}

void loop() {
    audio.loop();

    check_metadata();
    check_keypress();
    display.check_buttons();
}

////////////////////////////////////////////////////////////////////
//                          Page Element                          //
////////////////////////////////////////////////////////////////////
void page(Page p, bool redraw) {
    if (current_page == p && !redraw) return;

    if (current_page == Page::PLAYER) stop_player_timer();

    current_page = p;

    if (current_page == Page::PLAYER) start_player_timer();

    display.buttons_clear();
    display.clear();

    if (current_page != Page::BOOT_SCREEN && current_page != Page::PLAYER) {
        show_statusbar();
        show_menubar(current_page);
    }

    switch (p) {
        case Page::BOOT_SCREEN: page_boot(); break;
        case Page::PLAYER:      page_player(); break;
        case Page::FILES:       page_files(); break;
        case Page::SETTINGS:    page_settings(); break;
        default: break;
    }
}

////////////////////////////////////////////////////////////////////
//                           Page Boot                            //
////////////////////////////////////////////////////////////////////
void page_boot() {
    display.image(ImageType::FLASH, Theme::IMG_BOOT, Theme::IMG_BOOT_SIZE);
    display.text(Anchor::TOP_CENTER,    true, "おかえり~~~ :3",       Theme::FONT_HUGE,  Theme::C_BG, 0, 20);
    display.text(Anchor::BOTTOM_CENTER, true, " > Touch to start <", Theme::FONT_HUGE, Theme::C_TEXT_PRIMARY, 0, -60);
 
    display.button(Anchor::MIDDLE_CENTER, 320, 480, 0, 0, [](void*) {
        page(Page::PLAYER);
    });
}

////////////////////////////////////////////////////////////////////
//                           Page Player                          //
////////////////////////////////////////////////////////////////////
void page_player() {
    // TODO : Maybe i will fix later (Cover image loading)
    //
    // if (audio.get_covertype() == ImageType::NONE)
    //     display.object(Anchor::MIDDLE_CENTER, 200, 200, Theme::C_CARD, 10, 0, -80);
    //     display.image(ImageType::FLASH, PlayerBg, sizeof(BootBg));
    // else
    //     display.image(audio.get_covertype(), audio.get_coverpath(), 200, 200, 60, 40);

    display.image(ImageType::FLASH, Theme::IMG_PLAYER, Theme::IMG_PLAYER_SIZE);

    show_statusbar();
    show_menubar(current_page);

    // Artist and title text
    display.text(Anchor::MIDDLE_CENTER, true, metadata.title.c_str(), Theme::FONT_HUGE, Theme::C_TEXT_PRIMARY, 0, 50);
    display.text(Anchor::MIDDLE_CENTER, true, metadata.artist.c_str(), Theme::FONT_LARGE, Theme::C_TEXT_PRIMARY, 0, 80);

    // Duration text
    format_duration(time_buf, sizeof(time_buf), metadata.duration);
    display.object(Anchor::BOTTOM_RIGHT, 30, 12, Theme::C_ACCENT_DARK, 5, -20, -113);
    display.text(Anchor::BOTTOM_RIGHT, true, time_buf, Theme::FONT_SMALL, Theme::C_WHITE, -20, -110);

    // Button for Play/Pause
    display.button(Anchor::BOTTOM_CENTER, 35, 35, Theme::C_ACCENT_MUTED, -1, 0, -80, [](void*){
        audio.toggle();        
        display.object(Anchor::BOTTOM_CENTER, 35, 35, Theme::C_ACCENT_MUTED, -1, 0, -80);
        display.text(Anchor::BOTTOM_CENTER, true, 
            audio.get_status() ? "||" : "▶", 
            Theme::FONT_LARGE, Theme::C_WHITE, 
            audio.get_status() ? -1 : 2, 
            audio.get_status() ? -85 : -82
        );
    });
    display.text(Anchor::BOTTOM_CENTER, true, 
            audio.get_status() ? "||" : "▶", 
            Theme::FONT_LARGE, Theme::C_WHITE, 
            audio.get_status() ? -1 : 2, 
            audio.get_status() ? -85 : -82
    );

    // Button for < (Previous)
    display.button(Anchor::BOTTOM_CENTER, 35, 35, Theme::C_ACCENT_MUTED, -1, -46, -80, [](void*){
        audio.previous();
        show_notification("Previous Track", Theme::C_ACCENT, 500);
    });
    display.text(Anchor::BOTTOM_CENTER, true, "<", Theme::FONT_LARGE, Theme::C_WHITE, -48, -85);

    // Button for > (Next)
    display.button(Anchor::BOTTOM_CENTER, 35, 35, Theme::C_ACCENT_MUTED, -1, 47, -80, [](void*){
        audio.next();
        show_notification("Next Track", Theme::C_ACCENT, 500);
    });
    display.text(Anchor::BOTTOM_CENTER, true, ">", Theme::FONT_LARGE, Theme::C_WHITE, 47, -85);

    if (metadata.duration <= 0) {
        display.text(Anchor::BOTTOM_CENTER, true, "Select a Directory to Play!", Theme::FONT_LARGE, Theme::C_TEXT_PRIMARY, 0, -150);
    }
}

void check_metadata(){
    if (audio.get_update()) {
        metadata = audio.get_metadata();
        audio.yeah_i_have_updated();

        current_audiopath = String(audio.get_audiopath());

        if (!audio.is_actually_audio()) {
            return;
        }

        if (current_page == Page::PLAYER) {
            display.image(ImageType::FLASH, Theme::IMG_PLAYER, Theme::IMG_PLAYER_SIZE, 320, 85, 0, 280);
            display.text(Anchor::MIDDLE_CENTER, true, metadata.title.c_str(), Theme::FONT_HUGE, Theme::C_TEXT_PRIMARY, 0, 50);
            display.text(Anchor::MIDDLE_CENTER, true, metadata.artist.c_str(), Theme::FONT_LARGE, Theme::C_TEXT_PRIMARY, 0, 80);

            // Duration text
            format_duration(time_buf, sizeof(time_buf), metadata.duration);
            display.object(Anchor::BOTTOM_RIGHT, 30, 12, Theme::C_ACCENT_DARK, 5, -20, -113);
            display.text(Anchor::BOTTOM_RIGHT, true, time_buf, Theme::FONT_SMALL, Theme::C_WHITE, -20, -110);
        }

        if (current_page == Page::FILES && (strcmp(current_directory, selected_directory) == 0))
            page_files_listing(start_idx);
    }
}
////////////////////////////////////////////////////////////////////
//                      Progress Bar Update                       //
////////////////////////////////////////////////////////////////////
static void player_update_timer(TimerHandle_t xTimer) {
    check_progress();
}

void start_player_timer() {
    if (!timer_player) {
        timer_player = xTimerCreate(
            "Malkuth: Progress timer",
            pdMS_TO_TICKS(1000),
            pdTRUE,
            nullptr,
            player_update_timer
        );
    }

    xTimerStop(timer_player, 0);
    xTimerStart(timer_player, 0);
}

void stop_player_timer() {
    if (timer_player) {
        xTimerStop(timer_player, 0);
    }
}

void check_progress(){
    // The progress bar
    if (current_page != Page::PLAYER)
        return;

    if (!audio.get_status())
        return;

    duration = metadata.duration;
    if (duration <= 0)
        return;

    position = audio.get_position();
    progress = constrain(static_cast<uint8_t>((position / duration) * 100), 0, 100);
        
    // Player UI Update
    format_duration(time_buf, sizeof(time_buf), position);
    display.bar(Anchor::BOTTOM_CENTER, 280, 10, Theme::C_CARD, Theme::C_ACCENT, 5, 0, -130, progress, [](void*){
        TouchData p = display.get_touchdata();
        if (p.z == 0) return;
        
        uint8_t val = map(constrain(p.x, 5, 285), 5, 285, 0, 100);

        progress = val;
        Serial.println(progress);
        audio.set_position(progress);
        display.bar(Anchor::BOTTOM_CENTER, 280, 10, Theme::C_CARD, Theme::C_ACCENT, 5, 0, -130, progress, nullptr);
    });
    display.object(Anchor::BOTTOM_LEFT, 30, 12, Theme::C_ACCENT_DARK, 5, 21, -113);
    display.text(Anchor::BOTTOM_LEFT, true, time_buf, Theme::FONT_SMALL, Theme::C_WHITE, 20, -110);
}

////////////////////////////////////////////////////////////////////
//                           Page Files                          //
////////////////////////////////////////////////////////////////////
void page_files() {
    display.text(Anchor::TOP_LEFT, true, "", Theme::FONT_ICON_LARGE, Theme::C_WHITE, 10, 35);     // Folder Icon
    display.text(Anchor::TOP_LEFT, true, "File Chooser", Theme::FONT_HUGE, Theme::C_WHITE, 40, 38);

    // Previous dir button
    display.button(Anchor::TOP_CENTER, 300, 40, Theme::C_BG, 10, 0, 75, [start_idx](void*) {
        size_t len = strlen(current_directory);
        if (len > 1) {
            for (int i = len - 2; i >= 0; --i) {
                if (current_directory[i] == '/') {
                    current_directory[i + 1] = '\0';
                    break;
                }
            }
            page_files_listing(previous_index);
        }
    });
    display.text(Anchor::TOP_LEFT, true, "←   Previous Directory", Theme::FONT_LARGE, Theme::C_TEXT_PRIMARY, 20, 84);

    // Directory Listing
    page_files_listing(0);
}

void page_files_listing(uint8_t index) {
    display.buttons_clear_temp();

    // Refresh the view 
    display.object(Anchor::TOP_CENTER, 320, 240, TFT_BLACK, 0, 0, 120);
    display.object(Anchor::BOTTOM_RIGHT, 30, 30, Theme::C_BLACK, -1, -60, -80);
    display.object(Anchor::BOTTOM_RIGHT, 30, 30, Theme::C_BLACK, -1, -20, -80);
    
    files       = filesystem.get_directory_files(current_directory);
    files_count = files.size();

    start_idx   = index;
    end_idx     = start_idx + VISIBLE_ITEMS;

    if (end_idx > files_count) 
        end_idx = files_count;

    // List files and directory
    snprintf(idx_buf, sizeof(idx_buf), "%02u/%02u", end_idx, files_count);
    display.object(Anchor::TOP_RIGHT, 50, 30, Theme::C_BLACK, 0, -20, 38);
    display.text(Anchor::TOP_RIGHT, true, idx_buf, Theme::FONT_LARGE, Theme::C_WHITE, -20, 42);
    
    for (uint8_t i = 0; i < VISIBLE_ITEMS; ++i) {
        uint8_t idx = start_idx + i;
        if (idx >= files_count) break;

        int16_t y_offset = 75 + ((i + 1) * 45);
        const String& file = files[idx];

        String displayed_text = format_elipsis(file, MAX_VISIBLE_STRING);

        bool currently_played = (current_audiopath == (String(current_directory) + file)) && (current_audiopath != "");

        display.button( 
            Anchor::TOP_CENTER, 300, 40, 
            currently_played ? Theme::C_ACCENT_MUTED: Theme::C_ACCENT_EXMUTED, 
            currently_played ? 0 : 10, 
            0, y_offset, 
        [file, start_idx, idx](void*) {
            if (is_directory(file)) {
                strncat(current_directory, file.c_str(), sizeof(current_directory) - strlen(current_directory) - 1);
                previous_index = start_idx;
                page_files_listing(0);
            } else if (is_audio_file(file)){
                // if (strcmp(selected_directory, current_directory) == 0)
                //     audio.set_index(idx);
                // else
                //     show_notification("You haven't selected this dir!", Theme::C_ACCENT, 1000);

                if (strcmp(selected_directory, current_directory) == 0)
                    audio.set_index(idx);
                else {
                    strcpy(selected_directory, current_directory);
                    audio.process_directory(selected_directory);

                    audio.set_index(idx);
                }
            }
        }, nullptr, true);

        if (currently_played){
            display.object(Anchor::TOP_LEFT, 10, 40, Theme::C_ACCENT, 0, 0, y_offset);
        }

        if (is_audio_file(file))
            display.text(Anchor::TOP_LEFT, true, "", Theme::FONT_ICON_SMALL, Theme::C_TEXT_PRIMARY, 20, 82 + (i + 1) * 45); // Audio Icon
        else if (is_image_file(file))
            display.text(Anchor::TOP_LEFT, true, "", Theme::FONT_ICON_SMALL, Theme::C_TEXT_PRIMARY, 20, 82 + (i + 1) * 45); // Image Icon
        else if (is_directory(file))
            display.text(Anchor::TOP_LEFT, true, "", Theme::FONT_ICON_SMALL, Theme::C_TEXT_PRIMARY, 20, 82 + (i + 1) * 45); // Folder Icon
        else
            display.text(Anchor::TOP_LEFT, true, "", Theme::FONT_ICON_SMALL, Theme::C_TEXT_PRIMARY, 20, 82 + (i + 1) * 45); // File Icon

        display.text(Anchor::TOP_LEFT, true, displayed_text.c_str(), Theme::FONT_LARGE, Theme::C_TEXT_PRIMARY, 45, 84 + (i + 1) * 45);
    }

    display.text(Anchor::BOTTOM_LEFT, true, format_elipsis(String(current_directory), 78).c_str(), Theme::FONT_SMALL, Theme::C_TEXT_MUTED, 0, -116);

    // Select the directory
    bool selected = (strcmp(selected_directory, current_directory) == 0);
    display.button(Anchor::BOTTOM_LEFT, 85, 30, selected ? Theme::C_ACCENT : Theme::C_ACCENT_DARK, 10, 20, -80, [](void*) {
        strcpy(selected_directory, current_directory);
        audio.process_directory(selected_directory);
    }, nullptr, true);
    display.text(Anchor::BOTTOM_LEFT, true, "Select", Theme::FONT_LARGE, selected ? Theme::C_BG : Theme::C_TEXT_PRIMARY, 33, -81);

    // Return to the selected directory
    display.object(Anchor::BOTTOM_CENTER, 85, 30, Theme::C_BLACK, 0, 0, -80);
    if ((!(strcmp(selected_directory, "") == 0)) && (!(strcmp(selected_directory, current_directory) == 0))) {
        display.button(Anchor::BOTTOM_CENTER, 85, 30, Theme::C_ACCENT_DARK, 10, 0, -80, [](void*) {
            strcpy(current_directory, selected_directory);
            page_files_listing(0);
        }, nullptr, true);
        display.text(Anchor::BOTTOM_CENTER, true, "Now", Theme::FONT_LARGE, Theme::C_TEXT_PRIMARY, 0, -81);
    }

    // Scroll up
    if (start_idx > 0) {
        display.button(Anchor::BOTTOM_RIGHT, true, 30, 30, Theme::C_ACCENT, -1, -60, -80, [start_idx](void*) {
            page_files_listing(start_idx - 1);
        }, nullptr, true);
        display.text(Anchor::BOTTOM_RIGHT, true, "▲", Theme::FONT_LARGE, Theme::C_BLACK, -63, -80);
    }

    // Scroll down
    if (end_idx < files_count) {
        display.button(Anchor::BOTTOM_RIGHT, true, 30, 30, Theme::C_ACCENT, -1, -20, -80, [start_idx](void*) {
            page_files_listing(start_idx + 1);
        }, nullptr, true);
        display.text(Anchor::BOTTOM_RIGHT, true, "▼", Theme::FONT_LARGE, Theme::C_BLACK, -23, -80);
    }
}

void page_settings() {
    display.text(Anchor::TOP_LEFT, true, "", Theme::FONT_ICON_LARGE, Theme::C_WHITE, 10, 35);
    display.text(Anchor::TOP_LEFT, true, "Settings", Theme::FONT_HUGE, Theme::C_WHITE, 40, 38);

    // Brightness Slider
    current_brightness = display.get_brightness();
    display.text(Anchor::TOP_LEFT, true, "", Theme::FONT_ICON_SMALL, Theme::C_WHITE, 10, 78); // Brightness Icon
    display.text(Anchor::TOP_LEFT, true, "Brightness", Theme::FONT_LARGE, Theme::C_WHITE, 30, 80);
    display.bar(Anchor::TOP_CENTER, SLIDER_WIDTH, SLIDER_HEIGHT, Theme::C_TEXT_MUTED, Theme::C_WARNING, 10, 0, 110, current_brightness,
        [&current_brightness](void*) {
            TouchData p = display.get_touchdata();
            if (p.z == 0) return;

            uint8_t val = map(constrain(p.x, 10, SLIDER_WIDTH), 10, SLIDER_WIDTH, 0, 100);
            if (val != current_brightness) {
                current_brightness = val;
                display.set_brightness(current_brightness);
                display.bar(Anchor::TOP_CENTER, SLIDER_WIDTH, SLIDER_HEIGHT, Theme::C_TEXT_MUTED, Theme::C_WARNING, 10, 0, 110, current_brightness, nullptr);
            }
        }, nullptr);

    // Volume Slider
    current_volume = audio.get_volume();
    display.text(Anchor::TOP_LEFT, true, "", Theme::FONT_ICON_SMALL, TFT_WHITE, 10, 148); // Volume Icon
    display.text(Anchor::TOP_LEFT, true, "Volume", Theme::FONT_LARGE, TFT_WHITE, 30, 150);
    display.bar(Anchor::TOP_CENTER, SLIDER_WIDTH, SLIDER_HEIGHT, Theme::C_TEXT_MUTED, Theme::C_ACCENT, 10, 0, 180, current_volume,
        [&current_volume](void*) {
            TouchData p = display.get_touchdata();
            if (p.z == 0) return;

            uint8_t val = map(constrain(p.x, 10, SLIDER_WIDTH), 10, SLIDER_WIDTH, 0, 100);
            if (val != current_volume) {
                current_volume = val;
                audio.set_volume(current_volume);
                display.bar(Anchor::TOP_CENTER, SLIDER_WIDTH, SLIDER_HEIGHT, Theme::C_TEXT_MUTED, Theme::C_ACCENT, 10, 0, 180, current_volume, nullptr);
                
                snprintf(vol_buf, sizeof(vol_buf), "Vol: %-3d%%", audio.get_volume());
                display.object(Anchor::TOP_RIGHT, 80, 26, Theme::C_ACCENT_EXMUTED, 10, -5, 5);
                display.text(Anchor::TOP_RIGHT, true, vol_buf, Theme::FONT_MEDIUM, Theme::C_TEXT_PRIMARY, -10, 9);
            }
        }, nullptr);

    // Boot Screen Button
    display.button(Anchor::MIDDLE_CENTER, 280, 50, Theme::C_TEXT_MUTED, 15, 0, 5, [](void*) {
        page(Page::BOOT_SCREEN);
    });
    display.text(Anchor::MIDDLE_CENTER, true, "Boot Screen", Theme::FONT_LARGE, Theme::C_BG, 0, 5);

    // Dump to Serial Monitor Button
    display.button(Anchor::MIDDLE_CENTER, 135, 50, Theme::C_ACCENT_DARK, 15, -75, 70, [](void*) {
        char task_list_buf[1024];
        char runtime_buf[1024];

        Serial.println("=========== HEAP INFO ===========");
        Serial.printf("Free Heap (8-bit)        : %u bytes\n", (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT));
        Serial.printf("Min Free Heap (8-bit)    : %u bytes\n", (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));
        Serial.printf("Largest Free Block (8bit): %u bytes\n", (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        Serial.printf("Free Internal (DMA/32bit): %u bytes\n", (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

    #ifdef BOARD_HAS_PSRAM
        if (psramFound()) {
            Serial.println("=========== PSRAM INFO ===========");
            Serial.printf("Free PSRAM               : %u bytes\n", (unsigned)ESP.getFreePsram());
            Serial.printf("Total PSRAM              : %u bytes\n", (unsigned)ESP.getPsramSize());
        }
    #endif

        Serial.println("=========== TASK LIST ===========");
        task_list_buf[0] = '\0';
        vTaskList(task_list_buf);
        Serial.print(task_list_buf);
        if (task_list_buf[0] != '\0' && task_list_buf[strlen(task_list_buf)-1] != '\n') Serial.println();

        Serial.println("========= CPU USAGE (RUNTIME STATS) =========");
        runtime_buf[0] = '\0';
        vTaskGetRunTimeStats(runtime_buf);
        Serial.print(runtime_buf);
        if (runtime_buf[0] != '\0' && runtime_buf[strlen(runtime_buf)-1] != '\n') Serial.println();

        UBaseType_t hw = uxTaskGetStackHighWaterMark(NULL);
        Serial.println("=========== STACK INFO ===========");
        Serial.printf("Current task stack high-water mark: %u words (~%u bytes)\n",
                  (unsigned)hw, (unsigned)(hw * sizeof(StackType_t)));


        show_notification("Resouces Dumped!", Theme::C_SUCCESS, 2000);

    });
    display.text(Anchor::MIDDLE_CENTER, true, "", Theme::FONT_ICON_SMALL, Theme::C_WHITE, -100, 72); // Dump Icon
    display.text(Anchor::MIDDLE_CENTER, true, "Dump", Theme::FONT_LARGE, Theme::C_WHITE, -60, 72);

    // Mount SD Card Button
    display.button(Anchor::MIDDLE_CENTER, 135, 50, Theme::C_ACCENT, 15, 73, 70, [](void*) {
        show_notification("Mounting SD Card...", Theme::C_WARNING, 1000);
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
    display.text(Anchor::MIDDLE_CENTER, true, "", Theme::FONT_ICON_SMALL, Theme::C_BG, 45, 72); // Mount Icon
    display.text(Anchor::MIDDLE_CENTER, true, "Mount", Theme::FONT_LARGE, Theme::C_BG, 88, 72);

    // Restart ESP Button
    display.button(Anchor::BOTTOM_CENTER, 280, 50, Theme::C_ERROR, 15, 0, -80, [](void*) {
        show_notification("Restarting...", Theme::C_ERROR, 1000);
        ESP.restart();
    });
    display.text(Anchor::BOTTOM_CENTER, true, "Restart Device", Theme::FONT_LARGE, Theme::C_WHITE, 0, -89);
}

// ======================================================================== //
// =========================== Common UI Element ========================== //
// ======================================================================== //

void show_statusbar(){
    // Uncomment this if you want un-pilled bar
    // display.object(Anchor::TOP_CENTER, 320, 46, Theme::C_ACCENT_DARK, 0);

    // Or this if you want floating bar
    // display.object(Anchor::TOP_CENTER, 310, 26, Theme::C_ACCENT_DARK, 10, 5, 5);

    display.object(Anchor::TOP_LEFT, 115, 26, Theme::C_ACCENT_EXMUTED, 10, 5, 5);
    if (!audio.get_status())
        display.text(Anchor::TOP_LEFT, true, "Paused...", Theme::FONT_MEDIUM, Theme::C_TEXT_PRIMARY, 10, 9);
    else
        display.text(Anchor::TOP_LEFT, true, "おかえり~~~ :3", Theme::FONT_MEDIUM, Theme::C_TEXT_PRIMARY, 10, 9);

    snprintf(vol_buf, sizeof(vol_buf), "Vol: %-3d%%", audio.get_volume());
    display.object(Anchor::TOP_RIGHT, 80, 26, Theme::C_ACCENT_EXMUTED, 10, -5, 5);
    display.text(Anchor::TOP_RIGHT, true, vol_buf, Theme::FONT_MEDIUM, Theme::C_TEXT_PRIMARY, -10, 9);
}

void show_menubar(Page active) {
    // Uncomment this if you want un-pilled bar
    // display.object(Anchor::BOTTOM_CENTER, 320, 64, Theme::C_ACCENT_DARK, 0);

    auto tab = [&](Anchor anchor, Page p, const char* label, int offset_x, int text_offset) {
        bool is_active = (active == p);
        uint16_t color = is_active ? Theme::C_ACCENT : Theme::C_ACCENT_DARK;

        display.button(anchor, 90, 44, color, Theme::R_MEDIUM, offset_x, -10, [p](void*) { page(p); });
        display.text(anchor, true, label, Theme::FONT_ICON_LARGE, is_active ? Theme::C_BG : Theme::C_TEXT_MUTED, offset_x + text_offset, -10);
    };

    tab(Anchor::BOTTOM_LEFT,   Page::PLAYER,    "",    10, 30); // Music Icon
    tab(Anchor::BOTTOM_CENTER, Page::FILES,     "",     0,  0); // File Icon
    tab(Anchor::BOTTOM_RIGHT,  Page::SETTINGS,  "",   -10, -30);   // Gear Icon
}

static void notif_timeout(TimerHandle_t xTimer) {
    if (current_page == Page::SETTINGS || current_page == Page::FILES)
        display.object(Anchor::TOP_LEFT, 200, 26, Theme::C_BLACK, 0, 5, 5);
    show_statusbar();
}

void show_notification(const char* text, uint16_t color, uint16_t delay_ms) {
    if (!text) return;

    size_t len = strlen(text);
    uint16_t width;

    if (len > 21)       width = 200;
    else if (len > 15)  width = 160;
    else                width = 115;

    display.object(Anchor::TOP_LEFT, width, 26, Theme::C_ACCENT_EXMUTED, 10, 5, 5);
    display.text(Anchor::TOP_LEFT, true, text, Theme::FONT_MEDIUM, color, 10, 9);

    if (delay_ms == 0)
        return;

    if (!timer_notif) {
        timer_notif = xTimerCreate(
            "Malkuth: Notification Timer",
            pdMS_TO_TICKS(delay_ms),
            pdFALSE,
            NULL,
            notif_timeout
        );
    }

    xTimerStop(timer_notif, 0);
    xTimerChangePeriod(timer_notif, pdMS_TO_TICKS(delay_ms), 0);
    xTimerStart(timer_notif, 0);
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
uint8_t previous_brightness = current_brightness;

Keypad keypad = Keypad(makeKeymap(keys), pin_rows, pin_cols, ROWS, COLS); 

void check_keypress(){
    char key = keypad.getKey();
    if (!key) return;

    if (current_page == Page::BOOT_SCREEN) {
        page(Page::PLAYER);
        return;
    }
    current_volume      = audio.get_volume();
    current_brightness  = display.get_brightness();

    switch(key){
        ////////////////////////////////////////////////////////////////////
        //                          System Command                        //
        ////////////////////////////////////////////////////////////////////
        case 'x':
            display.clear();
            display.text(Anchor::MIDDLE_CENTER, true, "Shutting Down... :3", Theme::FONT_LARGE, Theme::C_ACCENT);
            vTaskDelay(1500/ portTICK_PERIOD_MS);
            esp_deep_sleep_start();
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
            if (current_page == Page::PLAYER){
                display.object(Anchor::BOTTOM_CENTER, 35, 35, Theme::C_ACCENT_MUTED, -1, 0, -80);
                display.text(Anchor::BOTTOM_CENTER, true, 
                    audio.get_status() ? "||" : "▶", 
                    Theme::FONT_LARGE, Theme::C_WHITE, 
                    audio.get_status() ? -1 : 2, 
                    audio.get_status() ? -85 : -82
                );
            }
            show_statusbar();
            break;

        case '+':
            if (current_volume >= 95)
                current_volume = 100;
            else
                current_volume += 2;

            audio.set_volume(current_volume);
            
            snprintf(vol_buf, sizeof(vol_buf), "Vol: %-3d%%", audio.get_volume());
            display.object(Anchor::TOP_RIGHT, 80, 26, Theme::C_ACCENT_EXMUTED, 10, -5, 5);
            display.text(Anchor::TOP_RIGHT, true, vol_buf, Theme::FONT_MEDIUM, Theme::C_TEXT_PRIMARY, -10, 9);

            if (current_page == Page::SETTINGS)
                display.bar(Anchor::TOP_CENTER, SLIDER_WIDTH, SLIDER_HEIGHT, Theme::C_TEXT_MUTED, Theme::C_ACCENT, 10, 0, 180, current_volume, nullptr);

            break;

        case '-':
            if (current_volume <= 5)
                current_volume = 0;
            else
                current_volume -= 2;

            audio.set_volume(current_volume);
            snprintf(vol_buf, sizeof(vol_buf), "Vol: %-3d%%", audio.get_volume());
            display.object(Anchor::TOP_RIGHT, 80, 26, Theme::C_ACCENT_EXMUTED, 10, -5, 5);
            display.text(Anchor::TOP_RIGHT, true, vol_buf, Theme::FONT_MEDIUM, Theme::C_TEXT_PRIMARY, -10, 9);

            if (current_page == Page::SETTINGS)
                display.bar(Anchor::TOP_CENTER, SLIDER_WIDTH, SLIDER_HEIGHT, Theme::C_TEXT_MUTED, Theme::C_ACCENT, 10, 0, 180, current_volume, nullptr);

            break;

        ////////////////////////////////////////////////////////////////////
        //                         Display Command                        //
        ////////////////////////////////////////////////////////////////////
        case '(':
            if (current_brightness <= 5)
                current_brightness = 1;
            else
                current_brightness -= 2;

            display.set_brightness(current_brightness);

            if (current_page == Page::SETTINGS)
                display.bar(Anchor::TOP_CENTER, SLIDER_WIDTH, SLIDER_HEIGHT, Theme::C_TEXT_MUTED, Theme::C_WARNING, 10, 0, 110, current_brightness, nullptr);

            break;
        case ')':
            if (current_brightness >= 95)
                  current_brightness = 100;
            else
                current_brightness += 2;

            display.set_brightness(current_brightness);

            if (current_page == Page::SETTINGS)
                display.bar(Anchor::TOP_CENTER, SLIDER_WIDTH, SLIDER_HEIGHT, Theme::C_TEXT_MUTED, Theme::C_WARNING, 10, 0, 110, current_brightness, nullptr);

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
}

////////////////////////////////////////////////////////////////////
//                        Helper Functions                        //
////////////////////////////////////////////////////////////////////
void format_duration(char* out, size_t len, float seconds) {
    uint32_t t = static_cast<uint32_t>(seconds);
    snprintf(out, len, "%02u:%02u", t / 60, t % 60);
}

String format_elipsis(const String& text, uint8_t length){
    if (text.length() <= length || length < 5) {
        return text;
    }

    const char* ellipsis = "...";
    size_t keep = length - 3;
    size_t front = keep / 2;
    size_t back  = keep - front;

    return text.substring(0, front) + ellipsis +
           text.substring(text.length() - back);
}

bool is_audio_file(const String& f) {
    return f.endsWith(".mp3") || f.endsWith(".wav") || f.endsWith(".flac") || f.endsWith(".aac");
}

bool is_image_file(const String& f) {
    return f.endsWith(".png") || f.endsWith(".jpg") || f.endsWith(".jpeg");
}

bool is_directory(const String& f) {
    return f.endsWith("/");
}
