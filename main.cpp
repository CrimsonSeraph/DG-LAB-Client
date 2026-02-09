#include "DGLABClient.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    DGLABClient window;
    window.show();

    return app.exec();
}
