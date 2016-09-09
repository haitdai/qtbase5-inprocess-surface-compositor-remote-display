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

#include <QDebug>
#include "qinputmgrkeyboardevent_p.h"

QT_BEGIN_NAMESPACE
// simple builtin US keymap
#include "qinputmgrkeyboard_defaultmap_p.h"

//#define QT_QPA_KEYMAP_DEBUG

QInputmgrKeyboardEvent::QInputmgrKeyboardEvent(QInputmgrEventListener &listener)
	: m_listener(listener), m_modifiers(0), m_composing(0), m_dead_unicode(0xffff),
      m_no_zap(true), m_do_compose(false),
      m_keymap(0), m_keymap_size(0), m_keycompose(0), m_keycompose_size(0)
{
	qDebug("QInputmgrKeyboardEvent create");
	loadDefaultKeymap();
}

QInputmgrKeyboardEvent::~QInputmgrKeyboardEvent()
{
}

void QInputmgrKeyboardEvent::handleLockEvent(int key,int state)
{
	m_locks[key] = state;
}

void QInputmgrKeyboardEvent::handleKeyEvent(::input_event* evt)
{
	quint16 code = evt->code;
    qint32 value = evt->value;

    QInputmgrKeyboardEvent::KeycodeAction ka;
    ka = processKeycode(code, value != 0, value == 2);

    switch (ka) {
    case QInputmgrKeyboardEvent::CapsLockOn:
    case QInputmgrKeyboardEvent::CapsLockOff:
        m_listener.handleSwitchLed(LED_CAPSL, ka == QInputmgrKeyboardEvent::CapsLockOn);
        break;

    case QInputmgrKeyboardEvent::NumLockOn:
    case QInputmgrKeyboardEvent::NumLockOff:
        m_listener.handleSwitchLed(LED_NUML, ka == QInputmgrKeyboardEvent::NumLockOn);
        break;

    case QInputmgrKeyboardEvent::ScrollLockOn:
    case QInputmgrKeyboardEvent::ScrollLockOff:
        m_listener.handleSwitchLed(LED_SCROLLL, ka == QInputmgrKeyboardEvent::ScrollLockOn);
        break;

    default:
        // ignore console switching and reboot
        break;
    }
}

QInputmgrKeyboardEvent::KeycodeAction QInputmgrKeyboardEvent::processKeycode(quint16 keycode, bool pressed, bool autorepeat)
{
    KeycodeAction result = None;
    bool first_press = pressed && !autorepeat;

    const QInputmgrKeyboardMap::Mapping *map_plain = 0;
    const QInputmgrKeyboardMap::Mapping *map_withmod = 0;

    quint8 modifiers = m_modifiers;

    // get a specific and plain mapping for the keycode and the current modifiers
    for (int i = 0; i < m_keymap_size && !(map_plain && map_withmod); ++i) {
        const QInputmgrKeyboardMap::Mapping *m = m_keymap + i;
        if (m->keycode == keycode) {
            if (m->modifiers == 0)
                map_plain = m;

            quint8 testmods = m_modifiers;
            if (m_locks[0] /*CapsLock*/ && (m->flags & QInputmgrKeyboardMap::IsLetter))
                testmods ^= QInputmgrKeyboardMap::ModShift;
            if (m->modifiers == testmods)
                map_withmod = m;
        }
    }

    if (m_locks[0] /*CapsLock*/ && map_withmod && (map_withmod->flags & QInputmgrKeyboardMap::IsLetter))
        modifiers ^= QInputmgrKeyboardMap::ModShift;

#ifdef QT_QPA_KEYMAP_DEBUG
    qWarning("Processing key event: keycode=%3d, modifiers=%02x pressed=%d, autorepeat=%d  |  plain=%d, withmod=%d, size=%d", \
             keycode, modifiers, pressed ? 1 : 0, autorepeat ? 1 : 0, \
             map_plain ? map_plain - m_keymap : -1, \
             map_withmod ? map_withmod - m_keymap : -1, \
             m_keymap_size);
#endif

    const QInputmgrKeyboardMap::Mapping *it = map_withmod ? map_withmod : map_plain;

    if (!it) {
#ifdef QT_QPA_KEYMAP_DEBUG
        // we couldn't even find a plain mapping
        qWarning("Could not find a suitable mapping for keycode: %3d, modifiers: %02x", keycode, modifiers);
#endif
        return result;
    }

    bool skip = false;
    quint16 unicode = it->unicode;
    quint32 qtcode = it->qtcode;

    if ((it->flags & QInputmgrKeyboardMap::IsModifier) && it->special) {
        // this is a modifier, i.e. Shift, Alt, ...
        if (pressed)
            m_modifiers |= quint8(it->special);
        else
            m_modifiers &= ~quint8(it->special);
    } else if (qtcode >= Qt::Key_CapsLock && qtcode <= Qt::Key_ScrollLock) {
        // (Caps|Num|Scroll)Lock
        if (first_press) {
            quint8 &lock = m_locks[qtcode - Qt::Key_CapsLock];
            lock ^= 1;

            switch (qtcode) {
            case Qt::Key_CapsLock  : result = lock ? CapsLockOn : CapsLockOff; break;
            case Qt::Key_NumLock   : result = lock ? NumLockOn : NumLockOff; break;
            case Qt::Key_ScrollLock: result = lock ? ScrollLockOn : ScrollLockOff; break;
            default                : break;
            }
        }
    } else if ((it->flags & QInputmgrKeyboardMap::IsSystem) && it->special && first_press) {
        switch (it->special) {
        case QInputmgrKeyboardMap::SystemReboot:
            result = Reboot;
            break;

        case QInputmgrKeyboardMap::SystemZap:
            if (!m_no_zap)
                qApp->quit();
            break;

        case QInputmgrKeyboardMap::SystemConsolePrevious:
            result = PreviousConsole;
            break;

        case QInputmgrKeyboardMap::SystemConsoleNext:
            result = NextConsole;
            break;

        default:
            if (it->special >= QInputmgrKeyboardMap::SystemConsoleFirst &&
                it->special <= QInputmgrKeyboardMap::SystemConsoleLast) {
                result = KeycodeAction(SwitchConsoleFirst + ((it->special & QInputmgrKeyboardMap::SystemConsoleMask) & SwitchConsoleMask));
            }
            break;
        }

        skip = true; // no need to tell Qt about it
    } else if ((qtcode == Qt::Key_Multi_key) && m_do_compose) {
        // the Compose key was pressed
        if (first_press)
            m_composing = 2;
        skip = true;
    } else if ((it->flags & QInputmgrKeyboardMap::IsDead) && m_do_compose) {
        // a Dead key was pressed
        if (first_press && m_composing == 1 && m_dead_unicode == unicode) { // twice
            m_composing = 0;
            qtcode = Qt::Key_unknown; // otherwise it would be Qt::Key_Dead...
        } else if (first_press && unicode != 0xffff) {
            m_dead_unicode = unicode;
            m_composing = 1;
            skip = true;
        } else {
            skip = true;
        }
    }

    if (!skip) {
        // a normal key was pressed
        const int modmask = Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier | Qt::KeypadModifier;

        // we couldn't find a specific mapping for the current modifiers,
        // or that mapping didn't have special modifiers:
        // so just report the plain mapping with additional modifiers.
        if ((it == map_plain && it != map_withmod) ||
            (map_withmod && !(map_withmod->qtcode & modmask))) {
            qtcode |= QInputmgrKeyboardEvent::toQtModifiers(modifiers);
        }

        if (m_composing == 2 && first_press && !(it->flags & QInputmgrKeyboardMap::IsModifier)) {
            // the last key press was the Compose key
            if (unicode != 0xffff) {
                int idx = 0;
                // check if this code is in the compose table at all
                for ( ; idx < m_keycompose_size; ++idx) {
                    if (m_keycompose[idx].first == unicode)
                        break;
                }
                if (idx < m_keycompose_size) {
                    // found it -> simulate a Dead key press
                    m_dead_unicode = unicode;
                    unicode = 0xffff;
                    m_composing = 1;
                    skip = true;
                } else {
                    m_composing = 0;
                }
            } else {
                m_composing = 0;
            }
        } else if (m_composing == 1 && first_press && !(it->flags & QInputmgrKeyboardMap::IsModifier)) {
            // the last key press was a Dead key
            bool valid = false;
            if (unicode != 0xffff) {
                int idx = 0;
                // check if this code is in the compose table at all
                for ( ; idx < m_keycompose_size; ++idx) {
                    if (m_keycompose[idx].first == m_dead_unicode && m_keycompose[idx].second == unicode)
                        break;
                }
                if (idx < m_keycompose_size) {
                    quint16 composed = m_keycompose[idx].result;
                    if (composed != 0xffff) {
                        unicode = composed;
                        qtcode = Qt::Key_unknown;
                        valid = true;
                    }
                }
            }
            if (!valid) {
                unicode = m_dead_unicode;
                qtcode = Qt::Key_unknown;
            }
            m_composing = 0;
        }

        if (!skip) {
//#ifdef QT_QPA_KEYMAP_DEBUG
            qWarning("Processing: uni=%04x, qt=%08x, qtmod=%08x", unicode, qtcode & ~modmask, (qtcode & modmask));
//#endif

            // send the result to the server
            m_listener.handleKeyboardEvent(keycode, unicode, qtcode & ~modmask, 
            					(qtcode & modmask), pressed, autorepeat);
            
        }
    }
    return result;
}

void QInputmgrKeyboardEvent::loadDefaultKeymap()
{
    m_keymap = s_keymap_default;
    m_keymap_size = sizeof(s_keymap_default) / sizeof(s_keymap_default[0]);
    m_keycompose = s_keycompose_default;
    m_keycompose_size = sizeof(s_keycompose_default) / sizeof(s_keycompose_default[0]);

    // reset state, so we could switch keymaps at runtime
    m_modifiers = 0;
    memset(m_locks, 0, sizeof(m_locks));
    m_composing = 0;
    m_dead_unicode = 0xffff;
}

QT_END_NAMESPACE
