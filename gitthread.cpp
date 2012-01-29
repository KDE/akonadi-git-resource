#include "gitthread.h"

#include <KDE/KLocale>

#include <git2/oid.h>
#include <git2/errors.h>
#include <git2/commit.h>
#include <git2/revwalk.h>
#include <git2/refs.h>
#include <git2/repository.h>

GitThread::GitThread( const QString &path, QObject *parent ) : QThread( parent )
                                                             , m_path( path )
                                                             , m_resultCode( ResultSuccess )
{
  m_path = "/data/sources/kde/trunk/kde/kdepim/.git/"; // TODO
}

void GitThread::run()
{
  git_repository *repository = 0;
  int error = 0;

  if ( git_repository_open( &repository, m_path.toUtf8() ) != GIT_SUCCESS ) {
    m_resultCode = ResultErrorOpeningRepository;
    m_errorString = "git_repository_open error";
    // git_repository_free( repository );
    return;
  }

  git_reference *head;
  if ( git_repository_head( &head, repository ) != GIT_SUCCESS ) {
    m_resultCode = ResultErrorRepositoryHead;
    m_errorString = "git_repository_head error";
    git_repository_free( repository );
    return;
  }

  git_revwalk *walk_this_way;
  if ( git_revwalk_new( &walk_this_way, repository ) != GIT_SUCCESS ) {
    m_resultCode = ResultErrorRevwalkNew;
    m_errorString = "git_revwalk_new error";
    git_repository_free( repository );
    return;
  }

  git_revwalk_sorting( walk_this_way, GIT_SORT_TOPOLOGICAL | GIT_SORT_REVERSE );

  const git_oid *head_oid = git_reference_oid( head );
  if ( ( error = git_revwalk_push( walk_this_way, head_oid ) ) != GIT_SUCCESS ) {
    m_resultCode = ResultErrorRevwalkPush;
    m_errorString = "git_revwalk_push error: " + QString::number( error );
    git_repository_free( repository );
    return;
  }

  git_oid head_oid2 = *head_oid;
  while( ( git_revwalk_next( &head_oid2, walk_this_way ) ) == GIT_SUCCESS ) {
    git_commit *wcommit = 0;
    if ( git_commit_lookup( &wcommit, repository, &head_oid2 ) != GIT_SUCCESS ) {
      m_resultCode = ResultErrorCommitLookup;
      m_errorString = "git_commit_lookup error";
      git_repository_free( repository );
      return;
    }

    const char *cmsg = git_commit_message( wcommit );
    const git_signature *cauth = git_commit_author( wcommit );
    Commit commit;
    commit.author = QLatin1String( cauth->email );
    commit.message = QByteArray( cmsg );
    m_commits << commit;
    delete cmsg;
    delete cauth;
  }

  git_revwalk_free( walk_this_way );
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
