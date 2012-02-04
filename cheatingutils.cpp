/*
    Copyright (c) 2012 Sérgio Martins <iamsergio@gmail.com>

    This library is free software; you can redistribute it and/or modify it
    under the terms of the GNU Library General Public License as published by
    the Free Software Foundation; either version 2 of the License, or (at your
    option) any later version.

    This library is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
    License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to the
    Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301, USA.
*/

#include "cheatingutils.h"

#include <KLocale>

#include <QFile>
#include <QProcess>
#include <QByteArray>

QByteArray CheatingUtils::getRemoteHead( const QString repoPath )
{
  QByteArray sha1;
  QFile file( repoPath + QLatin1String( "/refs/remotes/origin/master" ) );
  if ( file.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
    sha1 = file.readLine().trimmed();
  }

  return sha1;
}

bool CheatingUtils::gitFetch( const QString &path, QString *out_errorMessage )
{
  QProcess *process = new QProcess();
  process->setWorkingDirectory( path );
  process->start( QLatin1String( "git fetch origin" ) );
  process->waitForFinished();
  bool result = true;
  if ( process->exitCode() != 0 ) {
    result = false;
    *out_errorMessage = i18n( "Error doing git fetch: %1", QString::number( process->exitCode() ) );
  }
  process->deleteLater();
  return result;
}

bool CheatingUtils::gitDiff( const QString &path, const QString &sha1,
                             QByteArray *out_diff, QString *out_errorMessage )
{
  QProcess *process = new QProcess();
  QStringList args;
  process->setWorkingDirectory( path );
  process->start( QLatin1String( "git show " ) + sha1 );
  process->waitForFinished();
  *out_diff = process->readAllStandardOutput();
  bool result = true;
  if ( process->exitCode() != 0 ) {
    result = true;
    *out_errorMessage = i18n( "Error obtaining diff: %1", QString::number( process->exitCode() ) );
  }
  process->deleteLater();
  return result;
}