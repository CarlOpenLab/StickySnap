#include <QApplication>
#include <QCoreApplication>
#include <QIcon>
#include <QTimer>

#include "platform/platform.h"
#include "ui/app_controller.h"

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  QApplication::setApplicationName(QStringLiteral("StickySnap"));
  QApplication::setApplicationDisplayName(QStringLiteral("StickySnap"));
  QApplication::setOrganizationName(QStringLiteral("CarlOpenLab"));
  QApplication::setOrganizationDomain(QStringLiteral("carlopenlab.github.io"));
  QApplication::setQuitOnLastWindowClosed(false);
  QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/app-icon.png")));

  sticky::platform::configureApplication();

  sticky::ui::AppController controller;
  if (!controller.start()) return 0;

  // Developer hooks used by the local smoke test.
  const QStringList arguments = QCoreApplication::arguments();
  if (arguments.contains(QStringLiteral("--capture"))) {
    QTimer::singleShot(300, &controller,
                       &sticky::ui::AppController::startCapture);
  } else if (arguments.contains(QStringLiteral("--pin-clipboard"))) {
    QTimer::singleShot(300, &controller,
                       &sticky::ui::AppController::pinClipboard);
  }

  return app.exec();
}
