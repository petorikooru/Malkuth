#include <SPI.h>
#include <TFT_eSPI.h>
#include <PNGdec.h>
#include <FT6236.h>

#include <SdFat.h>

#include "malkuth_helper.h"

#ifndef MAX_IMAGE_WIDTH
    #define MAX_IMAGE_WIDTH 320
#endif

#ifndef MAX_TEXT_LENGTH
    #define MAX_TEXT_LENGTH 128
#endif

#ifndef PIN_BL
    #define PIN_BL 3
#endif

enum class DisplayType : uint8_t {
    IMAGE,
    TEXT,
    OBJECT,
    BAR,

    CLEAR
};

// Anchor value (wrapper) (using the value from tft_espi one as the base)
enum class Anchor : uint8_t {
    TOP_LEFT    = TL_DATUM,
    TOP_CENTER  = TC_DATUM,
    TOP_RIGHT   = TR_DATUM, 

    MIDDLE_LEFT    = ML_DATUM,
    MIDDLE_CENTER  = MC_DATUM,
    MIDDLE_RIGHT   = MR_DATUM,

    BOTTOM_LEFT    = BL_DATUM,
    BOTTOM_CENTER  = BC_DATUM,
    BOTTOM_RIGHT   = BR_DATUM,
};

struct TouchData {
    uint16_t x, y, z;
};

struct DisplayCommand {
    DisplayType type;

    union {
        struct {
            int16_t     offset_x, offset_y;
            uint16_t    size_x, size_y;

            uint16_t    color;
            int8_t      roundness;   // negative value = circle
            Anchor      anchor;
        } object;

        struct {
            ImageType       type;
            size_t          data_size;
            const uint8_t*  data;
            char            path[MAX_TEXT_LENGTH];

            uint16_t    size_x, size_y;
            uint16_t    offset_x, offset_y;
        } image;
        
        struct {
            int16_t offset_x, offset_y;

            char            string[MAX_TEXT_LENGTH];
            const uint8_t*  typeface;
            uint16_t        color;
            bool            transparent;
            Anchor          anchor;
        } text;

        struct {
            int16_t     offset_x, offset_y;
            uint16_t    size_x, size_y;

            uint16_t    color_bg;
            uint16_t    color_fill;
            int8_t      roundness;

            uint8_t     value;
        } bar;
    } payload;
};

struct Button {
    int16_t     offset_x, offset_y;
    uint16_t    size_x, size_y;
    bool        is_bar;

    std::function<void(void*)>  func;
    void*                       param;
};

class MalkuthDisplay {
private:
    TFT_eSPI _tft;
    FT6236   _ts    = FT6236();
    PNG      _png;
    // SdFs*    _sd;

    uint8_t  _brightness = 50;
    uint16_t _bg_color = TFT_BLACK;
    uint16_t _fg_color = TFT_WHITE;

    uint16_t  _constrain_x_start, _constrain_x_end;
    uint16_t  _constrain_y_start, _constrain_y_end;
    uint16_t  _constrain_width, _constrain_height;
    uint16_t  _constrain_counter = 1;

    bool     _ts_exist;
    uint16_t _ts_x = 0;
    uint16_t _ts_y = 0;
    uint32_t _hold_timeout;

    QueueHandle_t _queue_display        = nullptr;
    TaskHandle_t  _taskhandle_display   = nullptr;

    std::vector<Button> _buttons;
    std::vector<Button> _buttons_temp;

    static void task_display(void* parameters);

    static void handle_command(MalkuthDisplay* self, const DisplayCommand& cmd);

    int16_t calculate_anchor_x(Anchor anchor, uint16_t width);
    int16_t calculate_anchor_y(Anchor anchor, uint16_t height);

    static void draw_png_flash(MalkuthDisplay *self, const DisplayCommand&cmd);
    // static void draw_jpg(MalkuthDisplay *self, const DisplayCommand&cmd);
    
    // static bool render_jpg(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap);
    static int  render_png(PNGDRAW* png_draw);
    static int  render_png_constrained(PNGDRAW* png_draw);
    
    static void draw_image(MalkuthDisplay* self, const DisplayCommand& cmd);
    static void draw_text(MalkuthDisplay* self, const DisplayCommand& cmd);
    static void draw_object(MalkuthDisplay* self, const DisplayCommand& cmd);
    static void draw_bar(MalkuthDisplay* self, const DisplayCommand& cmd);

    void _buttons_check();

public:
    void init(),
         init(const uint8_t core);

    void image(
            const ImageType type, 
            const uint8_t*  image, 
            const size_t    data_size
    ),   image(
            const ImageType type, 
            const uint8_t*  image, 
            const size_t    data_size,

            const uint16_t  size_x, const uint16_t size_y,
            const uint16_t offset_x, const uint16_t offset_y

    ),   image(
            const ImageType type,
            const char*     path,

            const uint16_t size_x, const uint16_t size_y,
            const int16_t offset_x, const int16_t offset_y
    );

    void text(
            Anchor anchor, bool transparent,
            const char* string, const uint8_t* typeface,
            const uint16_t color
    ),   text(
            Anchor anchor, bool transparent,
            const char* string, const uint8_t* typeface,
            const uint16_t color,
            const int16_t offset_x, const int16_t offset_y
    ),   text( // only for testing though
            uint8_t implementation, Anchor anchor, 
            const char* string, const uint8_t* typeface,
            const uint16_t color,
            const int16_t offset_x, const int16_t offset_y
    );

    void object(
            const uint16_t color
    ),   object(
            Anchor anchor, 
            const uint16_t size_x, const uint16_t size_y, 
            const uint16_t color, const uint8_t roundness
    ),   object(
            Anchor anchor, 
            const uint16_t size_x,  const uint16_t size_y, 
            const uint16_t color,   const uint8_t roundness,
            const int8_t offset_x,  const int8_t offset_y
    );

    void button(
            Anchor anchor,
            const uint16_t size_x,  const uint16_t size_y,
            const int16_t offset_x, const int16_t offset_y, 
            const std::function<void(void*)>& func, void* param = nullptr,
             bool is_temp = false
    ),   button(
            Anchor anchor,
            const uint16_t size_x,  const uint16_t size_y,
            const uint16_t color,   const uint8_t roundness,
            const int16_t offset_x, const int16_t offset_y, 
            const std::function<void(void*)>& func, void* param = nullptr,
            const bool is_temp = false
    ),   button(
            Anchor anchor,          const bool is_bar,
            const uint16_t size_x,  const uint16_t size_y,
            const uint16_t color,   const uint8_t roundness,
            const int16_t offset_x, const int16_t offset_y, 
            const std::function<void(void*)>& func, void* param = nullptr,
            const bool is_temp = false
    );

    void bar(
            Anchor anchor,
            uint16_t size_x, uint16_t size_y,
            
            uint16_t color_bg, uint16_t color_fill,
            int8_t roundness,
            
            int16_t offset_x, int16_t offset_y,
            uint8_t value,
            
            std::function<void(void*)>  func,
            void*                       param = nullptr
    );

    void clear();

    void buttons_clear();
    void buttons_clear_temp();
    void check_buttons(){_buttons_check();};

    uint32_t get_free_resources();
    uint32_t get_free_queue();

    TouchData   get_touchdata();
    uint8_t     get_brightness();

    // void set_sdfs(SdFs& sd); // TODO: Cover Image loading from SD Card
    void set_brightness(uint8_t percent);

    uint16_t rgb888_to_rgb565(const uint32_t color);
};
