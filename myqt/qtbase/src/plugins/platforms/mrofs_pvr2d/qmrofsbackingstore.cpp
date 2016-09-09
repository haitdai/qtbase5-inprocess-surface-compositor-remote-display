#include "qmrofsscreen.h"
#include "qmrofswindow.h"
#include "qmrofscompositor.h"
#include "qmrofscursor.h"
#include "qmrofsbackingstore.h"
#include "qmrofsscreen.h"
#include <QtGui/qscreen.h>
#include <QtGui/qpainter.h>

QMrOfsBackingStore::QMrOfsBackingStore(QWindow *window) : 
    QPlatformBackingStore(window), m_image(NULL), m_image_shadow(NULL), m_compositor(NULL)
{
    QMrOfsScreen *native_scrn = static_cast<QMrOfsScreen *>(window->screen()->handle());
    QMrOfsWindow *native_win = static_cast<QMrOfsWindow*>(window->handle());
    if (native_win)
        native_win->setBackingStore(this);
    else
        native_scrn->addBackingStore(this);

    m_compositor = new QMrOfsCompositor(native_win, this);
    QMrOfsCursor *cur = reinterpret_cast<QMrOfsCursor*>(native_scrn->cursor());
    cur->setCompositor(reinterpret_cast<QMrOfsCompositor*>(m_compositor));
}

QMrOfsBackingStore::~QMrOfsBackingStore()
{
    delete m_image;
    m_image = NULL;
    delete m_image_shadow;
    m_image_shadow = NULL;
    delete m_compositor;
    m_compositor = NULL;
}

QPaintDevice *QMrOfsBackingStore::paintDevice()
{
    return m_image_shadow ? m_image_shadow->image() : NULL;
}

/*
    offset: ref QWidgetBackingStore::tlwOffset, (0, 0) in most time
*/
void QMrOfsBackingStore::flush(QWindow *window, const QRegion &region, const QPoint &offset)
{
    Q_UNUSED(offset);
    
    if (!m_image || m_image->image()->size().isEmpty() || 
        !m_image_shadow || m_image_shadow->image()->size().isEmpty())
        return;

    QSize imageSize = m_image->image()->size();

    QRegion clipped = region;
    clipped &= QRect(0, 0, window->width(), window->height());

    QRect bounds = clipped.boundingRect();
    if (bounds.isNull())
        return;
    m_compositor->lockBackingStoreResources();
    // sync shadow image to image
    QMrOfsShmImage*dst_image = reinterpret_cast<QMrOfsShmImage*>(m_image);
    QMrOfsShmImage *src_image =  reinterpret_cast<QMrOfsShmImage*>(m_image_shadow);
    src_image->wb(clipped);
    dst_image->simpleBlit(src_image, clipped);
    //there is no need of wb for dst_image since always accessed by GPU
    m_dirty = clipped;
    m_compositor->unlockBackingStoreResources();
    
    // TODO: move flushBackingStore to QPlatformCompositor
    (reinterpret_cast<QMrOfsCompositor*>(m_compositor))->flushBackingStore(m_dirty);
    //cleanup dirty regions  必须在合成器线程且flush 返回前执行, 否则下一次wave触发的合成将和它竞争
}

void QMrOfsBackingStore::resize(const QSize &size, const QRegion &staticContents)
{
    if (m_image && size == m_image->image()->size())
        return;
    
    // TODO: move resizeBackingStore to QPlatformCompositor
    (reinterpret_cast<QMrOfsCompositor*>(m_compositor))->resizeBackingStore(this, size, staticContents);    //callback into doResize in compositor thread, wait here...
}

/*!
    [compositor thread]
*/
void QMrOfsBackingStore::doResize(const QSize &size, const QRegion &staticContents)
{
    Q_UNUSED(staticContents);
    delete m_image;
    delete m_image_shadow;
    QMrOfsScreen *screen = static_cast<QMrOfsScreen *>(window()->screen()->handle());
    qDebug("QMrOfsBackingStore::doResize(%d, %d), create bs picture", size.width(), size.height());
    m_image = new QMrOfsShmImage(screen, size, 
        depthFromMainFormat(QMROFS_FORMAT_DEFAULT), 
        QMROFS_FORMAT_DEFAULT);
    m_image_shadow = new QMrOfsShmImage(screen, size, 
        depthFromMainFormat(QMROFS_FORMAT_DEFAULT), 
        QMROFS_FORMAT_DEFAULT);
}

void QMrOfsBackingStore::beginPaint(const QRegion &region)
{
    Q_UNUSED(region);

    if (!m_image || !m_image_shadow)
        return;
        
//    m_compositor->lockBackingStoreResources();

#if 0
    if (m_image->image()->hasAlphaChannel()) {
qDebug("!!!!!!!!!QMrOfsBackingStore::beginPaint has alpha channel");
        QPainter p(m_image->image());
        p.setCompositionMode(QPainter::CompositionMode_Source);
        const QVector<QRect> rects = region.rects();
        const QColor blank = Qt::transparent;
        for (QVector<QRect>::const_iterator it = rects.begin(); it != rects.end(); ++it) {
            p.fillRect(*it, blank);
        }
    }
#endif
}

void QMrOfsBackingStore::endPaint()
{
    // nothing to do
}

void QMrOfsBackingStore::endPaint2()
{
//    m_compositor->unlockBackingStoreResources();
}

QPlatformCompositor* QMrOfsBackingStore::compositor()
{
    return m_compositor;
}

/*!
    [compositor thread]
*/
const QRegion& QMrOfsBackingStore::effectiveClip()
{
    return m_dirty;
}

/*!
    [compositor thread]
*/
void QMrOfsBackingStore::cleanEffectiveClip()
{
    m_dirty = QRegion();
}

/*!
    for compositor
*/
QPlatformImage *QMrOfsBackingStore::platformImage()
{
    return m_image;
}

QSize QMrOfsBackingStore::size()
{
    return m_image->image()->size();
}

/*!
    1, offset: scroll image up/left is negative, scroll image down/right is positive
    2, scroll happens in bs shadow, dirty contents will be synced to screen when flushing
*/
bool QMrOfsBackingStore::scroll(const QRegion &area, int dx, int dy)
{
    if (!m_image || m_image->image()->isNull())
        return false;

    QMrOfsShmImage* native_img = dynamic_cast<QMrOfsShmImage*>(m_image_shadow);
    const QVector<QRect> rects = area.rects();
    for (int i = 0; i < rects.size(); ++i) {
        native_img->scroll(rects[i], QPoint(dx, dy));
    }
    return true;
}





