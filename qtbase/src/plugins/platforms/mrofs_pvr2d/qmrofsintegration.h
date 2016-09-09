#ifndef QMrOfsIntegration_H
#define QMrOfsIntegration_H

#include <qpa/qplatformintegration.h>
#include <qpa/qplatformnativeinterface.h>

QT_BEGIN_NAMESPACE

class QMrOfsIntegrationPrivate;
class QAbstractEventDispatcher;
class QMrOfsScreen;

class QMrOfsIntegration : public QPlatformIntegration, public QPlatformNativeInterface
{
public:
    QMrOfsIntegration(const QStringList &paramList);
    ~QMrOfsIntegration();

    void initialize() Q_DECL_OVERRIDE;
    bool hasCapability(QPlatformIntegration::Capability cap) const Q_DECL_OVERRIDE;

    QPlatformPixmap *createPlatformPixmap(QPlatformPixmap::PixelType type) const Q_DECL_OVERRIDE;
    QPlatformWindow *createPlatformWindow(QWindow *window) const Q_DECL_OVERRIDE;
    QPlatformBackingStore *createPlatformBackingStore(QWindow *window) const Q_DECL_OVERRIDE;
    QAbstractEventDispatcher *createEventDispatcher() const Q_DECL_OVERRIDE;
    QPlatformFontDatabase *fontDatabase() const Q_DECL_OVERRIDE;

    QList<QPlatformScreen *> screens() const;

private:
    QMrOfsScreen *m_primaryScreen;
    QPlatformFontDatabase *m_fontDb;

};

QT_END_NAMESPACE

#endif // QMrOfsIntegration_H

