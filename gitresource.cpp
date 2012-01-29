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

#include "gitresource.h"

#include "settings.h"
#include "settingsadaptor.h"
#include "configdialog.h"
#include "gitthread.h"

#include <akonadi/agentfactory.h>
#include <Akonadi/ItemFetchScope>
#include <Akonadi/ChangeRecorder>
#include <akonadi/dbusconnectionpool.h>
#include <Akonadi/EntityDisplayAttribute>
#include <KMime/Message>

#include <KCalCore/Event>
#include <KLocale>
#include <KWindowSystem>

using namespace Akonadi;

class GitResource::Private {
public:
  Private( GitResource *qq ) : mSettings( new GitSettings( componentData().config() ) )
                             , _thread( 0 )
                             , q( qq )
  {
  }

  Akonadi::Item commitToItem( const GitThread::Commit &commit ) const;

  GitSettings *mSettings;
  GitThread   *_thread;
private:
  GitResource *q;
};

Akonadi::Item GitResource::Private::commitToItem( const GitThread::Commit &commit ) const
{
  Item item;
  item.setMimeType( KMime::Message::mimeType() );

  KMime::Message *message = new KMime::Message();
  message->from()->fromUnicodeString( commit.author, "utf-8" );
  message->to()->fromUnicodeString( "iamsergio@gmail.com", "utf-8" ); // TODO: TO
  // message->cc()->fromUnicodeString( "some@mailaddy.com", "utf-8" ); // parse CCMAIL:
  //message->date()->setDateTime( KDateTime::currentLocalDateTime() ); // TODO: COMMIT date
  message->subject()->fromUnicodeString( "My Subject", "utf-8" );
  item.setPayload( KMime::Message::Ptr( message ) );

  KMime::Content *body = new KMime::Content();
  body->contentType()->setMimeType( "text/plain" );
  body->setBody( commit.message );
  message->addContent( body );

  return item;
}

GitResource::GitResource( const QString &id )
  : ResourceBase( id ), d( new Private( this ) )
{
  setName( QLatin1String( "Git Resource" ) );

  changeRecorder()->itemFetchScope().fetchFullPayload();
  changeRecorder()->fetchCollection( true );

  new SettingsAdaptor( d->mSettings );
  DBusConnectionPool::threadConnection().registerObject( QLatin1String( "/Settings" ),
                                                         d->mSettings,
                                                         QDBusConnection::ExportAdaptors );
  //connect( this, SIGNAL(reloadConfiguration()), SLOT(load()) );
  //load();
}

GitResource::~GitResource()
{
  delete d;
}

void GitResource::configure( WId windowId )
{
  ConfigDialog dlg( d->mSettings );
  if ( windowId )
    KWindowSystem::setMainWindow( &dlg, windowId );

  if ( dlg.exec() ) {
    emit configurationDialogAccepted();
  } else {
    emit configurationDialogRejected();
  }
}

void GitResource::retrieveCollections()
{
  Collection collection;
  collection.setRemoteId( QLatin1String( "master" ) ); // TODO: support more branches
  collection.setName( QLatin1String( "master" ) );
  collection.setParentCollection( Akonadi::Collection::root() );
  collection.setContentMimeTypes( QStringList() << KMime::Message::mimeType()
                                                << Akonadi::Collection::mimeType() );
  collection.setRights( Collection::ReadOnly );
  collectionsRetrieved( Collection::List() << collection );
}

void GitResource::retrieveItems( const Akonadi::Collection &collection )
{
  if ( !d->_thread ) {
    d->_thread = new GitThread( d->mSettings->repository() );
    connect( d->_thread, SIGNAL(finished()), SLOT(handleThreadFinished()) );
    emit status( Running, i18n( "Retrieving items..." ) );
    d->_thread->start();
  } else {
    cancelTask( i18n( "A retrieveItems() task is already running." ) );
  }
}

bool GitResource::retrieveItem( const Item &item, const QSet<QByteArray> &parts )
{
  return true;
}

void GitResource::handleThreadFinished()
{
  d->_thread->deleteLater();
  emit status( Idle, i18n( "Ready" ) );
  if ( d->_thread->lastErrorCode() == GitThread::ResultSuccess ) {
    Akonadi::Item::List items;
    const QVector<GitThread::Commit> commits = d->_thread->commits();
    foreach( const GitThread::Commit &commit, commits ) {
      items << d->commitToItem( commit );
    }
    itemsRetrieved( items ); // TODO: make it incremental?
  } else {
    cancelTask( i18n( "Error while doing retrieveItems(): %s ", d->_thread->lastErrorString() ) );
  }
  d->_thread = 0;
}

AKONADI_AGENT_FACTORY( GitResource, akonadi_git_resource )

#include "gitresource.moc"
