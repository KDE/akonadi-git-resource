#include "gitthread.h"

#include <KDE/KLocale>

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
  delete cmsg;
  delete cauth;

  return commit;
}

GitThread::GitThread( const QString &path, TaskType type, const QString &sha1,
                      QObject *parent ) : QThread( parent )
                                        , m_path( path )
                                        , m_resultCode( ResultSuccess )
                                        , m_type( type )
                                        , m_sha1( sha1 )
{
  m_path = "/data/sources/kde/trunk/kde/kdepim/.git/"; // TODO
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
  else
    Q_ASSERT( false );
}

void GitThread::getAllCommits()
{
  git_repository *repository = 0;
  if ( !openRepository( &repository ) )
    return;

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

  int error = 0;
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

    m_commits << parseCommit( wcommit );
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
