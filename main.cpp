#include "./include/DGLABClinet.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    DGLABClinet window;
    window.show();

    return app.exec();
}
