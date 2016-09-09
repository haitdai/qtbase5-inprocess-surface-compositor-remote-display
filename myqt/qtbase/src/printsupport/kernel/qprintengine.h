/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtGui module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QPRINTENGINE_H
#define QPRINTENGINE_H

#include <QtCore/qvariant.h>
#include <QtPrintSupport/qprinter.h>

QT_BEGIN_NAMESPACE


#ifndef QT_NO_PRINTER

#ifdef QT_PRINTFILE
#define QMDrawCmd_JobId QPrintEngine::PrintEnginePropertyKey(0xfe00)
#define QMDrawCmd_TotalPage QPrintEngine::PrintEnginePropertyKey(0xfe01)
#define QMDrawCmd_CurrentPage QPrintEngine::PrintEnginePropertyKey(0xfe02)
#define QMDrawCmd_Rotate90 QPrintEngine::PrintEnginePropertyKey(0xfe03)
#define QMDrawCmd_ShowSpeed QPrintEngine::PrintEnginePropertyKey(0xfe04)
#define QMDrawCmd_FileType QPrintEngine::PrintEnginePropertyKey(0xfe05)
#define QMDrawCmd_ShowAreaSize QPrintEngine::PrintEnginePropertyKey(0xfe06)
#define QMDrawCmd_ShowAreaDpiWToH QPrintEngine::PrintEnginePropertyKey(0xfe07)
#define QMDrawCmd_CmdFileData QPrintEngine::PrintEnginePropertyKey(0xfe08)

#define PRINTER_A4_WIDTH 2331
#define PRINTER_A4_HEIGHT 3345
#define RECORD_WIDTH_MM(width_in_mm, width_dpi) ( ((int)( (width_in_mm + 0.001)*8) ) * (width_dpi/200) )	//宽是200dpi，以200dpi为基础计算
#define RECORD_HEIGHT 384
#endif //QT_PRINTFILE
class Q_PRINTSUPPORT_EXPORT QPrintEngine
{
public:
    virtual ~QPrintEngine() {}
    enum PrintEnginePropertyKey {
        PPK_CollateCopies,
        PPK_ColorMode,
        PPK_Creator,
        PPK_DocumentName,
        PPK_FullPage,
        PPK_NumberOfCopies,
        PPK_Orientation,
        PPK_OutputFileName,
        PPK_PageOrder,
        PPK_PageRect,
        PPK_PageSize,
        PPK_PaperRect,
        PPK_PaperSource,
        PPK_PrinterName,
        PPK_PrinterProgram,
        PPK_Resolution,
        PPK_SelectionOption,
        PPK_SupportedResolutions,

        PPK_WindowsPageSize,
        PPK_FontEmbedding,

        PPK_Duplex,

        PPK_PaperSources,
        PPK_CustomPaperSize,
        PPK_PageMargins,
        PPK_CopyCount,
        PPK_SupportsMultipleCopies,
        PPK_PaperName,
        PPK_PaperSize = PPK_PageSize,

        PPK_CustomBase = 0xff00
    };

    virtual void setProperty(PrintEnginePropertyKey key, const QVariant &value) = 0;
    virtual QVariant property(PrintEnginePropertyKey key) const = 0;

    virtual bool newPage() = 0;
    virtual bool abort() = 0;

    virtual int metric(QPaintDevice::PaintDeviceMetric) const = 0;

    virtual QPrinter::PrinterState printerState() const = 0;
};

#endif // QT_NO_PRINTER

QT_END_NAMESPACE

#endif // QPRINTENGINE_H
