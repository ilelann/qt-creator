/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** Commercial Usage
**
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Commercial License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Nokia.
**
** GNU Lesser General Public License Usage
**
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** If you are unsure which license is appropriate for your use, please
** contact the sales department at http://qt.nokia.com/contact.
**
**************************************************************************/

#include "qmlprojectplugin.h"
#include "qmlprojectmanager.h"
#include "qmlprojectimportwizard.h"
#include "qmlprojectapplicationwizard.h"
#include "qmlstandaloneappwizard.h"
#include "qmlprojectconstants.h"
#include "qmlproject.h"
#include "qmlprojectrunconfigurationfactory.h"
#include "qmlprojectruncontrol.h"
#include "fileformat/qmlprojectfileformat.h"

#include <extensionsystem/pluginmanager.h>

#include <coreplugin/fileiconprovider.h>
#include <coreplugin/icore.h>
#include <coreplugin/mimedatabase.h>

#include <texteditor/texteditoractionhandler.h>

#include <projectexplorer/taskhub.h>

#include <QtCore/QtPlugin>

namespace QmlProjectManager {
namespace Internal {

QmlProjectPlugin::QmlProjectPlugin()
{ }

QmlProjectPlugin::~QmlProjectPlugin()
{
}

bool QmlProjectPlugin::initialize(const QStringList &, QString *errorMessage)
{
    using namespace Core;

    ICore *core = ICore::instance();
    Core::MimeDatabase *mimeDB = core->mimeDatabase();

    const QLatin1String mimetypesXml(":qmlproject/QmlProject.mimetypes.xml");

    if (! mimeDB->addMimeTypes(mimetypesXml, errorMessage))
        return false;

    Manager *manager = new Manager;

    addAutoReleasedObject(manager);
    addAutoReleasedObject(new Internal::QmlProjectRunConfigurationFactory);
    addAutoReleasedObject(new Internal::QmlRunControlFactory);
    addAutoReleasedObject(new QmlStandaloneAppWizard(QmlStandaloneAppWizard::NewQmlFile));
    addAutoReleasedObject(new QmlStandaloneAppWizard(QmlStandaloneAppWizard::ImportQmlFile));
    addAutoReleasedObject(new QmlProjectApplicationWizard);
    addAutoReleasedObject(new QmlProjectImportWizard);

    QmlProjectFileFormat::registerDeclarativeTypes();

    Core::FileIconProvider *iconProvider = Core::FileIconProvider::instance();
    iconProvider->registerIconOverlayForSuffix(QIcon(QLatin1String(":/qmlproject/images/qmlproject.png")), "qmlproject");
    return true;
}

void QmlProjectPlugin::extensionsInitialized()
{
}

} // namespace Internal
} // namespace QmlProjectManager

Q_EXPORT_PLUGIN(QmlProjectManager::Internal::QmlProjectPlugin)
