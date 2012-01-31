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

#include "configdialog.h"
#include "settings.h"

#include <Akonadi/Collection>
#include <Akonadi/CollectionRequester>

#include <QDateTime>

ConfigDialog::ConfigDialog( GitSettings *settings, QWidget *parent) :
    KDialog( parent ), mSettings( settings )
{
  ui.setupUi( mainWidget() );

  ui.repository->setUrl( KUrl( mSettings->repository() ) );
  ui.from->setDateTime( mSettings->from() );
  ui.scripty->setChecked( mSettings->scripty() );

  connect( this, SIGNAL(okClicked()), this, SLOT(save()) );
  show();
}

void ConfigDialog::save()
{
  mSettings->setFrom( ui.from->dateTime() );
  mSettings->setScripty( ui.scripty->checkState() == Qt::Checked );
  mSettings->setRepository( ui.repository->url().path() );
  mSettings->writeConfig();
}

#include "configdialog.moc"
