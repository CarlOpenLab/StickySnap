// CMake 探针：SDK 自带 C++/WinRT 投影头 + windowsapp.lib 可用才编译 OCR 实现。
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Ocr.h>

int main() {
  winrt::Windows::Media::Ocr::OcrEngine::AvailableRecognizerLanguages();
  return 0;
}
