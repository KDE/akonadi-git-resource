#include <QThread>
#include <QString>
#include <QVector>
#include <QDateTime>

#include <git2/repository.h>

#ifndef AKONADI_GIT_THREAD_H_
#define AKONADI_GIT_THREAD_H_

class GitThread : public QThread {
  Q_OBJECT
public:

  enum TaskType {
    GetAllCommits,
    GetOneCommit
  };

  enum ResultCode {
    ResultSuccess,
    ResultErrorOpeningRepository,
    ResultErrorCommitLookup,
    ResultErrorRevwalkPush,
    ResultErrorRevwalkNew,
    ResultErrorRepositoryHead,
    ResultThreadStillRunning,
    ResultNothingToFetch
  };

  struct Commit {
    QString author;
    QByteArray message;
    QDateTime dateTime;
    QString sha1;
  };

  GitThread( const QString &path,
             TaskType type,
             const QString &sha1 = QString(),
             QObject *parent = 0 );
  void run();

  QString lastErrorString() const;
  ResultCode lastErrorCode() const;
  QVector<Commit> commits() const;

private:
  bool openRepository( git_repository ** );
  void getAllCommits();
  void getOneCommit();

private:
  QVector<Commit> m_commits;
  QString m_path;
  QString m_errorString;
  ResultCode m_resultCode;
  TaskType m_type;
  QString m_sha1;
};

#endif
