#include <QApplication>

#include "mrdpviewer.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
	app.setAttribute(Qt::AA_ImmediateWidgetCreation);
	QThread::sleep(2);

    MRDPViewer viewer;
    viewer.show();
    return app.exec();
}
