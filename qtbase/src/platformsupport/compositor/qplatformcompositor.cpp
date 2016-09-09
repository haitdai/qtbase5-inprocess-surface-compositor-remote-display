#include "qplatformcompositor_p.h"

#ifdef QT_MRDP
bool QPlatformCompositorBase::startMRDPService()
{
    return false;
}

void QPlatformCompositorBase::stopMRDPService()
{

}

bool QPlatformCompositorBase::MRDPServiceRunning()
{
    return false;
}

static QRegion dummy_rgn;
const QRegion& QPlatformCompositorBase::screenDirtyRegion()
{
    return dummy_rgn;
}

static QImage dummy_img;
const QImage& QPlatformCompositorBase::screenImage()
{
    return dummy_img;
}

void QPlatformCompositorBase::lockScreenResources()
{

}

void QPlatformCompositorBase::unlockScreenResources()
{

}
#endif //QT_MRDP


