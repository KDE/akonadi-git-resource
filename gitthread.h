#include <QThread>
#include <QString>
#include <QVector>
#include <QDateTime>

#ifndef AKONADI_GIT_THREAD_H_
#define AKONADI_GIT_THREAD_H_

class GitThread : public QThread {
  Q_OBJECT
public:

  enum ResultCode {
    ResultSuccess,
    ResultErrorOpeningRepository,
    ResultErrorCommitLookup,
    ResultErrorRevwalkPush,
    ResultErrorRevwalkNew,
    ResultErrorRepositoryHead,
    ResultThreadStillRunning
  };

  struct Commit {
    QString author;
    QByteArray message;
    QDateTime dateTime;
  };

  GitThread( const QString &path, QObject * parent = 0 );
  void run();

  QString lastErrorString() const;
  ResultCode lastErrorCode() const;
  QVector<Commit> commits() const;

private:
  QVector<Commit> m_commits;
  QString m_path;
  QString m_errorString;
  ResultCode m_resultCode;
};

#endif
