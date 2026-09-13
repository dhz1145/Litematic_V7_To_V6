#pragma once

#include <QMainWindow>
#include <QStringList>

class QListWidget;
class QListWidgetItem;
class QPlainTextEdit;
class QPushButton;
class QProgressBar;
class QLabel;
class QThread;
class ConvertWorker;

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow(QWidget *parent = nullptr);
	~MainWindow() override;

protected:
	void dragEnterEvent(QDragEnterEvent *event) override;
	void dropEvent(QDropEvent *event) override;

private slots:
	void onAddFiles();
	void onClear();
	void onConvert();
	void onOpenOutput();
	void onShowFailureDetail();
	void onItemDoubleClicked(QListWidgetItem *item);
	void onFileStarted(const QString &path, int index, int total);
	void onFileFinished(const QString &path, bool success, const QString &errorReason,
		const QString &detailLog, qint64 elapsedMs);
	void onAllFinished(int successCount, int failCount);

private:
	void appendLog(const QString &text);
	void addFiles(const QStringList &paths);
	void setBusy(bool busy);
	void applyStyle();
	QString failureReasonOf(QListWidgetItem *item) const;

	QListWidget *m_list = nullptr;
	QPlainTextEdit *m_log = nullptr;
	QPushButton *m_btnAdd = nullptr;
	QPushButton *m_btnClear = nullptr;
	QPushButton *m_btnConvert = nullptr;
	QPushButton *m_btnOpenOut = nullptr;
	QPushButton *m_btnDetail = nullptr;
	QProgressBar *m_progress = nullptr;
	QLabel *m_status = nullptr;

	QThread *m_thread = nullptr;
	ConvertWorker *m_worker = nullptr;
	QStringList m_files;
	QString m_lastOutputDir;
	bool m_busy = false;
};
