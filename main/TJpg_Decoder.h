/*
 TJpg_Decoder.h

 JPEG Decoder for Arduino using TJpgDec:
 http://elm-chan.org/fsw/tjpgd/00index.html

 Adapted for SdFat
*/

#ifndef TJpg_Decoder_H
#define TJpg_Decoder_H

#include <Arduino.h>
#include <SdFat.h>
#include "tjpgd.h"

//------------------------------------------------------------
// JPG source types
//------------------------------------------------------------
enum {
  TJPG_ARRAY = 0,
  TJPG_SDFAT_FILE
};

//------------------------------------------------------------
// Callback type
//------------------------------------------------------------
typedef bool (*SketchCallback)(
  int16_t x, int16_t y,
  uint16_t w, uint16_t h,
  uint16_t *data
);

//------------------------------------------------------------
// Decoder class
//------------------------------------------------------------
class TJpg_Decoder {

public:
  TJpg_Decoder();
  ~TJpg_Decoder();

  static unsigned int jd_input(JDEC* jdec, uint8_t* buf, unsigned int len);
  static int jd_output(JDEC* jdec, void* bitmap, JRECT* jrect);

  void setJpgScale(uint8_t scale);
  void setCallback(SketchCallback cb);
  void setSwapBytes(bool swap);

  JRESULT drawJpg(int32_t x, int32_t y, const char *filename);
  JRESULT drawJpg(int32_t x, int32_t y, FsFile& file);

  JRESULT getJpgSize(uint16_t *w, uint16_t *h, const char *filename);
  JRESULT getJpgSize(uint16_t *w, uint16_t *h, FsFile& file);

  JRESULT drawJpg(int32_t x, int32_t y, const uint8_t *array, uint32_t size);
  JRESULT getJpgSize(uint16_t *w, uint16_t *h, const uint8_t *array, uint32_t size);

public:
  FsFile* jpgFile = nullptr;

  const uint8_t *array_data = nullptr;
  uint32_t array_index = 0;
  uint32_t array_size  = 0;

  uint8_t jpg_source = TJPG_ARRAY;

  int16_t jpeg_x = 0;
  int16_t jpeg_y = 0;

  uint8_t jpgScale = 0;
  bool _swap = false;

  SketchCallback tft_output = nullptr;

  uint8_t workspace[TJPGD_WORKSPACE_SIZE] __attribute__((aligned(4)));

  TJpg_Decoder *thisPtr = nullptr;
};

extern TJpg_Decoder TJpgDec;

extern SdFat sd;

#endif
