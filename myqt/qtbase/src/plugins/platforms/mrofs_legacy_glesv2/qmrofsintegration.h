#ifndef QMROFSINTEGRATION_H
#define QMROFSINTEGRATION_H

#include <qpa/qplatformintegration.h>
#include <qpa/qplatformnativeinterface.h>
#include <QtGui/qmrofs.h>          //public interface declaration in QtGui

QT_BEGIN_NAMESPACE

class QMrOfsIntegrationPrivate;
class QAbstractEventDispatcher;
class QMrOfsScreen;
class QMrOfsCompositor;

class QMrOfsIntegration : public QPlatformNativeInterface, public QPlatformIntegration
{
    Q_OBJECT
        
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
    QPlatformNativeInterface *nativeInterface() const;

    //TODO:  =================
    // compositing in independent thread (worker + QThread + moveToThread), (mrofs providing)
    // modify & flush waves in independent thread (app providing)
    // QMrOfs thread-safe (no-apartment mode)
    // QMrOfsIntegration thread-safe (no-apartment mode)
    // QScreen thread-safe
private:
    QMrOfsScreen              *m_primaryScreen;
    QPlatformFontDatabase   *m_fontDb;
    
};

QT_END_NAMESPACE

#endif // QLINUXFBINTEGRATION_H

