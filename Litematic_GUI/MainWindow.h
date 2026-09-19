#pragma once

#include <QHash>
#include <QMainWindow>
#include <QStringList>
#include <QVector>

class QListWidget;
class QListWidgetItem;
class QPlainTextEdit;
class QPushButton;
class QProgressBar;
class QLabel;
class QThread;
class QKeyEvent;
class QCheckBox;
class QWidget;
class ConvertWorker;

// ItemList 物品 id 列表（json）；定义在 ItemAlpha.hpp
struct AlphaItemSet;

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow(QWidget *parent = nullptr);
	~MainWindow() override;

protected:
	void dragEnterEvent(QDragEnterEvent *event) override;
	void dropEvent(QDropEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;
	bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
	void onAddFiles();
	void onClear();
	void onConvert();
	void onOpenOutput();
	void onChooseOutputDir();
	void onResetOutputDir();
	void onLoadItemList();
	void onShowItemListDiff();
	void onToggleItemListPanel(bool on);
	void onShowFailureDetail();
	void onItemDoubleClicked(QListWidgetItem *item);
	void onListCustomContextMenu(const QPoint &pos);
	void onRemoveSelectedFiles();
	void onFileStarted(const QString &path, int index, int total);
	void onFileFinished(const QString &path, bool success, const QString &errorReason,
		const QString &detailLog, qint64 elapsedMs, const QString &outputPath);
	void onAllFinished(int successCount, int failCount);

private:
	void appendLog(const QString &text);
	void addFiles(const QStringList &paths);
	void setBusy(bool busy);
	void applyStyle();
	void updateActionButtons();
	void refreshOutputDirLabel();
	void refreshItemListLabel();
	bool loadItemListFromPath(const QString &path);
	void runItemListDiffOn(const QString &v6Path);
	void showItemListReplaceDialog();
	bool handleDroppedUrls(const QList<QUrl> &urls);

	QListWidget *m_list = nullptr;
	QWidget *m_itemListPanel = nullptr;
	QPlainTextEdit *m_log = nullptr;
	QPushButton *m_btnAdd = nullptr;
	QPushButton *m_btnClear = nullptr;
	QCheckBox *m_chkItemList = nullptr;
	QPushButton *m_btnConvert = nullptr;
	QPushButton *m_btnOpenOut = nullptr;
	QPushButton *m_btnDetail = nullptr;
	QPushButton *m_btnPickOut = nullptr;
	QPushButton *m_btnResetOut = nullptr;
	QPushButton *m_btnLoadItemList = nullptr;
	QPushButton *m_btnShowDiff = nullptr;
	QLabel *m_outDirLabel = nullptr;
	QLabel *m_itemListLabel = nullptr;
	QProgressBar *m_progress = nullptr;
	QLabel *m_status = nullptr;

	QThread *m_thread = nullptr;
	ConvertWorker *m_worker = nullptr;
	QStringList m_files;
	QString m_lastOutputDir;
	QString m_customOutputDir;
	QString m_lastV6Path;
	bool m_busy = false;

	AlphaItemSet *m_alpha = nullptr;
	QString m_itemListError;
	QString m_lastDiffV6Path;
	QString m_lastDiffSummary;
	QStringList m_lastDiffLines;
	QHash<QString, int> m_lastAllCounts;
	QHash<QString, int> m_lastPaletteCounts;
	QHash<QString, int> m_lastContainerCounts;
	QHash<QString, int> m_lastEntityCounts;
	QHash<QString, int> m_lastOtherCounts;
	int m_lastDiffTotalSchematicIds = 0;
	int m_lastDiffMissingCount = 0;
};