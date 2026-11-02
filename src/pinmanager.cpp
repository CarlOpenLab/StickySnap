#include "pinmanager.h"

#include "config.h"
#include "gdiutil.h"
#include "session.h"
#include "util.h"

namespace {
constexpr size_t kMaxPins = 200;
}  // namespace

PinWindow* PinManager::CreatePin(HBITMAP bmp, int x, int y) {
  if (pins_.size() >= kMaxPins) {
    util::Debug(L"pin limit reached");
    DeleteObject(bmp);
    return nullptr;
  }
  const std::wstring path = util::NewPinImagePath();
  gdiutil::SaveImage(bmp, path);  // 尽力落盘，会话恢复依赖它
  PinWindow* p = PinWindow::Create(inst_, this, bmp, path, x, y);
  if (p) pins_.emplace_back(p);
  return p;
}

void PinManager::ClosePin(PinWindow* pin) {
  for (auto it = pins_.begin(); it != pins_.end(); ++it) {
    if (it->get() == pin) {
      pin->Close();     // DestroyWindow（同步）
      pins_.erase(it);  // 释放对象
      break;
    }
  }
  MarkDirty();
}

void PinManager::CloseAll() {
  // 先取出裸指针，避免遍历中修改容器
  std::vector<PinWindow*> list;
  list.reserve(pins_.size());
  for (auto& p : pins_) list.push_back(p.get());
  for (auto* p : list) p->Close();
  pins_.clear();
  MarkDirty();
}

void PinManager::Restore() {
  std::vector<PinSession> list;
  if (!session::Load(&list) || list.empty()) return;

  RECT b{};
  b.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
  b.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
  b.right = b.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
  b.bottom = b.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);

  int restored = 0;
  for (auto& s : list) {
    if (restored >= kMaxPins) break;
    HBITMAP bmp = gdiutil::LoadImageFile(s.img);
    if (!bmp) continue;  // 图片文件丢失则跳过
    s.x = util::Clamp(s.x, b.left, b.right - 40);
    s.y = util::Clamp(s.y, b.top, b.bottom - 40);
    PinWindow* p = PinWindow::Create(inst_, this, bmp, s.img, s.x, s.y, &s);
    if (p) {
      pins_.emplace_back(p);
      ++restored;
    }
  }
  util::Debug(L"restored %d pins", restored);
}

void PinManager::MarkDirty() {
  dirty_ = true;
  if (owner_) SetTimer(owner_, config::kTimerSave, 800, nullptr);
}

void PinManager::FlushIfDirty() {
  if (owner_) KillTimer(owner_, config::kTimerSave);
  if (!dirty_) return;
  SaveNow();
}

void PinManager::SaveNow() {
  std::vector<PinSession> list;
  list.reserve(pins_.size());
  for (auto& p : pins_) list.push_back(p->Session());
  session::Save(list);
  dirty_ = false;
}
