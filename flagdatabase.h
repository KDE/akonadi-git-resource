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

#ifndef FLAGDATABASE_H_
#define FLAGDATABASE_H_

#include <Akonadi/Item>
#include <QString>

class FlagDatabase {
public:
  FlagDatabase();
  ~FlagDatabase();

  bool insertFlag( const QString &sha1, const QString &flag );
  bool deleteFlag( const QString &sha1, const QString &flag );
  bool deleteFlags( const QString &sha1 );
  bool exists( const QString sha1, const QString &flag ) const;
  Akonadi::Item::Flags flags( const QString &sha1 ) const;

  bool clear();
private:
  class Private;
  Private *const d;
};

#endif
