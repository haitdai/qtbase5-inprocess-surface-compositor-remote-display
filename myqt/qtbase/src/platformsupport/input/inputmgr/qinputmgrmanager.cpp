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

#include "qinputmgrmanager_p.h"
#include <qpa/qwindowsysteminterface.h>
#include <QDebug>
#include <qguiapplication.h>
#include <qcursor.h>
#include <qnamespace.h>
#include "InputMgr.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>


QT_BEGIN_NAMESPACE

QInputmgrManager::QInputmgrManager(const QString &key, const QString &specification)
    : m_inputFd(0),m_x(0),m_y(0),m_mouseEvt(NULL),m_keyEvt(NULL), m_showCursor(true)
{
	int index = SCREEN_MAJOR;
	qWarning("QInputmgrManager create");
	qWarning()<<"index is :"<<specification;
	if (!specification.isEmpty()){
		index = specification.toInt();
		if (index < SCREEN_MAJOR || index > SCREEN_MINOR){
			index = SCREEN_MAJOR;
		}
	}
	m_inputFd = InputSysInit(index);
	if (m_inputFd <= 0){
		qWarning("InputSysInit error\n");
		return;
	}
	int flags = 0;
	flags = fcntl(m_inputFd,F_GETFL);
	if (flags & O_NONBLOCK)
	{
		return ;
	}
	flags |= O_NONBLOCK;
	fcntl(m_inputFd,F_SETFL,flags);
	m_mouseEvt = new QInputmgrMouseEvent(*this);
	m_keyEvt = new QInputmgrKeyboardEvent(*this);
	m_mouseMoveElapsedTimer.start();
	m_mouseLeftKeyPressed = false;
}

QInputmgrManager::~QInputmgrManager()
{
	if (m_inputFd > 0){
		InputSysClose(m_inputFd);
		m_inputFd = 0;
	}
	if (m_keyEvt){
		delete m_keyEvt;
		m_keyEvt = NULL;
	}
	if (m_mouseEvt){
		delete m_mouseEvt;
		m_mouseEvt = NULL;
	}
}

void QInputmgrManager::handleKeyLockEvt(int key, int state)
{
	if (key < EVLOCK_CAPS 
		|| key > EVLOCK_SCROLL){
		return ;
	}
	m_keyEvt->handleLockEvent(key,state == KEY_LOCKED ? 1 : 0);
}

void QInputmgrManager::readInputEvent()
{
	struct ::input_event buffer[20];
    int n = 0;
    bool posChanged = false, btnChanged = false;
    bool pendingMouseEvent = false;
    int eventCompressCount = 0;
	int result = 0;
	
    forever {
		result = InputReadEvent(m_inputFd, buffer, sizeof(buffer)/sizeof(struct ::input_event));
		if (0 == result) {
			break;
		}
		else if (0 > result) {
			qWarning("Could not read from input device: %s", strerror(errno));
			break;
		}
        else {//(0 < result)
        	for (int i = 0; i<result; i++){
				struct ::input_event *data = &buffer[i];
				switch(data->type){
					case EV_ABS:
					case EV_REL:
					case EV_SYN:
						m_mouseEvt->handleMouseEvent(data);
						break;
					case EV_KEY:
						if (data->code == BTN_MOUSE
							|| data->code == BTN_LEFT
							|| data->code == BTN_RIGHT
							|| data->code == BTN_MIDDLE
							|| data->code == BTN_TOUCH){
							m_mouseEvt->handleMouseEvent(data);
						}
						else{
							m_keyEvt->handleKeyEvent(data);
						}
						break;
					case EV_MOVE:
						if (data->code == MOVE_IN){
							QGuiApplication::setOverrideCursor(QCursor(Qt::ArrowCursor));
							m_showCursor = true;
						}
						else{
							QGuiApplication::setOverrideCursor(QCursor(Qt::BlankCursor));
							m_showCursor = false;
						}
						break;
					case EV_FOCUS:
						break;
					case EV_KEYLOCK:
						handleKeyLockEvt(data->code,data->value);
						break;
					case EV_SCAN:
						break;
					case EV_CURSOR:
						if (data->code == CURSOR_HIDE){
							QGuiApplication::setOverrideCursor(QCursor(Qt::BlankCursor));
							m_showCursor = false;
						}
						else{
							QGuiApplication::setOverrideCursor(QCursor(Qt::ArrowCursor));
							m_showCursor = true;
						}
						break;
					default:
						break;
				}
			}
        	if(result < sizeof(buffer)/sizeof(struct ::input_event)){
				break;
			}
        }
    }
}
void QInputmgrManager::handleMouseEvent(int x, int y, Qt::MouseButtons buttons)
{
	QPoint pos(x, y);
	m_x = x;
	m_y = y;
#ifdef INPUT_MGR_ARM
	bool isLeftKeyStatChanged = false;
	if ((Qt::LeftButton)&buttons)
	{
		if (!m_mouseLeftKeyPressed)
		{
			m_mouseLeftKeyPressed = true;
			isLeftKeyStatChanged = true;
		}
	}
	else
	{
		if (m_mouseLeftKeyPressed)
		{
			m_mouseLeftKeyPressed = false;
			isLeftKeyStatChanged = true;
		}
	}
	if (!isLeftKeyStatChanged)
	{
		if (40 > m_mouseMoveElapsedTimer.elapsed())
		{
			return;
		}
	}
	m_mouseMoveElapsedTimer.restart();
#endif //INPUT_MGR_ARM

    if(m_showCursor) {
        QWindowSystemInterface::handleMouseCursorMoveEvent(0, pos, pos, buttons);    // to compositor thread
    }
    QWindowSystemInterface::handleMouseEvent(0, pos, pos, buttons);                     // to ui thread
}

void QInputmgrManager::handleWheelEvent(int delta, Qt::Orientation orientation)
{
	QPoint pos(m_x, m_y);
    QWindowSystemInterface::handleWheelEvent(0, pos, pos, delta, orientation);
}

void QInputmgrManager::handleKeyboardEvent(int nativecode, int unicode, int qtcode,int imodifiers, 
			                                    	bool isPress, bool autoRepeat)
{
	QWindowSystemInterface::handleExtendedKeyEvent(0, static_cast<QEvent::Type>(isPress ? 6 : 7),
           	qtcode, Qt::KeyboardModifiers(imodifiers), nativecode + 8, 0, imodifiers,
           	(unicode != 0xffff ) ? QString(unicode) : QString(), autoRepeat);
}

void QInputmgrManager::handleSwitchLed(int led, bool state)
{
	KeyboardLedCtrl(m_inputFd,led,state);
}

bool QInputmgrManager::selectInputFd()
{
	if (0 == m_inputFd)
	{
		return false;
	}
	fd_set select_fds;
	timeval tmOut;
	tmOut.tv_sec = 1;
	tmOut.tv_usec = 0;
	FD_ZERO(&select_fds);
	FD_SET(m_inputFd, &select_fds);
	const int ret = select(m_inputFd+1, &select_fds, NULL, NULL, &tmOut);
	if (0 < ret)
	{
		readInputEvent();
	}
	else if (0 == ret)
	{
		//DO NOTHING!
	}
	else
	{
		qWarning("Input device error: %s", strerror(errno));
		//Fixme:errno´íÎó´¦Àí!
	}
	return true;
}

void QInputmgrManager::closeInputFd()
{
	if (m_inputFd > 0){
		InputSysClose(m_inputFd);
		m_inputFd = 0;
	}
}

QInputmgrThread::QInputmgrThread(const QString &key, const QString &specification, QObject *parent)
    : QThread(parent), m_key(key), m_spec(specification), m_mgr(NULL)
{
    start();
}

QInputmgrThread::~QInputmgrThread()
{
	if (NULL != m_mgr)
	{
		m_mgr->closeInputFd();
		//QInputmgrThread::run()Àïdelete m_mgr!
	}
    quit();
    wait();
}

void QInputmgrThread::run()
{
	m_mgr = new QInputmgrManager(m_key, m_spec);
	if (NULL != m_mgr)
	{
		while (m_mgr->selectInputFd())
		{
			//DO NOTHING!
		}
	    delete m_mgr;
	    m_mgr = NULL;
	}
}

QT_END_NAMESPACE
