#ifndef QMROFSBACKINGSTORE_H
#define QMROFSBACKINGSTORE_H

#include <qpa/qplatformbackingstore.h>
#include "qmrofsimage.h"
#include "qmrofsdefs.h"
#include <QtPlatformSupport/private/qplatformcompositor_p.h> 

QT_BEGIN_NAMESPACE

class QMrOfsCompositor;

class QMrOfsBackingStore : public QPlatformBackingStore {
public:
    QMrOfsBackingStore(QWindow *window);
    virtual ~QMrOfsBackingStore();

    QPaintDevice *paintDevice();
    void flush(QWindow *window, const QRegion &region, const QPoint &offset);
    void resize(const QSize &size, const QRegion &staticContents);
    QSize size();
    bool scroll(const QRegion &area, int dx, int dy);     // scroll acceleration

    void beginPaint(const QRegion &region);
    void endPaint();        
    void endPaint2();
    
    QPlatformImage *platformImage();
    void doResize(const QSize &size, const QRegion &staticContents);                   //callback [in compositor thread]
    QPlatformCompositor*  compositor();             // return QMrOfsCompositor
    const QRegion& effectiveClip();                    //region for compositor,  compositor thread
    void cleanEffectiveClip();                            //compositor thread    
private:
    QPlatformImage                     *m_image;
    QPlatformImage                     *m_image_shadow;    // bs shadow
    QRegion                                 m_dirty;              // bs dirty region, used by compositor
    QPlatformCompositor          *m_compositor;
};

QT_END_NAMESPACE

#endif //QMROFSBACKINGSTORE_H


