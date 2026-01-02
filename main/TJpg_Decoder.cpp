/*
 Taken from Bodmer's TJpg_Decoder library
*/

#include "TJpg_Decoder.h"

// Global instance
TJpg_Decoder TJpgDec;

//------------------------------------------------------------
// Constructor / Destructor
//------------------------------------------------------------
TJpg_Decoder::TJpg_Decoder() {
  thisPtr = this;
}

TJpg_Decoder::~TJpg_Decoder() {}

//------------------------------------------------------------
// Settings
//------------------------------------------------------------
void TJpg_Decoder::setSwapBytes(bool swap) {
  _swap = swap;
}

void TJpg_Decoder::setJpgScale(uint8_t scale) {
  switch (scale) {
    case 1: jpgScale = 0; break;
    case 2: jpgScale = 1; break;
    case 4: jpgScale = 2; break;
    case 8: jpgScale = 3; break;
    default: jpgScale = 0;
  }
}

void TJpg_Decoder::setCallback(SketchCallback cb) {
  tft_output = cb;
}

//------------------------------------------------------------
// TJpgDec input callback
//------------------------------------------------------------
unsigned int TJpg_Decoder::jd_input(JDEC* jdec, uint8_t* buf, unsigned int len) {
  TJpg_Decoder *self = TJpgDec.thisPtr;
  (void)jdec;

  // FLASH array source
  if (self->jpg_source == TJPG_ARRAY) {
    if (self->array_index + len > self->array_size) {
      len = self->array_size - self->array_index;
    }
    if (buf) {
      memcpy_P(buf, self->array_data + self->array_index, len);
    }
    self->array_index += len;
  }

  // SdFat file source
  else if (self->jpg_source == TJPG_SDFAT_FILE) {
    uint32_t available = self->jpgFile->available();
    if (available < len) len = available;

    if (buf) {
      self->jpgFile->read(buf, len);
    } else {
      self->jpgFile->seekSet(self->jpgFile->curPosition() + len);
    }
  }

  return len;
}

//------------------------------------------------------------
// TJpgDec output callback
//------------------------------------------------------------
int TJpg_Decoder::jd_output(JDEC* jdec, void* bitmap, JRECT* jrect) {
  TJpg_Decoder *self = TJpgDec.thisPtr;
  (void)jdec;

  int16_t x = jrect->left + self->jpeg_x;
  int16_t y = jrect->top  + self->jpeg_y;
  uint16_t w = jrect->right  + 1 - jrect->left;
  uint16_t h = jrect->bottom + 1 - jrect->top;

  if (!self->tft_output) return 0;
  return self->tft_output(x, y, w, h, (uint16_t*)bitmap);
}

//------------------------------------------------------------
// Draw JPG from SdFat filename
//------------------------------------------------------------
JRESULT TJpg_Decoder::drawJpg(int32_t x, int32_t y, const char *filename) {
  if (!sd.exists(filename)) {
    Serial.println(F("Jpeg file not found"));
    return JDR_INP;
  }

  FsFile file = sd.open(filename, O_RDONLY);
  if (!file) return JDR_INP;

  return drawJpg(x, y, file);
}

//------------------------------------------------------------
// Draw JPG from SdFat file handle
//------------------------------------------------------------
JRESULT TJpg_Decoder::drawJpg(int32_t x, int32_t y, FsFile& file) {
  JDEC jdec;
  JRESULT res;

  jpg_source = TJPG_SDFAT_FILE;
  jpeg_x = x;
  jpeg_y = y;
  jpgFile = &file;

  jdec.swap = _swap;

  res = jd_prepare(&jdec, jd_input, workspace, TJPGD_WORKSPACE_SIZE, 0);
  if (res == JDR_OK) {
    res = jd_decomp(&jdec, jd_output, jpgScale);
  }

  jpgFile->close();
  return res;
}

//------------------------------------------------------------
// Get JPG size from SdFat filename
//------------------------------------------------------------
JRESULT TJpg_Decoder::getJpgSize(uint16_t *w, uint16_t *h, const char *filename) {
  *w = *h = 0;

  if (!sd.exists(filename)) return JDR_INP;

  FsFile file = sd.open(filename, O_RDONLY);
  if (!file) return JDR_INP;

  return getJpgSize(w, h, file);
}

//------------------------------------------------------------
// Get JPG size from SdFat file handle
//------------------------------------------------------------
JRESULT TJpg_Decoder::getJpgSize(uint16_t *w, uint16_t *h, FsFile& file) {
  JDEC jdec;
  JRESULT res;

  jpg_source = TJPG_SDFAT_FILE;
  jpgFile = &file;

  res = jd_prepare(&jdec, jd_input, workspace, TJPGD_WORKSPACE_SIZE, 0);
  if (res == JDR_OK) {
    *w = jdec.width;
    *h = jdec.height;
  }

  jpgFile->close();
  return res;
}

//------------------------------------------------------------
// Draw JPG from memory array
//------------------------------------------------------------
JRESULT TJpg_Decoder::drawJpg(int32_t x, int32_t y, const uint8_t *array, uint32_t size) {
  JDEC jdec;
  JRESULT res;

  jpg_source = TJPG_ARRAY;
  array_data = array;
  array_size = size;
  array_index = 0;
  jpeg_x = x;
  jpeg_y = y;

  jdec.swap = _swap;

  res = jd_prepare(&jdec, jd_input, workspace, TJPGD_WORKSPACE_SIZE, 0);
  if (res == JDR_OK) {
    res = jd_decomp(&jdec, jd_output, jpgScale);
  }

  return res;
}

//------------------------------------------------------------
// Get JPG size from memory array
//------------------------------------------------------------
JRESULT TJpg_Decoder::getJpgSize(uint16_t *w, uint16_t *h, const uint8_t *array, uint32_t size) {
  JDEC jdec;
  JRESULT res;

  *w = *h = 0;

  jpg_source = TJPG_ARRAY;
  array_data = array;
  array_size = size;
  array_index = 0;

  res = jd_prepare(&jdec, jd_input, workspace, TJPGD_WORKSPACE_SIZE, 0);
  if (res == JDR_OK) {
    *w = jdec.width;
    *h = jdec.height;
  }

  return res;
}
