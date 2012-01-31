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

#include "flagdatabase.h"

#include <KStandardDirs>
#include <KDebug>

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QFile>

enum {
  Sha1Column = 0,
  FlagColumn
};

class FlagDatabase::Private
{
public:
  Private()
  {
    m_database = QSqlDatabase::addDatabase( "QSQLITE" );
    const QString filename = KStandardDirs::locateLocal( "data", "akonadi_git_resource/flags.db" );
    m_database.setDatabaseName( filename );
    if ( !QFile::exists( filename ) ) {
      createDB();
    } else {
      m_database.open(); //TODO error
    }
  }

  ~Private()
  {
    m_database.close();
  }

  void createDB();
  QSqlDatabase m_database;
};

void FlagDatabase::Private::createDB()
{
  //TODO better error control
  if ( m_database.open() ) {
    QSqlQuery query;
    if ( !query.exec( "create table flags "
                     "(sha1 varchar(40) primary key, flag varchar(10) ) " ) ) {
      kError() << "Error creating table. " << query.lastError();
    }
  } else {
    kError() << "Error opening seen database" << m_database.lastError();
  }
}

bool FlagDatabase::insertFlag( const QString &sha1, const QString &flag )
{
  QSqlQuery query;
  return query.exec( "insert into flags VALUES ('" + sha1 + "', '" + flag + "')" );
}

bool FlagDatabase::deleteFlag( const QString &sha1, const QString &flag )
{
  QSqlQuery query;
  return query.exec( "delete from flags where sha1 = '" + sha1 + "' and flag ='" + flag + "'" );
}

bool FlagDatabase::deleteFlags( const QString& sha1 )
{
  QSqlQuery query;
  return query.exec( "delete from flags where sha1 = '" + sha1 + "'" );
}

bool FlagDatabase::exists( const QString sha1, const QString &flag ) const
{
  QSqlQuery query;
  query.exec( "select 1 from flags where sha1 = '" + sha1 + "' and flag ='" + flag + "'" );
  return query.isValid();
}

bool FlagDatabase::clear()
{
  QSqlQuery query;
  return query.exec( "delete from flags" );
}

Akonadi::Item::Flags FlagDatabase::flags( const QString &sha1 ) const
{
  Akonadi::Item::Flags flags;
  QSqlQuery query( QString( "SELECT flag FROM flags WHERE sha1 = '%1'" ).arg( sha1 ) );
  while( query.next() ) {
    flags << query.value( int(FlagColumn) ).toString().toUtf8();
  }
  return flags;
}

FlagDatabase::FlagDatabase() : d( new Private() )
{
}


FlagDatabase::~FlagDatabase()
{
}
