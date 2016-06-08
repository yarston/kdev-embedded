/*
 * This file is part of KDevelop project
 * Copyright 2016 Patrick José Pereira <patrickelectric@gmail.com>
 * Based onde the work of:
 *  Andreas Pakulat <apaku@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

#include <QtCore/QString>

namespace KDevelop
{
class ILaunchConfiguration;
}

class KJob;
class QUrl;
class QStringList;

/**
@author Andreas Pakulat
*/

class IExecutePlugin
{
public:
    virtual ~IExecutePlugin() {}

    virtual QUrl executable(KDevelop::ILaunchConfiguration* config, QString& error) const = 0;
    virtual QStringList arguments(KDevelop::ILaunchConfiguration* config, QString& error) const = 0;
    virtual QUrl workingDirectory(KDevelop::ILaunchConfiguration* config) const = 0;
    virtual KJob* dependencyJob(KDevelop::ILaunchConfiguration* config) const = 0;
    virtual QString environmentGroup(KDevelop::ILaunchConfiguration* config) const = 0;
    virtual bool useTerminal(KDevelop::ILaunchConfiguration* config) const = 0;
    virtual QString terminal(KDevelop::ILaunchConfiguration* config) const = 0;
    virtual QString nativeAppConfigTypeId() const = 0;
};

Q_DECLARE_INTERFACE(IExecutePlugin, "org.kdevelop.IExecutePlugin")
