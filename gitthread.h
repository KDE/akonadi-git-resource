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

#ifndef AKONADI_GIT_THREAD_H_
#define AKONADI_GIT_THREAD_H_

#include <QThread>
#include <QString>
#include <QVector>
#include <QDateTime>

#include <git2/repository.h>


class GitThread : public QThread {
  Q_OBJECT
public:

  enum TaskType {
    GetAllCommits,
    GetOneCommit,
    GetDiff
  };

  enum ResultCode {
    ResultSuccess,
    ResultErrorOpeningRepository,
    ResultErrorCommitLookup,
    ResultErrorRevwalkPush,
    ResultErrorRevwalkNew,
    ResultErrorRepositoryHead,
    ResultThreadStillRunning,
    ResultNothingToFetch,
    ResultErrorDiffing,
    ResultErrorInvalidHead,
    ResultErrorPulling,
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
  QByteArray diff() const;
Q_SIGNALS:
  void gitFetchDone();

private:
  bool openRepository( git_repository ** );
  void getAllCommits();
  void getOneCommit();
  void gitDiff();
  void gitFetch();

private:
  QVector<Commit> m_commits;
  QByteArray m_diff;
  QString m_path;
  QString m_errorString;
  ResultCode m_resultCode;
  TaskType m_type;
  QString m_sha1;
};

#endif
