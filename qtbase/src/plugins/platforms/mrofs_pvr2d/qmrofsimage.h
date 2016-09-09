#ifndef QMROFSIMAGE_H
#define QMROFSIMAGE_H

#include <QtGui/QImage>
#include <QtGui/QPixmap>
#include "qmrofsdefs.h"
#include <QtPlatformSupport/private/qplatformcompositor_p.h>    // QPlatformImage
#include "cmem.h"

QT_BEGIN_NAMESPACE

class QMrOfsScreen;

/*!
    share between CPU and GPU
*/
class QMrOfsShmImage : public QPlatformImage {
public:
    QMrOfsShmImage(QMrOfsScreen *screen, 
                                      const QSize &size, 
                                      uint depth,                                       
                                      QAbstractMrOfs::Format format, 
                                      PVR2DMEMINFO *alreadyWrappedMem = NULL);
    virtual ~QMrOfsShmImage();
    QImage *image();
    const QImage *image() const;
    QSize size();
    uint stride();
    PVR2DMEMINFO *pvr2DMem();                    // compositor thread
    PVR2DFORMAT  pvr2DFormat();
    void simpleBlit(QMrOfsShmImage *src, const QRegion&clip);    
    void wb(const QRegion& dirty);                  //write back
    void wb(const QRect& dirty);
    void inv(const QRegion& dirty);
    void inv(const QRect& dirty);        
    void scroll(const QRect &rect, const QPoint &offset);
    
protected:
    QMrOfsScreen           *m_scrn;
    QImage                  m_qimage;
    PVR2DCONTEXTHANDLE      m_pvr2DDC;           // pvr2d Device Context
    uchar                  *m_bits;
    CMEM_AllocParams        m_allocParams;    
    PVR2DMEMINFO           *m_pvr2DMem;
    bool                    m_outerBits;         // bits is allocated outside
    PVR2DFORMAT             m_pvr2DFormat;
    uint                    m_bytesPerPixel;
    uint                    m_strideInByte;     
    uint                    m_sizeInByte;
    PVR2DBLTINFO            m_blt_inf;
};


QT_END_NAMESPACE


#endif // QMROFSIMAGE_H
