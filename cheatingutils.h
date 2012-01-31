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

#ifndef CHEATING_UTILS_H_
#define CHEATING_UTILS_H_

// Not using libgit is cheating

#include <QFile>
#include <QString>
#include <QByteArray>

// returns the SHA1 for origin/master
static QByteArray getRemoteHead( const QString repoPath )
{
  QByteArray sha1;
  QFile file( repoPath + QLatin1String( "/refs/remotes/origin/master" ) );
  if ( file.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
    sha1 = file.readLine().trimmed();
  }

  return sha1;
}

#endif
