#include <QApplication>
#include <QCursor>
#include <iostream>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    for (int i = 0; i < 5; ++i) {
        QPoint p = QCursor::pos();
        std::cout << "QCursor::pos() = (" << p.x() << ", " << p.y() << ")\n";
    }
    return 0;
}
