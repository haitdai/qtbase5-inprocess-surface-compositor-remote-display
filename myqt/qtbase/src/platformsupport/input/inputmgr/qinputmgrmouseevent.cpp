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

#include "qinputmgrmouseevent_p.h"
#include <QDebug>
#include "InputMgr.h"

QT_BEGIN_NAMESPACE

QInputmgrMouseEvent::QInputmgrMouseEvent(QInputmgrEventListener &listener)
    : m_listener(listener), m_x(0),m_y(0)
{

}

QInputmgrMouseEvent::~QInputmgrMouseEvent()
{

}

void QInputmgrMouseEvent::handleMouseEvent(::input_event* evt)
{
    struct ::input_event *data = evt;
    if (data->type == EV_ABS) {
        if (data->code == ABS_X && m_x != data->value) {
            m_x = data->value;
            m_posChanged = true;
        } else if (data->code == ABS_Y && m_y != data->value) {
            m_y = data->value;
            m_posChanged = true;
        }
    } else if (data->type == EV_REL) {
        if (data->code == REL_X) {
            m_x += data->value;
            m_posChanged = true;
        } else if (data->code == REL_Y) {
            m_y += data->value;
            m_posChanged = true;
        } else if (data->code == ABS_WHEEL) { // vertical scroll
            // data->value: 1 == up, -1 == down
            const int delta = 120 * data->value;
			m_listener.handleWheelEvent(delta, Qt::Vertical);
        } else if (data->code == ABS_THROTTLE) { // horizontal scroll
            // data->value: 1 == right, -1 == left
            const int delta = 120 * -data->value;
            m_listener.handleWheelEvent(delta, Qt::Horizontal);
        }
    }else if (data->type == EV_KEY && data->code >= BTN_LEFT && data->code <= BTN_JOYSTICK) {
        Qt::MouseButton button = Qt::NoButton;
        // BTN_LEFT == 0x110 in kernel's input.h
        // The range of possible mouse buttons ends just before BTN_JOYSTICK, value 0x120.
        switch (data->code) {
        case 0x110: button = Qt::LeftButton; break;    // BTN_LEFT
        case 0x111: button = Qt::RightButton; break;
        case 0x112: button = Qt::MiddleButton; break;
        case 0x113: button = Qt::ExtraButton1; break;  // AKA Qt::BackButton
        case 0x114: button = Qt::ExtraButton2; break;  // AKA Qt::ForwardButton
        case 0x115: button = Qt::ExtraButton3; break;  // AKA Qt::TaskButton
        case 0x116: button = Qt::ExtraButton4; break;
        case 0x117: button = Qt::ExtraButton5; break;
        case 0x118: button = Qt::ExtraButton6; break;
        case 0x119: button = Qt::ExtraButton7; break;
        case 0x11a: button = Qt::ExtraButton8; break;
        case 0x11b: button = Qt::ExtraButton9; break;
        case 0x11c: button = Qt::ExtraButton10; break;
        case 0x11d: button = Qt::ExtraButton11; break;
        case 0x11e: button = Qt::ExtraButton12; break;
        case 0x11f: button = Qt::ExtraButton13; break;
        }
        if (data->value)
            m_buttons |= button;
        else
            m_buttons &= ~button;
        m_btnChanged = true;
    } else if (data->type == EV_SYN && data->code == SYN_REPORT) {
        if (m_btnChanged) {
            m_btnChanged = m_posChanged = false;
            m_listener.handleMouseEvent(m_x, m_y, m_buttons);
        } else if (m_posChanged) {
            m_posChanged = false;
            m_listener.handleMouseEvent(m_x, m_y, m_buttons);
        }
    } 
}

QT_END_NAMESPACE
