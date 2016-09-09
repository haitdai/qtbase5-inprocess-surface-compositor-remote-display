#ifndef QMROFSSCREEN_H
#define QMROFSSCREEN_H

#include <qpa/qplatformscreen.h>
#include <QtCore/QSize>
#include "qmrofsdefs.h"
#include "qmrofsimage.h"

QT_BEGIN_NAMESPACE

class QMrOfsWindow;
class QMrOfsCompositor;
class QMrOfsCursor;
class QMrOfsBackingStore;

/*!
	\brief base screen
*/
class QMrOfsScreen : public QObject, public QPlatformScreen
{
    Q_OBJECT
public:
    QMrOfsScreen(const QStringList &args);
    ~QMrOfsScreen();   /*< we ues virtual functions but NOT use polymorphism */

    //derived from QPlatformScreen
    QRect geometry() const Q_DECL_OVERRIDE { return mGeometry; }                      /*< design target: screen geometry will be changed when orientation changed */
    int depth() const Q_DECL_OVERRIDE { return mDepth; }
    int fbDepth() const {return mFbDepth; }
    QImage::Format format() const Q_DECL_OVERRIDE { return mFormat; }
    PVR2DFORMAT formatPVR2D() const { return mFormatPVR2D; }
    QSizeF physicalSize() const Q_DECL_OVERRIDE { return mPhysicalSize; }             /* < determing logicalDpi in QPlatformScreen/QScreen */
    QPlatformCursor *cursor() const Q_DECL_OVERRIDE;
    QWindow *topLevelAt(const QPoint & p) const Q_DECL_OVERRIDE;
    
    void addBackingStore(QMrOfsBackingStore *bs);
    
    virtual void addWindow(QMrOfsWindow *window);
    virtual void removeWindow(QMrOfsWindow *window);
    virtual void raise(QMrOfsWindow *window);
    virtual void lower(QMrOfsWindow *window);
    virtual void topWindowChanged(QWindow *window);
    QWindow *topWindow() const;
    void setDirty(const QRect &rect);
    
    Qt::ScreenOrientation orientation() const { return mOrientation; }
    Qt::ScreenOrientation nativeOrientation() const {return mOrientation; }   /*< by pass the screen orientation update mask */
    /*!
        \note will change screen geometry also
        \sa setGeometry
    */
    Qt::ScreenOrientation setOrientation(Qt::ScreenOrientation orientation);
    /*!
        \sa QMrOfsIntegration::initialize
    */
    bool initialize(QMrOfsCompositor *compositor = NULL);    
    PVR2DCONTEXTHANDLE deviceContext();
    QMrOfsShmImage *picture();                // fb mem
    int stride();
private slots:    
    void setPhysicalSize(const QSize &size);
    void setGeometry(const QRect &rect);
    
private:
    bool initializeFbDev(const QString & fbDevice, const QSize & userMmSize);
    bool initializePVR2DDev();

    QList<QMrOfsWindow*>           m_winStack;    
    QRect                            mGeometry;          /* watch out: memory alignment !!!*/
    int                              mDepth;
    QImage::Format                   mFormat;
    PVR2DFORMAT                      mFormatPVR2D;
    QSizeF                           mPhysicalSize;

    Qt::ScreenOrientation            mOrientation;       /*<  screen orientation */
    QMrOfsCursor                  *m_cursor;
    
    QStringList mArgs;                                   /*<plugin arguments */
    int mFbFd;                                           /*< fb */
    int mFbDepth;
    int mTtyFd;                                          /*< tty */
    int mOldTtyMode;    
    PVR2DCONTEXTHANDLE               m_pvr2DContext;
    PVR2DDISPLAYINFO                     m_displayInfo;
    QMrOfsShmImage                *m_pvr2DfbPic;
    QList <QMrOfsBackingStore*>    m_backingStores;
};

QT_END_NAMESPACE

#endif // QMROFSSCREEN_H
