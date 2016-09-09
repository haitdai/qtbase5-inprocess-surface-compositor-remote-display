#ifndef _QPLATFORMCOMPOSITOR_P_H_
#define _QPLATFORMCOMPOSITOR_P_H_

#include "QtWidgets/qplatformcompositor.h"

class QPlatformImage
{
public:
    virtual ~QPlatformImage() {};
    virtual QImage *image() = 0;
    virtual const QImage *image() const = 0;
};

class QPlatformCompositorBase : public QPlatformCompositor {
#ifdef QT_MRDP
public:
    bool startMRDPService();
    void stopMRDPService();
    bool MRDPServiceRunning();
    const QRegion& screenDirtyRegion();
    const QImage& screenImage();
    void lockScreenResources();
    void unlockScreenResources();    
#endif    
};

#endif  // _QPLATFORMCOMPOSITOR_P_H_
