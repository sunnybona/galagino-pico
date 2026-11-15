#ifndef LOGO_H
#define LOGO_H

#include <cstdint>
#include "asset.h"

// Menu logo, 224x96 pixels with a 16 color palette, stored uncompressed in
// flash and read in place (XIP). Layout (romconv/romconv/gfx/logo.py):
//   16 x uint16_t  palette, byte swapped RGB565 as the display expects
//   10752 bytes    pixels, 2 per byte, high nibble first
// decodeInto() copies it into a menu cache slot that keeps this layout.
class Logo {
public:
    static constexpr uint32_t WIDTH = 224;
    static constexpr uint32_t HEIGHT = 96;
    static constexpr uint32_t PIXELS = WIDTH * HEIGHT;
    static constexpr uint32_t COLORS = 16;
    static constexpr uint32_t PACKED_BYTES = COLORS * sizeof(uint16_t) + PIXELS / 2;
    static constexpr uint32_t SLOT_BYTES = PACKED_BYTES;

    Logo(const FlashAsset<uint8_t, PLAIN> &blob) : data(blob.data) { }

    Logo(const Logo &) = delete;
    Logo &operator=(const Logo &) = delete;

    // slot: SLOT_BYTES, 2 byte aligned (palette is read as uint16_t)
    void decodeInto(uint8_t *slot) const;

    // 未压缩存在 flash 里，直接拿指针用（XIP），不再需要菜单缓存槽。
    const uint8_t *raw() const { return data; }

    static const uint16_t *palette(const uint8_t *slot) {
        return (const uint16_t *)slot;
    }

    static const uint8_t *pixels(const uint8_t *slot) {
        return slot + COLORS * sizeof(uint16_t);
    }

    // palette index of pixel i, e.g. byte 0xab: pixel 0 = 0xa, pixel 1 = 0xb
    static uint8_t colorIndex(const uint8_t *pixels, uint32_t i) {
        const uint8_t pair = pixels[i >> 1];
        return (i & 1) ? (pair & 0x0f) : (pair >> 4);
    }

private:
    const uint8_t *data;
};

#endif
