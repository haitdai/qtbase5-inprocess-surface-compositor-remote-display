#include "qmrofs.h"
#include "qmrofsscreen.h"
#include "qmrofsimage.h"

/*!
    NOTE:  dirty information maintained by backingstore and waveform independently !!!
*/
QMrOfsShmImage::QMrOfsShmImage(QMrOfsScreen *screen, const QSize &size, uint depth, 
    QAbstractMrOfs::Format format, PVR2DMEMINFO *alreadyWrappedMem) :
    m_scrn(screen), m_pvr2DDC(0), m_bits(NULL), m_allocParams(CMEM_DEFAULTPARAMS), m_pvr2DMem(NULL), m_bytesPerPixel(depth/8), m_sizeInByte(0)
{
    if(!screen) {
        qFatal("QMrOfsShmImage::QMrOfsShmImage, screen is NULL");
    }

    m_pvr2DFormat = pvr2DFormatFromMainFormat(format);

    m_pvr2DDC = screen->deviceContext();
    int stride_in_pixel = (size.width() + 31) & ~31;       // 32-pixel alignment
    int stride_in_byte = 0;
    int size_in_byte = 0;
    if(alreadyWrappedMem) {                              // memory already wrapped, such as fb mmaped then wrapped (PVR2DGetFrameBuffer): use it
        m_pvr2DMem = alreadyWrappedMem;
        m_bits = static_cast<uchar*>((m_pvr2DMem->pBase));
        stride_in_byte = stride_in_pixel * m_bytesPerPixel;
        size_in_byte = stride_in_byte * size.height();
        m_outerBits = true;
    } else {                                  	         // not allocated yet: allocate in CPU page table(non-contigous)
        stride_in_byte = m_bytesPerPixel * stride_in_pixel;
        size_in_byte = stride_in_byte * size.height();
        if(size_in_byte <= 0) {
            qWarning("QMrOfsShmImage::QMrOfsShmImage: size_in_byte(%d)", size_in_byte);
            return;
        }
#ifdef GPU_SURFACE
        qDebug("QMrOfsShmImage::QMrOfsShmImage: GPU_SURFACE");
#   ifdef QMROFS_MEM_ALLOCATED_PVR                  // only noncache is available
        qDebug("QMrOfsShmImage::QMrOfsShmImage: PVR2DMemAlloc");
        PVR2DERROR err = PVR2DMemAlloc(screen->deviceContext(), size_in_byte, 128, 0, &m_pvr2DMem);
        if(err != PVR2D_OK || !m_pvr2DMem) {
            qFatal("QMrOfsShmImage::QMrOfsShmImage, PVR2DMemAlloc failed(%d), depth(%d), width(%d), height(%d)", 
                err, depth, size.width(), size.height());
        }
        m_bits = static_cast<uchar*>(m_pvr2DMem->pBase);
#   elif defined(QMROFS_MEM_ALLOCATED_MALLOC)        // we do NOT know how to noncache / cache write back
        qDebug("QMrOfsShmImage::QMrOfsShmImage: malloc");
        m_bits = static_cast<uchar*>(malloc(size_in_byte));
        if(!m_bits) {
            qFatal("QMrOfsShmImage::QMrOfsShmImage, malloc failed, depth(%d), width(%d), height(%d)", 
                depth, size.width(), size.height());
        }

        // wrap it to GPU page table(non-contiguous)
        PVR2DERROR err = PVR2DMemWrap(screen->deviceContext(), m_bits, PVR2D_WRAPFLAG_NONCONTIGUOUS, size_in_byte, NULL, &m_pvr2DMem);
        if(err != PVR2D_OK || !m_pvr2DMem) {
            qFatal("QMrOfsShmImage::QMrOfsShmImage, unable to wrap memory(%p), dc(%p)", m_bits, screen->deviceContext());
        }
#   else       // QMROFS_MEM_ALLOCATED_CMEM                         // we can use CMEM_cacheWb
        m_allocParams.type = CMEM_POOL;
        m_allocParams.flags = CMEM_CACHED;                        // CMEM's NONCACHE mode is much slower than PVR2D's
        m_bits = reinterpret_cast<uchar*>(CMEM_alloc(size_in_byte, &m_allocParams));
        if(!m_bits) {
            qWarning("CMEM_alloc from pool return NULL, try heap alloc");
            m_allocParams.type = CMEM_HEAP;
            m_allocParams.flags = CMEM_CACHED;
            m_bits = reinterpret_cast<uchar*>(CMEM_alloc(size_in_byte, &m_allocParams));
            if(!m_bits) {
                qFatal("CMEM_alloc from heap return NULL");
            }
        }
        // check alignment
        unsigned long phy_addr = CMEM_getPhys((void*)m_bits);
        if(phy_addr & 0xFFF) {
            qFatal("PVR2DMemWrap may have issues with this non-aligned address!\n");
        }

        // wrap it - physical address is contiguous for cmem pool
        unsigned long pageAddr[2];        
        pageAddr[0] = reinterpret_cast<PVR2D_ULONG>(m_bits) & 0xFFFFF000;
        pageAddr[1] = 0;        
        PVR2DERROR err = PVR2DMemWrap(screen->deviceContext(), m_bits, PVR2D_WRAPFLAG_CONTIGUOUS, size_in_byte, pageAddr, &m_pvr2DMem);
        if(err != PVR2D_OK || !m_pvr2DMem) {
            qFatal("QMrOfsShmImage::QMrOfsShmImage, unable to wrap memory(%p), dc(%p)", m_bits, screen->deviceContext());
        }
#   endif    
#else
        qDebug("QMrOfsShmImage::QMrOfsShmImage: CPU_SURFACE");
        m_bits = static_cast<uchar*>(malloc(size_in_byte));
        if(!m_bits) {
            qFatal("QMrOfsShmImage::QMrOfsShmImage, malloc failed, depth(%d), width(%d), height(%d)", 
                depth, size.width(), size.height());
        }
        m_pvr2DMem = malloc(sizeof(PVR2DMEMINFO));
        memset(m_pvr2DMem, 0, sizeof(PVR2DMEMINFO));
        m_pvr2DMem->pBase = m_bits;
#endif      // !defined(GPU_SURFACE)
        Q_ASSERT(m_bits == static_cast<uchar*>((m_pvr2DMem->pBase)));
        m_outerBits = false;
    }

    memset(&m_blt_inf, 0, sizeof(m_blt_inf));
    
    // note: size of QImage may be not the same as sizein arguments, but must be the same as pvr2dMem's
    m_qimage = QImage((uchar*) m_bits, stride_in_pixel, size.height(), stride_in_byte, qtFormatFromMainFormat(format));
    m_strideInByte = stride_in_byte;
    m_sizeInByte = size_in_byte;
    qDebug("QMrOfsShmImage::QMrOfsShmImage:  \
        pvr2DMem(%p), m_bits(%p), pBase(%p), phy(%lx),   \
        width(%d), height(%d), stride_in_pixel(%d), stride_in_byte(%d), size_in_byte(%d),   \
        outerbits(%d), format(%d)", 
        m_pvr2DMem, m_bits, m_pvr2DMem->pBase, m_pvr2DMem->ui32DevAddr, 
        size.width(), size.height(), stride_in_pixel, m_strideInByte, m_sizeInByte, 
        m_outerBits, qtFormatFromMainFormat(format));
}

QMrOfsShmImage::~QMrOfsShmImage()
{
    if(!m_outerBits) {
        if(m_pvr2DMem) {
#ifdef GPU_SURFACE
            qDebug("QMrOfsShmImage::~QMrOfsShmImage: PVR2DMemFree(%p), bits(%p), base(%p)", m_pvr2DMem, m_bits, m_pvr2DMem->pBase);
            PVR2DMemFree(m_pvr2DDC, m_pvr2DMem);            // PVR2DMemAlloc or PVR2DMemWrap are all freed by PVR2DMemFree
#else
            free(m_pvr2DMem);
#endif
            m_pvr2DMem = NULL;
        }
        if(m_bits) {
#ifdef QMROFS_MEM_ALLOCATED_MALLOC
            free(m_bits);
#elif defined(QMROFS_MEM_ALLOCATED_CMEM)
            CMEM_free(m_bits, &m_allocParams);
#endif
            m_bits = NULL;
        }
    }
}

QImage *QMrOfsShmImage::image()
{
    return &m_qimage;
}

const QImage *QMrOfsShmImage::image() const
{
    return &m_qimage;
}

/*!
    if m_pvr2DMem is not allocated, QImage.size is QSize(0, 0)
*/
QSize QMrOfsShmImage::size()
{
    return m_qimage.size();
}

uint QMrOfsShmImage::stride()
{
    return m_strideInByte;
}

PVR2DMEMINFO *QMrOfsShmImage::pvr2DMem()
{
    return m_pvr2DMem;
}

PVR2DFORMAT  QMrOfsShmImage::pvr2DFormat()
{
    return m_pvr2DFormat;
}

/*!
    simple blit, src and dst must be in same size
*/
void QMrOfsShmImage::simpleBlit(QMrOfsShmImage *src, const QRegion&clip)
{
    Q_ASSERT(src->m_qimage.size() == m_qimage.size() && 
        src->m_qimage.format() == m_qimage.format());
        
  #if defined(DEBUG_GPU_BLT)

    QVector<QRect> dirty_rects = clip.rects();
    for(int i = 0; i < dirty_rects.size(); i ++) {
        const QRect &rc = dirty_rects.at(i);
        m_blt_inf.CopyCode = PVR2DROPcopy;
        m_blt_inf.BlitFlags = PVR2D_BLIT_DISABLE_ALL;               // SRC OP     
        m_blt_inf.pDstMemInfo = m_pvr2DMem;
        m_blt_inf.DstSurfWidth = m_qimage.width();
        m_blt_inf.DstSurfHeight = m_qimage.height();
        m_blt_inf.DstStride = m_strideInByte;
        m_blt_inf.DstFormat = m_pvr2DFormat;
        m_blt_inf.DSizeX = rc.width();
        m_blt_inf.DSizeY = rc.height();
        m_blt_inf.DstX = rc.x();
        m_blt_inf.DstY = rc.y();
        
        m_blt_inf.pSrcMemInfo = src->pvr2DMem();
        m_blt_inf.SrcSurfWidth = m_qimage.width();
        m_blt_inf.SrcSurfHeight = m_qimage.height();
        m_blt_inf.SrcStride = m_strideInByte;
        m_blt_inf.SrcFormat = m_pvr2DFormat;
        m_blt_inf.SizeX = rc.width();
        m_blt_inf.SizeY = rc.height();
        m_blt_inf.SrcX = rc.x();
        m_blt_inf.SrcY = rc.y();
        
        if(!m_blt_inf.pDstMemInfo || !m_blt_inf.pSrcMemInfo) {
            return;
        }
        PVR2DERROR err = PVR2D_OK;
        if((err = PVR2DBlt(m_scrn->deviceContext(), &m_blt_inf)) != PVR2D_OK) {
            qWarning("%s: PVR2DBlt failed: error(%d),     \
            CopyCode(%lu), BlitFlags(0x%x),     \
            DstSurfWidth(%lu), DstSurfHeight(%lu), DstStride(%ld), DstFormat(0x%x),  \
            DSizeX(%ld), DSizeY(%ld), DstX(%ld), DstY(%ld),     \
            SrcSurfWidth(%lu), SrcSurfHeight(%lu), SrcStride(%ld), SrcFormat(0x%x),  \
            SizeX(%ld), SizeY(%ld), SrcX(%ld), SrcY(%ld)",
            __FUNCTION__,
            err, 
            m_blt_inf.CopyCode, static_cast<uint>(m_blt_inf.BlitFlags), 
            m_blt_inf.DstSurfWidth, m_blt_inf.DstSurfHeight, m_blt_inf.DstStride, static_cast<uint>(m_blt_inf.DstFormat), 
            m_blt_inf.DSizeX, m_blt_inf.DSizeY, m_blt_inf.DstX, m_blt_inf.DstY, 
            m_blt_inf.SrcSurfWidth, m_blt_inf.SrcSurfHeight, m_blt_inf.SrcStride, static_cast<uint>(m_blt_inf.SrcFormat), 
            m_blt_inf.SizeX, m_blt_inf.SizeY, m_blt_inf.SrcX, m_blt_inf.SrcY);
        }
    }
#else
                // cpu copy is too much expensive, especially wide-range update
#endif
}

void QMrOfsShmImage::wb(const QRegion& dirty)
{
#ifdef QMROFS_MEM_ALLOCATED_CMEM
#if 0
    CMEM_cacheWb(m_bits, m_sizeInByte);
#else
    QVector<QRect> dirty_rects = dirty.rects();
    for(int i = 0; i < dirty_rects.size(); i ++) {
        for(int m = dirty_rects.at(i).top(); m < dirty_rects.at(i).top() + dirty_rects.at(i).height(); m++) {
            CMEM_cacheWb(m_bits + m * m_strideInByte + dirty_rects.at(i).left() * m_bytesPerPixel, dirty_rects.at(i).width() * m_bytesPerPixel);   // 1 line
        }    
    }
#endif    
#endif
}

void QMrOfsShmImage::wb(const QRect& dirty)
{
#ifdef QMROFS_MEM_ALLOCATED_CMEM
#if 0
    CMEM_cacheWb(m_bits, m_sizeInByte);
#else
    for(int m = dirty.top(); m < dirty.top() + dirty.height(); m++) {
        CMEM_cacheWb(m_bits + m * m_strideInByte + dirty.left() * m_bytesPerPixel, dirty.width() * m_bytesPerPixel);   // 1 line
    }    
#endif
#endif
}

void QMrOfsShmImage::inv(const QRegion& dirty)
{
#ifdef QMROFS_MEM_ALLOCATED_CMEM
#if 0
        CMEM_cacheWb(m_bits, m_sizeInByte);
#else
        QVector<QRect> dirty_rects = dirty.rects();
        for(int i = 0; i < dirty_rects.size(); i ++) {
            for(int m = dirty_rects.at(i).top(); m < dirty_rects.at(i).top() + dirty_rects.at(i).height(); m++) {
                CMEM_cacheInv(m_bits + m * m_strideInByte + dirty_rects.at(i).left() * m_bytesPerPixel, dirty_rects.at(i).width() * m_bytesPerPixel);   // 1 line
            }    
        }
#endif    
#endif
}

void QMrOfsShmImage::inv(const QRect& dirty)
{
#ifdef QMROFS_MEM_ALLOCATED_CMEM
#if 0
        CMEM_cacheWb(m_bits, m_sizeInByte);
#else
        for(int m = dirty.top(); m < dirty.top() + dirty.height(); m++) {
            CMEM_cacheInv(m_bits + m * m_strideInByte + dirty.left() * m_bytesPerPixel, dirty.width() * m_bytesPerPixel);   // 1 line
        }    
#endif
#endif

}

extern void qt_scrollRectInImage(QImage &img, const QRect &rect, const QPoint &offset);
/*!
    accelerated by pvr2d, [ui thread]
    FIXME: 如果本QMrOfsShmImage 在其它线程也操作了(by cpu), 那么需要lock
*/
void QMrOfsShmImage::scroll(const QRect &rect, const QPoint &offset)
{
    const QRect imageRect(0, 0, m_qimage.width(), m_qimage.height());
    const QRect r = rect & imageRect & imageRect.translated(-offset);
    const QPoint p = rect.topLeft() + offset;
    QRect src_rc(r.left(), r.top(), r.width(), r.height()), dest_rc(p.x(), p.y(), r.width(), r.height());
    
    if (r.isEmpty())
        return;
        
    wb(src_rc);

#if defined(DEBUG_GPU_BLT)
    m_blt_inf.CopyCode = PVR2DROPcopy;
    m_blt_inf.BlitFlags = PVR2D_BLIT_DISABLE_ALL;
    m_blt_inf.pDstMemInfo = m_pvr2DMem;
    m_blt_inf.DstSurfWidth = m_qimage.width();
    m_blt_inf.DstSurfHeight = m_qimage.height();
    m_blt_inf.DstStride = m_strideInByte;
    m_blt_inf.DstFormat = m_pvr2DFormat;
    m_blt_inf.DSizeX = dest_rc.width();
    m_blt_inf.DSizeY = dest_rc.height();
    m_blt_inf.DstX = dest_rc.x();
    m_blt_inf.DstY = dest_rc.y();
    
    m_blt_inf.pSrcMemInfo = m_pvr2DMem;
    m_blt_inf.SrcSurfWidth = m_qimage.width();
    m_blt_inf.SrcSurfHeight = m_qimage.height();
    m_blt_inf.SrcStride = m_strideInByte;
    m_blt_inf.SrcFormat = m_pvr2DFormat;
    m_blt_inf.SizeX = src_rc.width();
    m_blt_inf.SizeY = src_rc.height();
    m_blt_inf.SrcX = src_rc.x();
    m_blt_inf.SrcY = src_rc.y();
    
    if(!m_blt_inf.pDstMemInfo || !m_blt_inf.pSrcMemInfo) {
        return;
    }
    PVR2DERROR err = PVR2D_OK;
    if((err = PVR2DBlt(m_scrn->deviceContext(), &m_blt_inf)) != PVR2D_OK) {
        qWarning("%s: PVR2DBlt failed: error(%d),     \
        CopyCode(%lu), BlitFlags(0x%x),     \
        DstSurfWidth(%lu), DstSurfHeight(%lu), DstStride(%ld), DstFormat(0x%x),  \
        DSizeX(%ld), DSizeY(%ld), DstX(%ld), DstY(%ld),     \
        SrcSurfWidth(%lu), SrcSurfHeight(%lu), SrcStride(%ld), SrcFormat(0x%x),  \
        SizeX(%ld), SizeY(%ld), SrcX(%ld), SrcY(%ld)",
        __FUNCTION__,
        err, 
        m_blt_inf.CopyCode, static_cast<uint>(m_blt_inf.BlitFlags), 
        m_blt_inf.DstSurfWidth, m_blt_inf.DstSurfHeight, m_blt_inf.DstStride, static_cast<uint>(m_blt_inf.DstFormat), 
        m_blt_inf.DSizeX, m_blt_inf.DSizeY, m_blt_inf.DstX, m_blt_inf.DstY, 
        m_blt_inf.SrcSurfWidth, m_blt_inf.SrcSurfHeight, m_blt_inf.SrcStride, static_cast<uint>(m_blt_inf.SrcFormat), 
        m_blt_inf.SizeX, m_blt_inf.SizeY, m_blt_inf.SrcX, m_blt_inf.SrcY);
    }

    // tell cpu the cache in dest_rc is invalid
    inv(dest_rc);
    
    // wait sgx530 to finish it's job
    PVR2DQueryBlitsComplete(m_scrn->deviceContext(), m_pvr2DMem, 1);
    
    // let pvr2d to handle overlapping situation
#else
    qt_scrollRectInImage(m_qimage, rect, offset);
#endif
}

