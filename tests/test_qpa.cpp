#include <QApplication>
#include <QCursor>
#include <iostream>

int main(int argc, char* argv[]) {
    setenv("QT_QPA_PLATFORM", "xcb", 1);
    QApplication app(argc, argv);
    std::cout << "Platform name: " << app.platformName().toStdString() << "\n";
    QPoint p = QCursor::pos();
    std::cout << "xcb QCursor::pos() = (" << p.x() << ", " << p.y() << ")\n";
    return 0;
}
