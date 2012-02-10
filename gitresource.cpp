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
#include <Akonadi/CachePolicy>
#include <Akonadi/ChangeRecorder>
#include <akonadi/dbusconnectionpool.h>
#include <Akonadi/EntityDisplayAttribute>
#include <KMime/Message>
#include <KPIMIdentities/Identity>
#include <KPIMIdentities/IdentityManager>

#include <KCalCore/Event>
#include <KLocale>
#include <KWindowSystem>

#include <QFileInfo>
#include <QFileSystemWatcher>

using namespace Akonadi;

enum {
  IntervalCheckTime = 5 // minutes
};

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

  QString repositoryName() const;

  void setupWatcher();
  Akonadi::Item commitToItem( const GitThread::Commit &commit,
                              const QByteArray &diff = QByteArray() ) const;

  void updateResourceName();

  GitSettings *mSettings;
  GitThread   *m_thread;
  GitThread   *m_diffThread;
  QFileSystemWatcher *m_watcher;
  FlagDatabase *m_flagsDatabase;
  QByteArray m_currentHead;
private:
  GitResource *q;
};

void GitResource::Private::updateResourceName()
{
  const QString repName = repositoryName();
  q->setName( repositoryName().isEmpty() ? QLatin1String( "GitResource" ) : repName );
}

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

QString GitResource::Private::repositoryName() const
{
  QString repoPath = mSettings->repository();
  if ( repoPath.endsWith( '/') || repoPath.endsWith( '\\') )
    repoPath.chop( 1 );

  QFileInfo info( repoPath );
  return info.baseName();
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

  d->updateResourceName();
  d->m_currentHead = CheatingUtils::getRemoteHead( d->mSettings->repository() +
                                                   QLatin1String( "/.git/" ) );
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
    d->updateResourceName();
    d->setupWatcher();
    Collection collection;
    collection.setRemoteId( QLatin1String( "master" ) );
    invalidateCache( collection );
    synchronizeCollectionTree();
    synchronize();
  } else {
    emit configurationDialogRejected();
  }
}

void GitResource::retrieveCollections()
{
  Collection rootCollection;
  rootCollection.setName( d->repositoryName() );
  rootCollection.setContentMimeTypes( QStringList() << Akonadi::Collection::mimeType() );
  rootCollection.setRights( Collection::ReadOnly );
  rootCollection.setParentCollection( Akonadi::Collection::root() );
  rootCollection.setRemoteId( QLatin1String( "git_resource_root" ) );

  EntityDisplayAttribute *const evendDisplayAttribute = new EntityDisplayAttribute();
  evendDisplayAttribute->setIconName( "git" );
  rootCollection.addAttribute( evendDisplayAttribute );

  Collection master;
  master.setName( QLatin1String( "master" ) );
  master.setParentCollection( rootCollection );
  master.setRemoteId( QLatin1String( "master" ) ); // TODO: support more branches
  master.setContentMimeTypes( QStringList() << KMime::Message::mimeType()
                                            << Akonadi::Collection::mimeType() );
  master.setRights( Collection::ReadOnly );

  Akonadi::CachePolicy policy;
  policy.setIntervalCheckTime( IntervalCheckTime );
  policy.setInheritFromParent( false );
  policy.setSyncOnDemand( true );
  master.setCachePolicy( policy );

  collectionsRetrieved( Collection::List() << rootCollection << master );
}

void GitResource::retrieveItems( const Akonadi::Collection &collection )
{
  Q_UNUSED( collection );
  if ( collection.remoteId() == QLatin1String( "master" ) ) {
    if ( !d->m_thread ) {
      d->m_thread = new GitThread( d->mSettings, GitThread::GetAllCommits );
      connect( d->m_thread, SIGNAL(finished()), SLOT(handleGetAllFinished()) );
      connect( d->m_thread, SIGNAL(gitFetchDone()), SLOT(handleGitFetch()) );
      emit status( Running, i18n( "Retrieving items..." ) );
      d->m_watcher->blockSignals( true ); // We don't want signals during the git fetch
      d->m_thread->start();
    } else {
      cancelTask( i18n( "A retrieveItems() task is already running." ) );
    }
  } else {
    itemsRetrieved( Akonadi::Item::List() );
  }
}

bool GitResource::retrieveItem( const Item &item, const QSet<QByteArray> &parts )
{
  Q_UNUSED( parts );
  if ( !d->m_thread ) {
    d->m_thread = new GitThread( d->mSettings, GitThread::GetOneCommit, item.remoteId() );
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
  kDebug() << "GitResource::handleGetAllFinished()";
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
    d->m_thread = 0;
    itemsRetrieved( items ); // TODO: make it incremental?
  } else {
    d->m_thread = 0;
    cancelTask( i18n( "Error while doing retrieveItems(): %s ", d->m_thread->lastErrorString() ) );
  }
}

void GitResource::handleGetOneFinished()
{
  kDebug() << "GitResource::handleGetOneFinished()";
  emit status( Idle, i18n( "Ready" ) );
  if ( d->m_thread->lastErrorCode() == GitThread::ResultSuccess ) {
    Akonadi::Item item( d->m_thread->property( "item" ).value<Akonadi::Item>() );
    d->m_diffThread = new GitThread( d->mSettings, GitThread::GetDiff, item.remoteId() );
    connect( d->m_diffThread, SIGNAL(finished()), SLOT(handleGetDiffFinished()) );
    d->m_diffThread->start();
  } else {
    kError() << "GitResource::handleGetOneFinished() error: " << d->m_thread->lastErrorString()
             << d->m_thread->lastErrorCode();
    cancelTask( i18n( "Error while doing retrieveItem(): %s ", d->m_thread->lastErrorString() ) );
  }
}

void GitResource::handleGetDiffFinished()
{
  kDebug() << "GitResource::handleGetDiffFinished()";
  d->m_diffThread->deleteLater();
  d->m_thread->deleteLater();

  const QVector<GitThread::Commit> commits = d->m_thread->commits();
  Q_ASSERT( commits.count() == 1 );
  const GitThread::Commit commit = commits.first();
  const QString lastErrorString = d->m_diffThread->lastErrorString();
  const GitThread::ResultCode lastErrorCode = d->m_diffThread->lastErrorCode();
  Akonadi::Item item( d->m_thread->property( "item" ).value<Akonadi::Item>() );
  const QByteArray diff = d->m_diffThread->diff();

  d->m_thread = 0;
  d->m_diffThread = 0;

  if ( lastErrorCode == GitThread::ResultSuccess ) {
    Q_ASSERT( !diff.isEmpty() );
    item.setPayload<KMime::Message::Ptr>( d->commitToItem( commit,
                                                           diff ).payload<KMime::Message::Ptr>() );
    itemRetrieved( item );
  } else {
    kError() << "GitResource::handleGetDiffFinished()" << lastErrorString;
    cancelTask( lastErrorString );
  }
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
  const QByteArray newHead = CheatingUtils::getRemoteHead( d->mSettings->repository() +
                                                           QLatin1String( "/.git/" ) );
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
