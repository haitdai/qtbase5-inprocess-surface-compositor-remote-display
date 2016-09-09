
#include <qpa/qplatformintegrationplugin.h>
#include "qmrofsintegration.h"

QT_BEGIN_NAMESPACE

class QMrOfsIntegrationPlugin : public QPlatformIntegrationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPA.QPlatformIntegrationFactoryInterface.5.2" FILE "mrofs_legacy_glesv2.json")
public:
    QPlatformIntegration *create(const QString&, const QStringList&);
};

QPlatformIntegration* QMrOfsIntegrationPlugin::create(const QString& system, const QStringList& paramList)
{
    Q_UNUSED(paramList);
    if (!system.compare(QLatin1String("mrofs_legacy_glesv2"), Qt::CaseInsensitive))
        return new QMrOfsIntegration(paramList);

    return 0;
}

QT_END_NAMESPACE

#include "main.moc"

