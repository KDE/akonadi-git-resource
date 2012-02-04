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

#include "gitthread.h"
#include "cheatingutils.h"

#include <KDE/KLocale>
#include <KProcess>

#include <QDir>
#include <QDebug>

#include <git2/oid.h>
#include <git2/errors.h>
#include <git2/commit.h>
#include <git2/revwalk.h>
#include <git2/refs.h>

static GitThread::Commit parseCommit( git_commit *wcommit )
{
  Q_ASSERT( wcommit );
  const char *cmsg = git_commit_message( wcommit );
  const git_signature *cauth = git_commit_author( wcommit );
  const git_time_t time = git_commit_time( wcommit );

  GitThread::Commit commit;
  commit.author   = QLatin1String( cauth->email );
  commit.message  = QByteArray( cmsg );
  commit.dateTime = QDateTime::fromMSecsSinceEpoch( time * 1000 );
  const git_oid *oid = git_commit_id( wcommit );
  char sha1[41];
  git_oid_fmt( sha1, oid );
  sha1[40] = '\0';
  commit.sha1 = QByteArray( sha1 );
  return commit;
}

GitThread::GitThread( const QString &path, TaskType type, const QString &sha1,
                      QObject *parent ) : QThread( parent )
                                        , m_path( path )
                                        , m_resultCode( ResultSuccess )
                                        , m_type( type )
                                        , m_sha1( sha1 )
{
  m_path = path + QLatin1String( "/.git/" );
  Q_ASSERT( !( type == GitThread::GetAllCommits && !sha1.isEmpty() ) );
}

bool GitThread::openRepository( git_repository **repository )
{
  if ( git_repository_open( repository, m_path.toUtf8() ) != GIT_SUCCESS ) {
    m_resultCode = ResultErrorOpeningRepository;
    m_errorString = "git_repository_open error";
    return false;
  }
  return true;
}

void GitThread::run()
{
  if ( m_type == GitThread::GetAllCommits )
    getAllCommits();
  else if ( m_type == GitThread::GetOneCommit )
    getOneCommit();
  else if ( m_type == GitThread::GetDiff )
    gitDiff();
  else
    Q_ASSERT( false );
}

void GitThread::getAllCommits()
{
  // First, do a git fetch
  gitFetch();
  emit gitFetchDone();

  if ( m_resultCode != ResultSuccess ) {
    m_resultCode = ResultSuccess;
    // Lets still do a normal sync, without the fetching...
    m_errorString.clear();
    // Not sure this ever happens though, git fetch should be successfull
  }

  git_repository *repository = 0;
  if ( !openRepository( &repository ) )
    return;

  git_revwalk *walk_this_way;
  if ( git_revwalk_new( &walk_this_way, repository ) != GIT_SUCCESS ) {
    m_resultCode = ResultErrorRevwalkNew;
    m_errorString = "git_revwalk_new error";
    git_repository_free( repository );
    return;
  }

  git_revwalk_sorting( walk_this_way, GIT_SORT_TOPOLOGICAL | GIT_SORT_REVERSE );

  //git_reference *head;
  const QByteArray remoteHeadSha1 = getRemoteHead( m_path );

  if ( remoteHeadSha1.isEmpty() ) {
    m_resultCode = ResultErrorInvalidHead;
    m_errorString = "Can't find head for origin/master";
    git_repository_free( repository );
    return;
  }
  /*
  if ( git_repository_head( &head, repository ) != GIT_SUCCESS ) {
    m_resultCode = ResultErrorRepositoryHead;
    m_errorString = "git_repository_head error";
    git_repository_free( repository );
    return;
  }
  const git_oid *head_oid = git_reference_oid( head );
  */

  git_oid head_oid;
  if ( git_oid_fromstr( &head_oid, remoteHeadSha1.data() ) != GIT_SUCCESS ) {
    m_resultCode = ResultErrorInvalidHead;
    m_errorString = "Can't find head for origin/master";
    git_repository_free( repository );
    return;
  }

  int error = 0;
  if ( ( error = git_revwalk_push( walk_this_way, &head_oid ) ) != GIT_SUCCESS ) {
    m_resultCode = ResultErrorRevwalkPush;
    m_errorString = "git_revwalk_push error: " + QString::number( error );
    git_repository_free( repository );
    return;
  }

  while( ( git_revwalk_next( &head_oid, walk_this_way ) ) == GIT_SUCCESS ) {
    git_commit *wcommit = 0;
    if ( git_commit_lookup( &wcommit, repository, &head_oid ) != GIT_SUCCESS ) {
      m_resultCode = ResultErrorCommitLookup;
      m_errorString = "git_commit_lookup error";
      git_repository_free( repository );
      return;
    }

    m_commits << parseCommit( wcommit );
    git_commit_close( wcommit );
  }

  git_revwalk_free( walk_this_way );
  git_repository_free( repository );
}

void GitThread::getOneCommit()
{
  if ( m_sha1.isEmpty() ) {
    m_resultCode = ResultNothingToFetch;
    m_errorString = i18n( "Error: Empty remote id" );
    return;
  }

  git_repository *repository = 0;
  if ( !openRepository( &repository ) )
    return;

  git_commit *wcommit = 0;
  git_oid oid;
  git_oid_fromstr( &oid, m_sha1.toUtf8().data() );
  if ( git_commit_lookup( &wcommit, repository, &oid ) != GIT_SUCCESS ) {
    m_resultCode = ResultErrorCommitLookup;
    m_errorString = "git_commit_lookup error";
    git_repository_free( repository );
    return;
  }

  m_commits << parseCommit( wcommit );

  git_commit_close( wcommit );
  git_repository_free( repository );
}


QString GitThread::lastErrorString() const
{
  if ( isFinished() ) // to avoid concurrency
    return m_errorString;

  return QString();
}

GitThread::ResultCode GitThread::lastErrorCode() const
{
  if ( isFinished() ) // to avoid concurrency
    return m_resultCode;

  return ResultThreadStillRunning;
}

QVector<GitThread::Commit> GitThread::commits() const
{
  return m_commits;
}

void GitThread::gitDiff()
{
  QProcess *process = new QProcess();
  QStringList args;
  process->setWorkingDirectory( m_path );
  process->start( QLatin1String( "git show " ) + m_sha1 );
  process->waitForFinished();
  m_diff = process->readAllStandardOutput();
  if ( process->exitCode() != 0 ) {
    m_resultCode = ResultErrorDiffing;
    m_errorString = i18n( "Error obtaining diff: %1", QString::number( process->exitCode() ) );
  } else {
    m_resultCode = ResultSuccess;
  }
  process->deleteLater();
}

QByteArray GitThread::diff() const
{
  return m_diff;
}

void GitThread::gitFetch()
{
  QProcess *process = new QProcess();
  process->setWorkingDirectory( m_path );
  process->start( QLatin1String( "git fetch origin" ) );
  process->waitForFinished();
  if ( process->exitCode() != 0 ) {
    m_resultCode = ResultErrorPulling;
    m_errorString = i18n( "Error doing git pull: %1", QString::number( process->exitCode() ) );
  } else {
    m_resultCode = ResultSuccess;
  }
  process->deleteLater();
}
