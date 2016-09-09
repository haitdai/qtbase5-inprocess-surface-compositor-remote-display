#ifndef QMROFS_P_H
#define QMROFS_P_H

#include <QtCore/qnamespace.h>
#include "private/qobject_p.h"
#include <QtWidgets/qmrofs.h>
#include <QtWidgets/qplatformcompositor.h>      // QPlatformCompositor

QT_BEGIN_NAMESPACE

class Q_WIDGETS_EXPORT QMrOfsPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QMrOfs)
        
public:
    QMrOfsPrivate();
    ~QMrOfsPrivate();          // auto virtual
    
protected:
    /*!
        \brief 
        for QMrOfs, compositor is the only 1 compositor >>>worked in primary screen<<<, which compositing 1st TLW and WAVE VIEW.
        for QMrOfsV4, compositor is the only 1 compositor >>>worked in TLW<<<, which compositing TLW, WAVE VIEWs, and iVIEW.
    */
    QPlatformCompositor *compositor;
    Qt::ConnectionType       connType; 
};
    
QT_END_NAMESPACE
    
#endif

