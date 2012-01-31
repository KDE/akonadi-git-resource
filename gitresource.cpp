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
#include "flagdatabase.h"
#include "cheatingutils.h"

#include <akonadi/agentfactory.h>
#include <Akonadi/ItemFetchScope>
#include <Akonadi/ChangeRecorder>
#include <akonadi/dbusconnectionpool.h>
#include <Akonadi/EntityDisplayAttribute>
#include <KMime/Message>
#include <KPIMIdentities/Identity>
#include <KPIMIdentities/IdentityManager>

#include <KCalCore/Event>
#include <KLocale>
#include <KWindowSystem>

#include <QFileSystemWatcher>

using namespace Akonadi;

class GitResource::Private {
public:
  Private( GitResource *qq ) : mSettings( new GitSettings( componentData().config() ) )
                             , _thread( 0 )
                             , _diffThread( 0 )
                             , _watcher()
                             , q( qq )
  {
    setupWatcher();
  }

  void setupWatcher();
  Akonadi::Item commitToItem( const GitThread::Commit &commit,
                              const QByteArray &diff = QByteArray() ) const;

  GitSettings *mSettings;
  GitThread   *_thread;
  GitThread   *_diffThread;
  QFileSystemWatcher *_watcher;
  FlagDatabase _flagsDatabase;
  QByteArray m_currentHead;
private:
  GitResource *q;
};

void GitResource::Private::setupWatcher()
{
  delete _watcher;
  _watcher = new QFileSystemWatcher( q );
  connect( _watcher, SIGNAL(fileChanged(QString)), q, SLOT(handleRepositoryChanged()) );
  if ( !mSettings->repository().isEmpty() ) {
    _watcher->addPath( mSettings->repository() + QLatin1String( "/.git/refs/remotes/origin/master" ) );
  }
}

Akonadi::Item GitResource::Private::commitToItem( const GitThread::Commit &commit,
                                                  const QByteArray &body ) const
{
  Item item;
  item.setMimeType( KMime::Message::mimeType() );

  KMime::Message *message = new KMime::Message();
  KMime::Headers::ContentType *ct = message->contentType();
  ct->setMimeType( "text/plain" );
  message->contentTransferEncoding()->clear();
  const QByteArray firstLine = commit.message.split('\n').first();

  message->subject()->fromUnicodeString( firstLine, "utf-8" );
  message->from()->fromUnicodeString( commit.author, "utf-8" );
  message->to()->fromUnicodeString( mSettings->identity(), "utf-8" );
  // message->cc()->fromUnicodeString( "some@mailaddy.com", "utf-8" ); // parse CCMAIL:
  message->date()->setDateTime( KDateTime( commit.dateTime ) );
  item.setPayload( KMime::Message::Ptr( message ) );

  message->contentType()->setMimeType( "text/plain" );

  if ( !body.isEmpty() ) {
    message->setBody( body );
  }

  item.setRemoteId( commit.sha1 );
  message->assemble();

  item.setFlags( _flagsDatabase.flags( commit.sha1 ) );
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
  if ( !d->mSettings->from().isValid() ) {
    d->mSettings->setFrom( QDateTime::currentDateTime().addDays( -30 ) );
    d->mSettings->writeConfig();
  }

  if ( d->mSettings->identity().isEmpty() ) {
    KPIMIdentities::IdentityManager identManager;
    const KPIMIdentities::Identity identity = identManager.defaultIdentity();
    d->mSettings->setIdentity( identity.fullEmailAddr() );
    d->mSettings->writeConfig();
  }

  d->m_currentHead = getRemoteHead( d->mSettings->repository() + QLatin1String( "/.git/" ) );
}

GitResource::~GitResource()
{
  delete d;
}

void GitResource::configure( WId windowId )
{
  // TODO clear the db when the repo changes
  ConfigDialog dlg( d->mSettings );
  if ( windowId )
    KWindowSystem::setMainWindow( &dlg, windowId );

  const QString oldRepo = d->mSettings->repository();
  if ( dlg.exec() ) {
    emit configurationDialogAccepted();

    if ( d->mSettings->repository() != oldRepo ) {
      d->_flagsDatabase.clear();
    }

    d->setupWatcher();
    Collection collection;
    collection.setRemoteId( QLatin1String( "master" ) );
    invalidateCache( collection );
    synchronize();
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
  Q_UNUSED( collection );
  if ( !d->_thread ) {
    d->_thread = new GitThread( d->mSettings->repository(), GitThread::GetAllCommits );
    connect( d->_thread, SIGNAL(finished()), SLOT(handleGetAllFinished()) );
    emit status( Running, i18n( "Retrieving items..." ) );
    d->_thread->start();
  } else {
    cancelTask( i18n( "A retrieveItems() task is already running." ) );
  }
}

bool GitResource::retrieveItem( const Item &item, const QSet<QByteArray> &parts )
{
  Q_UNUSED( parts );
  if ( !d->_thread ) {
    d->_thread = new GitThread( d->mSettings->repository(), GitThread::GetOneCommit,
                                item.remoteId() );
    connect( d->_thread, SIGNAL(finished()), SLOT(handleGetOneFinished()) );
    emit status( Running, i18n( "Retrieving item..." ) );
    d->_thread->setProperty( "item", QVariant::fromValue<Akonadi::Item>( item ) );
    d->_thread->start();
    return true;
  } else {
    cancelTask( i18n( "A retrieveItem() task is already running." ) );
    return false;
  }
}

void GitResource::handleGetAllFinished()
{
  d->_thread->deleteLater();
  emit status( Idle, i18n( "Ready" ) );
  if ( d->_thread->lastErrorCode() == GitThread::ResultSuccess ) {
    Akonadi::Item::List items;
    const QVector<GitThread::Commit> commits = d->_thread->commits();
    const QDateTime currentDateTime = QDateTime::currentDateTime();
    foreach( const GitThread::Commit &commit, commits ) {
      const bool fromScripty = commit.author == QLatin1String( "scripty@kde.org" );
      if ( commit.dateTime.date() >= d->mSettings->from().date() &&
           !( fromScripty && !d->mSettings->scripty() ) ) {
        items << d->commitToItem( commit );
      }
    }
    itemsRetrieved( items ); // TODO: make it incremental?
  } else {
    cancelTask( i18n( "Error while doing retrieveItems(): %s ", d->_thread->lastErrorString() ) );
  }
  d->_thread = 0;
}

void GitResource::handleGetOneFinished()
{
  emit status( Idle, i18n( "Ready" ) );
  if ( d->_thread->lastErrorCode() == GitThread::ResultSuccess ) {
    Akonadi::Item item( d->_thread->property( "item" ).value<Akonadi::Item>() );
    d->_diffThread = new GitThread( d->mSettings->repository(), GitThread::GetDiff, item.remoteId() );
    connect( d->_diffThread, SIGNAL(finished()), SLOT(handleGetDiffFinished()) );
    d->_diffThread->start();
  } else {
    cancelTask( i18n( "Error while doing retrieveItem(): %s ", d->_thread->lastErrorString() ) );
  }
}

void GitResource::handleGetDiffFinished()
{
  d->_diffThread->deleteLater();
  d->_thread->deleteLater();

  const QVector<GitThread::Commit> commits = d->_thread->commits();
  Q_ASSERT( commits.count() == 1 );
  const GitThread::Commit commit = commits.first();

  if ( d->_diffThread->lastErrorCode() == GitThread::ResultSuccess ) {
    Akonadi::Item item( d->_thread->property( "item" ).value<Akonadi::Item>() );
    const QByteArray diff = d->_diffThread->diff();
    Q_ASSERT( !diff.isEmpty() );
    item.setPayload<KMime::Message::Ptr>( d->commitToItem( commit,
                                                           diff ).payload<KMime::Message::Ptr>() );
    itemRetrieved( item );
  } else {
    kError() << "DEBUG " << d->_diffThread->lastErrorString();
    cancelTask( d->_diffThread->lastErrorString() );
  }

  d->_thread = 0;
  d->_diffThread = 0;
}

void GitResource::itemChanged( const Akonadi::Item &item, const QSet<QByteArray> &parts )
{
  Q_UNUSED( parts );
  const QString sha1 = item.remoteId();
  d->_flagsDatabase.deleteFlags( sha1 );
  foreach( const QByteArray &flag, item.flags() ) {
    d->_flagsDatabase.insertFlag( sha1, QString::fromUtf8( flag ) );
  }
  // TODO: error handling
  changeCommitted( item );
}

void GitResource::handleRepositoryChanged()
{
  const QByteArray newHead = getRemoteHead( d->mSettings->repository() + QLatin1String( "/.git/" ) );
  if ( newHead != d->m_currentHead && !newHead.isEmpty() ) {
    d->m_currentHead = newHead;
    Collection collection;
    collection.setRemoteId( QLatin1String( "master" ) );
    invalidateCache( collection );
    synchronize();
  }
}


AKONADI_AGENT_FACTORY( GitResource, akonadi_git_resource )

#include "gitresource.moc"
