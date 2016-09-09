#include <QtWidgets>
#include "InputMgr.h"
#include "dialog.h"

//! [main() function]
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setAttribute(Qt::AA_ImmediateWidgetCreation);

    MultiModeInfo info;
    memset(&info, 0, sizeof(info));
    info.monitor_pos[0] = 1;            // 1 single display:  useless
    info.mode = MODE_CLONE;       // 1 single display
    int ret = SetMultiMode(&info);
    qWarning(">>>>>SetMultiMode:  clone(%d)", ret);

    QThread::sleep(2);
    Dialog dialog(true, false, 0, NULL);
    dialog.setWindowFlags(Qt::FramelessWindowHint);
    dialog.setGeometry(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    dialog.setAutoFillBackground(true);
    dialog.show();
    return app.exec();
//    return dialog.exec();
}
//! [main() function]
