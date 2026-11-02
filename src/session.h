#pragma once
// 贴图会话持久化（header-only）
// 文件：%APPDATA%\StickySnap\index.txt，每行一个贴图：
//   img\tx\ty\tzoom\talpha\tpass
// 图片本体存于 %APPDATA%\StickySnap\pins\*.png（贴图创建时即落盘）
// 注：img 路径由本程序生成（ASCII 目录 + 受控文件名），无制表符风险
#include <cstdio>
#include <string>
#include <vector>

#include "config.h"
#include "util.h"

struct PinSession {
  std::wstring img;
  int x = 0, y = 0;
  double zoom = 1.0;
  int alpha = 255;
  bool pass = false;
};

namespace session {

inline std::wstring IndexPath() { return util::AppDir() + L"\\index.txt"; }

inline void Save(const std::vector<PinSession>& pins) {
  const std::wstring tmp = IndexPath() + L".tmp";
  FILE* f = _wfopen(tmp.c_str(), L"w, ccs=UTF-8");
  if (!f) {
    util::Debug(L"session save failed");
    return;
  }
  for (const auto& p : pins) {
    fwprintf(f, L"%s\t%d\t%d\t%.3f\t%d\t%d\n", p.img.c_str(), p.x, p.y,
             p.zoom, p.alpha, p.pass ? 1 : 0);
  }
  fclose(f);
  MoveFileExW(tmp.c_str(), IndexPath().c_str(), MOVEFILE_REPLACE_EXISTING);
}

inline bool Load(std::vector<PinSession>* out) {
  out->clear();
  FILE* f = _wfopen(IndexPath().c_str(), L"r, ccs=UTF-8");
  if (!f) return false;
  wchar_t line[2048];
  while (fgetws(line, 2048, f)) {
    wchar_t img[1024];
    int x = 0, y = 0, alpha = 255, pass = 0;
    double zoom = 1.0;
    if (swscanf(line, L"%1023[^\t]\t%d\t%d\t%lf\t%d\t%d", img, &x, &y, &zoom,
                &alpha, &pass) == 6) {
      PinSession p;
      p.img = img;
      p.x = x;
      p.y = y;
      p.zoom = util::Clamp(zoom, config::kZoomMin, config::kZoomMax);
      p.alpha = util::Clamp(alpha, config::kAlphaMin, 255);
      p.pass = pass != 0;
      out->push_back(p);
    }
  }
  fclose(f);
  return true;
}

}  // namespace session
