#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class ConvertWorker : public QObject
{
	Q_OBJECT

public:
	explicit ConvertWorker(QObject *parent = nullptr);

public slots:
	void convertFiles(const QStringList &files);

signals:
	void fileStarted(const QString &path, int index, int total);
	// success 为 false 时，errorReason 为中文失败原因；detailLog 为完整日志
	void fileFinished(const QString &path, bool success, const QString &errorReason,
		const QString &detailLog, qint64 elapsedMs);
	void allFinished(int successCount, int failCount);

private:
	struct RunOutcome
	{
		bool ok = false;
		QString errorReason;
		QString detailLog;
		qint64 elapsedMs = 0;
	};

	static RunOutcome runOne(const QString &path);
};
