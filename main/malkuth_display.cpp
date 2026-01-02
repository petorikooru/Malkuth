#include "malkuth_display.h"
#include "malkuth_helper.h"

///
/// Private Function
///

extern TJpg_Decoder TJpgDec;
MalkuthDisplay* MalkuthDisplay::_jpg_self = nullptr;
JpgState MalkuthDisplay::_jpg_state;

void MalkuthDisplay::task_display(void* parameters) {
  MalkuthDisplay* self = static_cast<MalkuthDisplay*>(parameters);
  DisplayCommand cmd;

  self->_tft.init();
  self->_tft.setRotation(0);
  self->_tft.setAttribute(UTF8_SWITCH, true);
  self->_tft.setAttribute(PSRAM_ENABLE, true);
  self->set_brightness(self->_brightness);
  self->_ts_exist = self->_ts.begin(40, 16, 15);

  while (true) {
    xQueueReceive(self->_queue_display, &cmd, portMAX_DELAY);
    self->handle_command(self, cmd);
  }
}

void MalkuthDisplay::handle_command(MalkuthDisplay* self, const DisplayCommand& cmd) {
  switch (cmd.type) {
    case DisplayType::IMAGE: 
        draw_image(self, cmd); 
        break;
    case DisplayType::TEXT: 
        draw_text(self, cmd); 
        break;
    case DisplayType::OBJECT: 
        draw_object(self, cmd); 
        break;
    case DisplayType::BAR:
        draw_bar(self, cmd);
        break;
    case DisplayType::CLEAR: 
        self->_tft.fillScreen(self->_bg_color); 
        break;
  }
}

void MalkuthDisplay::draw_image(MalkuthDisplay* self, const DisplayCommand& cmd) {
    switch (cmd.payload.image.type) {
        case ImageType::FLASH:
            draw_png_flash(self, cmd);
            break;

        case ImageType::PNG:
            TODO("Render PNG from sd card");
            break;
            
        case ImageType::JPG:
            draw_jpg(self, cmd);
            break;
    }
}

void MalkuthDisplay::draw_png_flash(MalkuthDisplay *self, const DisplayCommand&cmd) {
    const auto& img = cmd.payload.image;
    
    if (self->_png.openFLASH((uint8_t*)img.data, img.data_size, render_png) == PNG_SUCCESS) {
        self->_tft.startWrite();
        self->_png.decode(self, 0);
        self->_png.close();
        self->_tft.endWrite();
    }
}

void MalkuthDisplay::draw_jpg(MalkuthDisplay *self, const DisplayCommand&cmd){
    const auto& img = cmd.payload.image;

    _jpg_self = self;
    FsFile _file = self->_sd->open(img.path, O_READ);

    self->_jpg_state.dst_x = img.offset_x;
    self->_jpg_state.dst_y = img.offset_y;
    self->_jpg_state.dst_w = img.size_x;
    self->_jpg_state.dst_h = img.size_y;

    self->_jpg.setCallback(render_jpg);

    // Pick closest scale
    uint8_t scale = 1;
    uint16_t w, h;
;
    self->_jpg.getJpgSize(&w, &h, _file);

    Serial.printf("width = %d, height = %d\n", w, h);

    if (w > img.size_x * 4) scale = 4;
    else if (w > img.size_x * 2) scale = 2;

    self->_jpg.setJpgScale(scale);

    self->_tft.startWrite();
    self->_jpg.drawJpg(0, 0, _file);
    self->_tft.endWrite();
}

void MalkuthDisplay::draw_text(MalkuthDisplay* self, const DisplayCommand& cmd) {
  const auto& txt = cmd.payload.text;
  if (txt.string == nullptr || txt.string[0] == '\0') {
    return;
  }

  TFT_eSPI& tft = self->_tft;
  TFT_eSprite spr(&tft);

  spr.loadFont(txt.typeface);

  int16_t tw = spr.textWidth(txt.string);
  int16_t th = spr.fontHeight();

  const uint8_t padding = 4;
  uint16_t sprite_w = tw + 2 * padding;
  uint16_t sprite_h = th + 2 * padding;

  if (sprite_w > tft.width())
    sprite_w = tft.width();
  if (sprite_h > tft.height())
    sprite_h = tft.height();

  spr.createSprite(sprite_w, sprite_h);
  spr.setTextColor(txt.color, self->_bg_color);

  spr.fillSprite(TFT_TRANSPARENT);
  spr.setTextDatum(MC_DATUM);
  spr.setTextWrap(false, true);
  
  spr.drawString(txt.string, sprite_w / 2, sprite_h / 2);

  int16_t dest_x = txt.offset_x;
  int16_t dest_y = txt.offset_y;

  int16_t offset_x = 0;
  int16_t offset_y = 0;

  switch (txt.anchor) {
    case Anchor::TOP_LEFT:
      break;
    case Anchor::TOP_CENTER:
      offset_x = (self->_tft.width() / 2) - (sprite_w / 2);
      break;
    case Anchor::TOP_RIGHT:
      offset_x = self->_tft.width() - (sprite_w);
      break;
    case Anchor::MIDDLE_LEFT:
      offset_y = (self->_tft.height() / 2) - (sprite_h / 2);
      break;
    case Anchor::MIDDLE_CENTER:
      offset_x = (self->_tft.width() / 2) - (sprite_w / 2);
      offset_y = (self->_tft.height() / 2) - (sprite_h / 2);
      break;
    case Anchor::MIDDLE_RIGHT:
      offset_x = self->_tft.width() - (sprite_w);
      offset_y = (self->_tft.height() / 2) - (sprite_h / 2);
      break;
    case Anchor::BOTTOM_LEFT:
      offset_y = self->_tft.height() - (sprite_h);
      break;
    case Anchor::BOTTOM_CENTER:
      offset_x = (self->_tft.width() / 2) - (sprite_w / 2);
      offset_y = self->_tft.height() - (sprite_h);
      break;
    case Anchor::BOTTOM_RIGHT:
      offset_x = self->_tft.width() - (sprite_w);
      offset_y = self->_tft.height() - (sprite_h);
      break;
  }

  dest_x += offset_x;
  dest_y += offset_y;

  if (!txt.transparent)
    spr.pushSprite(dest_x, dest_y, self->_bg_color);
  else
    spr.pushSprite(dest_x, dest_y, TFT_TRANSPARENT);

  spr.unloadFont();
  spr.deleteSprite();
}

void MalkuthDisplay::draw_object(MalkuthDisplay* self, const DisplayCommand& cmd) {
  const auto& obj = cmd.payload.object;

  if (obj.size_x == 0 || obj.size_y == 0) {
    return;
  }

  TFT_eSPI& tft = self->_tft;
  uint16_t sprite_w = obj.size_x;
  uint16_t sprite_h = obj.size_y;

  if (sprite_w > tft.width()) sprite_w = tft.width();
  if (sprite_h > tft.height()) sprite_h = tft.height();

  uint8_t radius = obj.roundness;
  if (obj.roundness < 0) {
    radius = std::min(obj.size_x, sprite_h) / 2;
  }

  int16_t dest_x = obj.offset_x;
  int16_t dest_y = obj.offset_y;

  int16_t offset_x = 0;
  int16_t offset_y = 0;

  switch (cmd.payload.object.anchor) {
    case Anchor::TOP_LEFT:
      break;
    case Anchor::TOP_CENTER:
      offset_x = (self->_tft.width() / 2) - (sprite_w / 2);
      break;
    case Anchor::TOP_RIGHT:
      offset_x = self->_tft.width() - (sprite_w);
      break;
    case Anchor::MIDDLE_LEFT:
      offset_y = (self->_tft.height() / 2) - (sprite_h / 2);
      break;
    case Anchor::MIDDLE_CENTER:
      offset_x = (self->_tft.width() / 2) - (sprite_w / 2);
      offset_y = (self->_tft.height() / 2) - (sprite_h / 2);
      break;
    case Anchor::MIDDLE_RIGHT:
      offset_x = self->_tft.width() - (sprite_w);
      offset_y = (self->_tft.height() / 2) - (sprite_h / 2);
      break;
    case Anchor::BOTTOM_LEFT:
      offset_y = self->_tft.height() - (sprite_h);
      break;
    case Anchor::BOTTOM_CENTER:
      offset_x = (self->_tft.width() / 2) - (sprite_w / 2);
      offset_y = self->_tft.height() - (sprite_h);
      break;
    case Anchor::BOTTOM_RIGHT:
      offset_x = self->_tft.width() - (sprite_w);
      offset_y = self->_tft.height() - (sprite_h);
      break;
  }

  dest_x += offset_x;
  dest_y += offset_y;

  tft.fillRoundRect(dest_x, dest_y, sprite_w, sprite_h, radius, obj.color);
}

void MalkuthDisplay::draw_bar(MalkuthDisplay* self, const DisplayCommand& cmd) {
    const auto& bar = cmd.payload.bar;

    uint16_t fill_w = map(bar.value, 0, 100, 0, bar.size_x);\

    self->_tft.fillRoundRect(bar.offset_x, bar.offset_y, bar.size_x, bar.size_y, bar.roundness, bar.color_bg);
    if (bar.value > 0) {
        self->_tft.fillRoundRect(bar.offset_x, bar.offset_y, fill_w, bar.size_y, bar.roundness, bar.color_fill);
    }
}

int MalkuthDisplay::render_png(PNGDRAW* png_draw) {
  MalkuthDisplay* self = static_cast<MalkuthDisplay*>(png_draw->pUser);
  uint16_t line_buffer[MAX_IMAGE_WIDTH];

  self->_png.getLineAsRGB565(
    png_draw,
    line_buffer,
    PNG_RGB565_BIG_ENDIAN,
    0xffffffff);

  self->_tft.pushImage(
    0,
    png_draw->y,
    png_draw->iWidth,
    1,
    line_buffer);

  return 1;
}

bool MalkuthDisplay::render_jpg(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    if (!_jpg_self) return false;

    auto& s = _jpg_self->_jpg_state;

    // Offset shift
    int16_t draw_x = s.dst_x + x;
    int16_t draw_y = s.dst_y + y;

    // Clipping
    int16_t clip_x2 = s.dst_x + s.dst_w;
    int16_t clip_y2 = s.dst_y + s.dst_h;

    // If completely outside
    if (draw_x >= clip_x2 || draw_y >= clip_y2) return true;
    if (draw_x + w <= s.dst_x || draw_y + h <= s.dst_y) return true;

    // Partial clipping
    int16_t skip_x = 0;
    int16_t skip_y = 0;

    if (draw_x < s.dst_x) {
        skip_x = s.dst_x - draw_x;
        draw_x = s.dst_x;
        w -= skip_x;
    }

    if (draw_y < s.dst_y) {
        skip_y = s.dst_y - draw_y;
        draw_y = s.dst_y;
        h -= skip_y;
    }

    if (draw_x + w > clip_x2) w = clip_x2 - draw_x;
    if (draw_y + h > clip_y2) h = clip_y2 - draw_y;

    if (w == 0 || h == 0) return true;

    _jpg_self->_tft.pushImage(
        draw_x,
        draw_y,
        w,
        h,
        bitmap + skip_y * w + skip_x
    );

    return 1;
}

void MalkuthDisplay::set_brightness(uint8_t percent) {
  if (percent > 100)
    percent = 100;

  _brightness = percent;
  analogWrite(PIN_BL, static_cast<uint16_t>(percent) * 255 / 100);
}

///
/// Public Function
///
void MalkuthDisplay::init() {
    _queue_display = xQueueCreate(32, sizeof(DisplayCommand));

    xTaskCreate(
        task_display,
        "Malkuth_Display",
        32000,
        this,
        0,
        &_taskhandle_display
    );
}

void MalkuthDisplay::init(const uint8_t core) {
    _queue_display = xQueueCreate(64, sizeof(DisplayCommand));

    xTaskCreatePinnedToCore(
        task_display,
        "Malkuth_Display",
        32000,
        this,
        1,
        &_taskhandle_display,
        core
    );
}

void MalkuthDisplay::buttons_clear() {
  _buttons.clear();
}

void MalkuthDisplay::button(
    Anchor anchor,
    const uint16_t size_x, const uint16_t size_y,
    const uint16_t color, const uint8_t roundness,
    const int16_t offset_x, const int16_t offset_y, 
    const std::function<void(void*)>& func, void* param
){
    uint16_t width = size_x;
    uint16_t height = size_y;
    int16_t x = offset_x;
    int16_t y = offset_y;

    if (width > _tft.width()) width = _tft.width();
    if (height > _tft.height()) height = _tft.height();

    switch (anchor) {
        case Anchor::TOP_LEFT:
            break;
        case Anchor::TOP_CENTER:
            x += (_tft.width() / 2) - (width / 2);
            break;
        case Anchor::TOP_RIGHT:
            x += _tft.width() - (width);
            break;
        case Anchor::MIDDLE_LEFT:
            y += (_tft.height() / 2) - (height / 2);
            break;
        case Anchor::MIDDLE_CENTER:
            x += (_tft.width() / 2) - (width / 2);
            y += (_tft.height() / 2) - (height / 2);
            break;
        case Anchor::MIDDLE_RIGHT:
            x += _tft.width() - (width);
            y += (_tft.height() / 2) - (height / 2);
            break;
        case Anchor::BOTTOM_LEFT:
            y += _tft.height() - (height);
            break;
        case Anchor::BOTTOM_CENTER:
            x += (_tft.width() / 2) - (width / 2);
            y += _tft.height() - (height);
            break;
        case Anchor::BOTTOM_RIGHT:
            x += _tft.width() - (width);
            y += _tft.height() - (height);
            break;
    }

    object(anchor, size_x, size_y, color, roundness, offset_x, offset_y);

    _buttons.push_back({ 
        x, y,
        width, height,
        func, param 
    });
}

void MalkuthDisplay::button(
    Anchor anchor,
    const uint16_t size_x, const uint16_t size_y,
    const int16_t offset_x, const int16_t offset_y, 
    const std::function<void(void*)>& func, void* param
){
    uint16_t width = size_x;
    uint16_t height = size_y;
    int16_t x = offset_x;
    int16_t y = offset_y;
    if (width > _tft.width()) width = _tft.width();
    if (height > _tft.height()) height = _tft.height();

    switch (anchor) {
        case Anchor::TOP_LEFT:
            break;
        case Anchor::TOP_CENTER:
            x += (_tft.width() / 2) - (width / 2);
            break;
        case Anchor::TOP_RIGHT:
            x += _tft.width() - (width);
            break;
        case Anchor::MIDDLE_LEFT:
            y += (_tft.height() / 2) - (height / 2);
            break;
        case Anchor::MIDDLE_CENTER:
            x += (_tft.width() / 2) - (width / 2);
            y += (_tft.height() / 2) - (height / 2);
            break;
        case Anchor::MIDDLE_RIGHT:
            x += _tft.width() - (width);
            y += (_tft.height() / 2) - (height / 2);
            break;
        case Anchor::BOTTOM_LEFT:
            y += _tft.height() - (width);
            break;
        case Anchor::BOTTOM_CENTER:
            x += (_tft.width() / 2) - (width / 2);
            y += _tft.height() - (height);
            break;
        case Anchor::BOTTOM_RIGHT:
            x += _tft.width() - (width);
            y += _tft.height() - (height);
            break;
    }

    _buttons.push_back({ 
        x, y,
        width, height,
        func, param 
    });
}

void MalkuthDisplay::_buttons_check() {
  if (!_ts_exist) return;

  TS_Point p = _ts.getPoint();
  if (p.z == 0) return;

  uint16_t tx = p.x;
  uint16_t ty = p.y;

  for (const auto& btn : _buttons) {
    int16_t btn_x = btn.offset_x;
    int16_t btn_y = btn.offset_y;
    uint16_t btn_w = btn.size_x;
    uint16_t btn_h = btn.size_y;

    if (tx >= btn_x && 
        tx < btn_x + btn_w && 
        ty >= btn_y && 
        ty < btn_y + btn_h) {
      if (btn.func) {
        btn.func(btn.param);
        vTaskDelay(300 / portTICK_PERIOD_MS);
        return;
      }
    }
  }

  vTaskDelay(100 / portTICK_PERIOD_MS);
}

void MalkuthDisplay::text(
  Anchor anchor, const bool transparent,
  const char* string, const uint8_t* typeface,
  const uint16_t color
){

  DisplayCommand cmd{
    .type = DisplayType::TEXT,
    .payload = { .text = {
        .offset_x = 0,
        .offset_y = 0,
        .typeface = typeface,
        .color = color,
        .transparent = transparent,
        .anchor = anchor,
    }}
  };    
  
  strncpy(cmd.payload.text.string, string, MAX_TEXT_LENGTH - 1);
  cmd.payload.text.string[MAX_TEXT_LENGTH -  1] = '\0';

  xQueueSend(_queue_display, &cmd, 50);
}

void MalkuthDisplay::text(
    Anchor anchor, const bool transparent,
    const char* string, const uint8_t* typeface,
    const uint16_t color,
    const int16_t offset_x, const int16_t offset_y
){

    DisplayCommand cmd{
        .type = DisplayType::TEXT,
        .payload = { .text = {
            .offset_x = offset_x,
            .offset_y = offset_y,
            .typeface = typeface,
            .color = color,
            .transparent = transparent,
            .anchor = anchor,
        }}
    };

    strncpy(cmd.payload.text.string, string, MAX_TEXT_LENGTH - 1);
    cmd.payload.text.string[MAX_TEXT_LENGTH -  1] = '\0';

    xQueueSend(_queue_display, &cmd, 50);
}

void MalkuthDisplay::image(
    ImageType type,
    const uint8_t* image,
    size_t data_size
){
    DisplayCommand cmd = {
      .type = DisplayType::IMAGE,
      .payload = { .image = {
                     .type = type,
                     .data_size = data_size,
                     .data = image,
                     .path = NULL,
                     .size_x = 0,
                     .size_y = 0,
                     .offset_x = 0,
                     .offset_y = 0,
                   } }
    };

    xQueueSend(_queue_display, &cmd, 50);
}

void MalkuthDisplay::image(
    ImageType   type,
    const char* path,

    const uint16_t size_x, const uint16_t size_y,
    const int16_t offset_x, const int16_t offset_y
){
    DisplayCommand cmd = {
      .type = DisplayType::IMAGE,
      .payload = { .image = {
            .type = type,
            .data_size = 0,
            .data = NULL,
            .size_x = size_x,
            .size_y = size_y,
            .offset_x = offset_x,
            .offset_y = offset_y,
        }}
    };

    strncpy(cmd.payload.image.path, path, MAX_TEXT_LENGTH - 1);
    cmd.payload.image.path[MAX_TEXT_LENGTH -  1] = '\0';

    xQueueSend(_queue_display, &cmd, 50);
}

void MalkuthDisplay::object(
    Anchor anchor,
    const uint16_t size_x, const uint16_t size_y,
    const uint16_t color, const uint8_t roundness
){
    DisplayCommand cmd = {
      .type = DisplayType::OBJECT,
      .payload = { .object = {
                     .offset_x = 0,
                     .offset_y = 0,
                     .size_x = size_x,
                     .size_y = size_y,
                     .color = color,
                     .roundness = roundness,
                     .anchor = anchor,
                   } }
    };

    xQueueSend(_queue_display, &cmd, 50);
}

void MalkuthDisplay::object(
    Anchor anchor,
    const uint16_t size_x, const uint16_t size_y,
    const uint16_t color, const uint8_t roundness,
    const int8_t offset_x, const int8_t offset_y) {
    DisplayCommand cmd = {
      .type = DisplayType::OBJECT,
      .payload = { .object = {
                     .offset_x = offset_x,
                     .offset_y = offset_y,
                     .size_x = size_x,
                     .size_y = size_y,
                     .color = color,
                     .roundness = roundness,
                     .anchor = anchor,
                   } }
    };

    xQueueSend(_queue_display, &cmd, 50);
}

void MalkuthDisplay::bar(
    Anchor anchor,
    uint16_t size_x, uint16_t size_y,
    uint16_t color_bg, uint16_t color_fill,

    int8_t roundness,

    int16_t offset_x, int16_t offset_y,
    uint8_t value,

    std::function<void(void*)>  func,
    void*                       param
){
    uint16_t width = size_x;
    uint16_t height = size_y;
    int16_t x = offset_x;
    int16_t y = offset_y;

    if (width > _tft.width()) width = _tft.width();
    if (height > _tft.height()) height = _tft.height();

    switch (anchor) {
        case Anchor::TOP_LEFT:
            break;
        case Anchor::TOP_CENTER:
            x += (_tft.width() / 2) - (width / 2);
            break;
        case Anchor::TOP_RIGHT:
            x += _tft.width() - (width);
            break;
        case Anchor::MIDDLE_LEFT:
            y += (_tft.height() / 2) - (height / 2);
            break;
        case Anchor::MIDDLE_CENTER:
            x += (_tft.width() / 2) - (width / 2);
            y += (_tft.height() / 2) - (height / 2);
            break;
        case Anchor::MIDDLE_RIGHT:
            x += _tft.width() - (width);
            y += (_tft.height() / 2) - (height / 2);
            break;
        case Anchor::BOTTOM_LEFT:
            y += _tft.height() - (width);
            break;
        case Anchor::BOTTOM_CENTER:
            x += (_tft.width() / 2) - (width / 2);
            y += _tft.height() - (height);
            break;
        case Anchor::BOTTOM_RIGHT:
            x += _tft.width() - (width);
            y += _tft.height() - (height);
            break;
    }

    DisplayCommand cmd = {
        .type = DisplayType::BAR,
        .payload = { .bar ={
            .offset_x = x,
            .offset_y = y,
            .size_x = size_x,
            .size_y = size_y,
            .color_bg = color_bg,
            .color_fill = color_fill,
            .roundness = roundness,
            .value = value
        }}
    };

    xQueueSend(_queue_display, &cmd, 50);

    _buttons.push_back({
        x, y,
        size_x, size_y,
        func, param
    });
}

void MalkuthDisplay::clear(){
    DisplayCommand clear_cmd{ .type = DisplayType::CLEAR };
    xQueueSend(_queue_display, &clear_cmd, 50);
}

uint32_t MalkuthDisplay::get_free_resources() {
  return uxTaskGetStackHighWaterMark(_taskhandle_display);
}

uint32_t MalkuthDisplay::get_free_queue() {
  return uxQueueSpacesAvailable(_queue_display);
}

uint8_t MalkuthDisplay::get_brightness() {
    return _brightness;
}

TouchData MalkuthDisplay::get_touchdata() {
  TS_Point p = _ts.getPoint();
  return TouchData{
    .x = p.x,
    .y = p.y,
    .z = p.z
  };
}

void MalkuthDisplay::set_fg(const uint16_t color){
    _fg_color = color;
}

void MalkuthDisplay::set_bg(const uint16_t color){
    _bg_color = color;
}

void MalkuthDisplay::set_sdfs(SdFs& sd){
    _sd = &sd;
}

uint16_t MalkuthDisplay::rgb888_to_rgb565(const uint32_t color) {
  return ((color >> 16) & 0xF8) << 8 |
         ((color >> 8)  & 0xFC) << 3 |
         (color         & 0x1F);
}
