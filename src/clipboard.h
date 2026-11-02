#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace clipboard {

// 写入剪贴板（CF_DIB + PNG 双格式），失败返回 false
bool CopyImage(HBITMAP hbmp);

// 读取剪贴板图片：优先 PNG 格式，其次 CF_DIB；没有图片返回 nullptr
HBITMAP GetImage();

}  // namespace clipboard
