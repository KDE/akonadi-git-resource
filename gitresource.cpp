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
                             , m_thread( 0 )
                             , m_diffThread( 0 )
                             , m_watcher( 0 )
                             , m_flagsDatabase( 0 )
                             , q( qq )
  {
    setupWatcher();
    m_flagsDatabase = new FlagDatabase( q->identifier() );
  }

  ~Private()
  {
    delete m_flagsDatabase;
  }

  void setupWatcher();
  Akonadi::Item commitToItem( const GitThread::Commit &commit,
                              const QByteArray &diff = QByteArray() ) const;

  GitSettings *mSettings;
  GitThread   *m_thread;
  GitThread   *m_diffThread;
  QFileSystemWatcher *m_watcher;
  FlagDatabase *m_flagsDatabase;
  QByteArray m_currentHead;
private:
  GitResource *q;
};

void GitResource::Private::setupWatcher()
{
  delete m_watcher;
  m_watcher = new QFileSystemWatcher( q );
  connect( m_watcher, SIGNAL(fileChanged(QString)), q, SLOT(handleRepositoryChanged()) );
  if ( !mSettings->repository().isEmpty() ) {
    m_watcher->addPath( mSettings->repository() + QLatin1String( "/.git/refs/remotes/origin/master" ) );
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

  item.setFlags( m_flagsDatabase->flags( commit.sha1 ) );
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
      d->m_flagsDatabase->clear();
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
  if ( !d->m_thread ) {
    d->m_thread = new GitThread( d->mSettings->repository(), GitThread::GetAllCommits );
    connect( d->m_thread, SIGNAL(finished()), SLOT(handleGetAllFinished()) );
    connect( d->m_thread, SIGNAL(gitFetchDone()), SLOT(handleGitFetch()) );
    emit status( Running, i18n( "Retrieving items..." ) );
    d->m_watcher->blockSignals( true ); // We don't want signals during the git fetch
    d->m_thread->start();
  } else {
    cancelTask( i18n( "A retrieveItems() task is already running." ) );
  }
}

bool GitResource::retrieveItem( const Item &item, const QSet<QByteArray> &parts )
{
  Q_UNUSED( parts );
  if ( !d->m_thread ) {
    d->m_thread = new GitThread( d->mSettings->repository(), GitThread::GetOneCommit,
                                item.remoteId() );
    connect( d->m_thread, SIGNAL(finished()), SLOT(handleGetOneFinished()) );
    emit status( Running, i18n( "Retrieving item..." ) );
    d->m_thread->setProperty( "item", QVariant::fromValue<Akonadi::Item>( item ) );
    d->m_thread->start();
    return true;
  } else {
    cancelTask( i18n( "A retrieveItem() task is already running." ) );
    return false;
  }
}

void GitResource::handleGetAllFinished()
{
  d->m_thread->deleteLater();
  emit status( Idle, i18n( "Ready" ) );
  if ( d->m_thread->lastErrorCode() == GitThread::ResultSuccess ) {
    Akonadi::Item::List items;
    const QVector<GitThread::Commit> commits = d->m_thread->commits();
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
    cancelTask( i18n( "Error while doing retrieveItems(): %s ", d->m_thread->lastErrorString() ) );
  }
  d->m_thread = 0;
}

void GitResource::handleGetOneFinished()
{
  emit status( Idle, i18n( "Ready" ) );
  if ( d->m_thread->lastErrorCode() == GitThread::ResultSuccess ) {
    Akonadi::Item item( d->m_thread->property( "item" ).value<Akonadi::Item>() );
    d->m_diffThread = new GitThread( d->mSettings->repository(), GitThread::GetDiff, item.remoteId() );
    connect( d->m_diffThread, SIGNAL(finished()), SLOT(handleGetDiffFinished()) );
    d->m_diffThread->start();
  } else {
    cancelTask( i18n( "Error while doing retrieveItem(): %s ", d->m_thread->lastErrorString() ) );
  }
}

void GitResource::handleGetDiffFinished()
{
  d->m_diffThread->deleteLater();
  d->m_thread->deleteLater();

  const QVector<GitThread::Commit> commits = d->m_thread->commits();
  Q_ASSERT( commits.count() == 1 );
  const GitThread::Commit commit = commits.first();

  if ( d->m_diffThread->lastErrorCode() == GitThread::ResultSuccess ) {
    Akonadi::Item item( d->m_thread->property( "item" ).value<Akonadi::Item>() );
    const QByteArray diff = d->m_diffThread->diff();
    Q_ASSERT( !diff.isEmpty() );
    item.setPayload<KMime::Message::Ptr>( d->commitToItem( commit,
                                                           diff ).payload<KMime::Message::Ptr>() );
    itemRetrieved( item );
  } else {
    kError() << "DEBUG " << d->m_diffThread->lastErrorString();
    cancelTask( d->m_diffThread->lastErrorString() );
  }

  d->m_thread = 0;
  d->m_diffThread = 0;
}

void GitResource::itemChanged( const Akonadi::Item &item, const QSet<QByteArray> &parts )
{
  Q_UNUSED( parts );
  const QString sha1 = item.remoteId();
  d->m_flagsDatabase->deleteFlags( sha1 );
  foreach( const QByteArray &flag, item.flags() ) {
    d->m_flagsDatabase->insertFlag( sha1, QString::fromUtf8( flag ) );
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

void GitResource::handleGitFetch()
{
  // Our own git fetch is done, re-enable so we listen to external changes
  d->m_watcher->blockSignals( false );
}



AKONADI_AGENT_FACTORY( GitResource, akonadi_git_resource )

#include "gitresource.moc"
