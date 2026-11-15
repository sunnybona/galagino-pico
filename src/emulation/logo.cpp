#include "logo.h"
#include <cstring>

void Logo::decodeInto(uint8_t *slot) const {
    // Flash 直读：logo 的存储布局就是 PACKED_BYTES 的原始字节（16 色板 +
    // 半字节像素），之前在固件里是 LZMA 压缩的，现在不压缩了，直接拷一份。
    memcpy(slot, data, PACKED_BYTES);
}
