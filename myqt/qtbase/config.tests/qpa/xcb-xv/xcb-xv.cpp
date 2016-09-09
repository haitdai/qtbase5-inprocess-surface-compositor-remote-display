/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the config.tests of the Qt Toolkit.
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

#include <xcb/xcb.h>
#include <xcb/xv.h>

int main(int, char **)
{
    int primaryScreen = 0;
    xcb_connection_t *connection = xcb_connect("", &primaryScreen);

    int width = 100;
    int height = 100;    
    xcb_window_t root = {0};
    xcb_xv_query_adaptors_reply_t *adaptors = xcb_xv_query_adaptors_reply(connection, xcb_xv_query_adaptors(connection, root), NULL);
    
    xcb_xv_adaptor_info_iterator_t it;
    for (it = xcb_xv_query_adaptors_info_iterator(adaptors); it.rem > 0; xcb_xv_adaptor_info_next(&it)) {  
        const xcb_xv_adaptor_info_t *adaptor = it.data;        
        xcb_xv_list_image_formats_reply_t *list = xcb_xv_list_image_formats_reply(connection, xcb_xv_list_image_formats (connection, adaptor->base_id), NULL);
        for (const xcb_xv_image_format_info_t *f = xcb_xv_list_image_formats_format(list), *f_end = f + xcb_xv_list_image_formats_format_length(list); f < f_end; f++) {
            xcb_xv_query_image_attributes_reply(connection, xcb_xv_query_image_attributes(connection, adaptor->base_id, f->id, width, height), NULL);
        }
    }

    return 0;
}
