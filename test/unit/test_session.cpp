// session::Save / Load 往返 + 坏行容错
// 注：session 的路径固定为 %APPDATA%\StickySnap\index.txt，
//     因此每个用例都用 FileGuard 备份/还原该文件，避免污染开发机。
#include <string>
#include <vector>

#include "test_util.h"

#include "session.h"

namespace {

// 宽字符（中文/空格）路径必须原样往返
const wchar_t kCnImg[] = L"C:\\Users\\测试 用户\\AppData\\Roaming\\StickySnap\\pins\\pin_1_1.png";

}  // namespace

TEST_CASE(session_roundtrip_two_pins) {
  ::test::FileGuard guard(::session::IndexPath());

  std::vector<PinSession> pins(2);
  pins[0].img = kCnImg;
  pins[0].x = -1200;
  pins[0].y = 480;
  pins[0].zoom = 1.245;
  pins[0].alpha = 128;
  pins[0].pass = true;

  pins[1].img = L"C:\\pins\\pin_2_2.png";
  pins[1].x = 12;
  pins[1].y = 34;
  pins[1].zoom = 0.500;
  pins[1].alpha = 255;
  pins[1].pass = false;

  ::session::Save(pins);

  std::vector<PinSession> loaded;
  EXPECT_TRUE(::session::Load(&loaded));
  EXPECT_EQ(loaded.size(), static_cast<size_t>(2));
  if (loaded.size() == 2) {
    EXPECT_WSTR_EQ(loaded[0].img, kCnImg);
    EXPECT_EQ(loaded[0].x, -1200);
    EXPECT_EQ(loaded[0].y, 480);
    EXPECT_EQ(loaded[0].zoom, 1.245);
    EXPECT_EQ(loaded[0].alpha, 128);
    EXPECT_TRUE(loaded[0].pass);

    EXPECT_WSTR_EQ(loaded[1].img, L"C:\\pins\\pin_2_2.png");
    EXPECT_EQ(loaded[1].x, 12);
    EXPECT_EQ(loaded[1].y, 34);
    EXPECT_EQ(loaded[1].zoom, 0.500);
    EXPECT_EQ(loaded[1].alpha, 255);
    EXPECT_TRUE(!loaded[1].pass);
  }
}

TEST_CASE(session_empty_roundtrip) {
  ::test::FileGuard guard(::session::IndexPath());

  std::vector<PinSession> none;
  ::session::Save(none);

  std::vector<PinSession> loaded;
  EXPECT_TRUE(::session::Load(&loaded));
  EXPECT_EQ(loaded.size(), static_cast<size_t>(0));
}

TEST_CASE(session_load_skips_bad_lines) {
  ::test::FileGuard guard(::session::IndexPath());

  // 2 条合法行 + 3 条坏行（无 tab 的残行 / 字段不足的截断行 / 全空字段行）
  const std::wstring text =
      L"C:\\pins\\a.png\t10\t-20\t2.500\t200\t1\n"
      L"this line has no tab at all\n"
      L"C:\\pins\\b.png\t1\t2\n"
      L"\t\t\t\t\t\n"
      L"C:\\pins\\c.png\t7\t8\t9.000\t300\t0\n";
  EXPECT_TRUE(::test::WriteTextFile(::session::IndexPath(), text));

  std::vector<PinSession> loaded;
  EXPECT_TRUE(::session::Load(&loaded));
  EXPECT_EQ(loaded.size(), static_cast<size_t>(2));
  if (loaded.size() == 2) {
    EXPECT_WSTR_EQ(loaded[0].img, L"C:\\pins\\a.png");
    EXPECT_EQ(loaded[0].x, 10);
    EXPECT_EQ(loaded[0].y, -20);
    EXPECT_EQ(loaded[0].zoom, 2.500);
    EXPECT_EQ(loaded[0].alpha, 200);
    EXPECT_TRUE(loaded[0].pass);

    EXPECT_WSTR_EQ(loaded[1].img, L"C:\\pins\\c.png");
    // 越界值被夹到合法范围：zoom 9.0 → kZoomMax，alpha 300 → 255
    EXPECT_EQ(loaded[1].zoom, config::kZoomMax);
    EXPECT_EQ(loaded[1].alpha, 255);
    EXPECT_TRUE(!loaded[1].pass);
  }
}
