#pragma once

#include <stdint.h>

#ifndef TODO
#define TODO(text) Serial.printf("[TODO] : %s\n", text)
#endif

enum class ImageType : uint8_t {
    NONE,
    FLASH,
    PNG,
    JPG
};