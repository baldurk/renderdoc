/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//
// Despite its file name, this really is not a public header.
// It is an implementation detail of the private bootstrap library.
//

#if 0
// silence syncqt warnings
#pragma qt_sync_skip_header_check
#pragma qt_sync_stop_processing
#endif

#ifdef QT_BOOTSTRAPPED

#ifndef QT_NO_EXCEPTIONS
#define QT_NO_EXCEPTIONS
#endif

#define QT_NO_USING_NAMESPACE
#define QT_NO_DEPRECATED

// Keep feature-test macros in alphabetic order by feature name:
#define QT_FEATURE_alloca 1
#define QT_FEATURE_alloca_h -1
#ifdef _WIN32
# define QT_FEATURE_alloca_malloc_h 1
#else
# define QT_FEATURE_alloca_malloc_h -1
#endif
#define QT_CRYPTOGRAPHICHASH_ONLY_SHA1
#define QT_NO_DATASTREAM
#define QT_FEATURE_datetimeparser -1
#define QT_NO_GEOM_VARIANT
#define QT_FEATURE_iconv -1
#define QT_FEATURE_icu -1
#define QT_FEATURE_journald -1
#define QT_FEATURE_library -1
#define QT_NO_QOBJECT
#define QT_FEATURE_process -1
#define QT_FEATURE_sharedmemory -1
#define QT_FEATURE_slog2 -1
#define QT_FEATURE_syslog -1
#define QT_NO_SYSTEMLOCALE
#define QT_FEATURE_systemsemaphore -1
#define QT_FEATURE_temporaryfile 1
#define QT_NO_THREAD
#define QT_FEATURE_timezone -1
#define QT_FEATURE_topleveldomain -1
#define QT_NO_TRANSLATION
#define QT_FEATURE_translation -1

#ifdef QT_BUILD_QMAKE
#define QT_FEATURE_commandlineparser -1
#define QT_NO_COMPRESS
#define QT_JSON_READONLY
#define QT_NO_STANDARDPATHS
#define QT_NO_TEXTCODEC
#define QT_FEATURE_textcodec -1
#else
#define QT_NO_CODECS
#define QT_FEATURE_codecs -1
#define QT_FEATURE_commandlineparser 1
#define QT_FEATURE_textcodec 1
#endif

#endif // QT_BOOTSTRAPPED
