#include "platform/platform.h"

#include <QBuffer>
#include <QGuiApplication>
#include <QImage>
#include <QMetaObject>

#include <windows.h>

#ifdef STICKYSNAP_HAVE_OCR
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Media.Ocr.h>
#include <winrt/Windows.Storage.Streams.h>

#include <thread>
#include <utility>

namespace sticky::platform {
namespace {

namespace wgi = winrt::Windows::Graphics::Imaging;
namespace wss = winrt::Windows::Storage::Streams;

// QImage → PNG 字节 → 内存流 → BitmapDecoder，全走 WinRT 投影 API。
// （直接写 SoftwareBitmap 像素需要 IMemoryBufferByteAccess 这类未投影的原始
// COM 接口，与投影头混编麻烦；PNG 编码开销对截图尺寸可忽略。）
wgi::SoftwareBitmap makeSoftwareBitmap(const QImage& image) {
  QByteArray png;
  QBuffer device(&png);
  device.open(QIODevice::WriteOnly);
  if (!image.save(&device, "PNG")) {
    throw winrt::hresult_error(E_FAIL, winrt::hstring(L"image encode failed"));
  }
  device.close();

  auto stream = wss::InMemoryRandomAccessStream();
  auto writer = wss::DataWriter(stream);
  writer.WriteBytes(winrt::array_view<const uint8_t>(
      reinterpret_cast<const uint8_t*>(png.constData()),
      reinterpret_cast<const uint8_t*>(png.constData()) + png.size()));
  writer.StoreAsync().get();
  writer.FlushAsync().get();
  writer.DetachStream();
  stream.Seek(0);

  auto decoder = wgi::BitmapDecoder::CreateAsync(stream).get();
  return decoder
      .GetSoftwareBitmapAsync(wgi::BitmapPixelFormat::Bgra8,
                              wgi::BitmapAlphaMode::Ignore)
      .get();
}

}  // namespace

void ocrText(const QImage& image, OcrCallback callback) {
  if (image.isNull()) {
    callback({}, QObject::tr("图片为空。"));
    return;
  }

  // OCR 引擎是异步 WinRT API：放到独立线程阻塞等待，结果再排队回 GUI 线程。
  std::thread([image, callback = std::move(callback)]() mutable {
    QString text;
    QString error;
    try {
      winrt::init_apartment(winrt::apartment_type::multi_threaded);

      auto engine =
          winrt::Windows::Media::Ocr::OcrEngine::TryCreateFromUserProfileLanguages();
      if (!engine) {
        error = QObject::tr(
            "没有可用的 OCR 语言包。请在系统设置中添加语言并勾选「可选功能」"
            "中的文字识别。");
      } else {
        const auto result = engine.RecognizeAsync(makeSoftwareBitmap(image)).get();
        text = QString::fromStdWString(std::wstring(result.Text()));
        text.remove(QChar(0x200B));  // OCR 引擎偶尔夹带零宽字符
      }
      winrt::uninit_apartment();
    } catch (const winrt::hresult_error& e) {
      error = QString::fromStdWString(std::wstring(e.message()));
    } catch (...) {
      error = QObject::tr("OCR 识别失败。");
    }

    QMetaObject::invokeMethod(
        QGuiApplication::instance(),
        [callback = std::move(callback), text, error]() mutable {
          callback(text, error);
        },
        Qt::QueuedConnection);
  }).detach();
}

}  // namespace sticky::platform

#else  // !STICKYSNAP_HAVE_OCR

namespace sticky::platform {

void ocrText(const QImage& image, OcrCallback callback) {
  Q_UNUSED(image);
  callback({}, QObject::tr("此构建未包含 OCR（需要带 C++/WinRT 投影头的 Windows SDK）。"));
}

}  // namespace sticky::platform

#endif  // STICKYSNAP_HAVE_OCR
