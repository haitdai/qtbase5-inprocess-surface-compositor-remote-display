#ifndef QMROFSSCREEN_H
#define QMROFSSCREEN_H

#include <qpa/qplatformscreen.h>
#include <QtCore/QSize>

QT_BEGIN_NAMESPACE

class QMrOfsWindow;
class QMrOfsCompositor;

#ifdef  MROFS_USE_16BIT
#define QMROFS_SCREEN_DEPTH   16
#define QMROFS_SCREEN_FORMAT   QImage::Format_RGB16
#elif defined(MROFS_USE_32BIT)
#define QMROFS_SCREEN_DEPTH      32
#define QMROFS_SCREEN_FORMAT   QImage::Format_RGB32
#endif

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
    QSizeF physicalSize() const Q_DECL_OVERRIDE { return mPhysicalSize; }             /* < determing logicalDpi in QPlatformScreen/QScreen */
    QPlatformCursor *cursor() const Q_DECL_OVERRIDE;
    QWindow *topLevelAt(const QPoint & p) const Q_DECL_OVERRIDE;
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
    bool initialize(QMrOfsCompositor *compositor);    
    
private slots:    
    void setPhysicalSize(const QSize &size);
    void setGeometry(const QRect &rect);
    
private:
    bool initializeFbDev(const QString & fbDevice, const QSize & userMmSize);
    
    QRect                                mGeometry;
    int                                     mDepth;
    QImage::Format                 mFormat;
    QSizeF                               mPhysicalSize;

    Qt::ScreenOrientation         mOrientation;        /*<  screen orientation */
    QMrOfsCompositor          *mCompositor;       /*< compositor object of this screen */

    QStringList mArgs;                                         /*<plugin arguments */
    int mFbFd;                                                       /*< fb */
    int mFbDepth;
    int mTtyFd;                                                      /*< tty */
    int mOldTtyMode;    
};

QT_END_NAMESPACE

#endif // QFBSCREEN_H
