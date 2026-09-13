#include "MainWindow.h"
#include "ConvertWorker.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QDesktopServices>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QMimeData>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QThread>
#include <QUrl>
#include <QVBoxLayout>
#include <QStyle>

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
{
	setWindowTitle(QStringLiteral("Litematic V7 → V6 转换器"));
	setMinimumSize(720, 520);
	resize(860, 600);
	setAcceptDrops(true);

	auto *central = new QWidget(this);
	setCentralWidget(central);

	auto *root = new QVBoxLayout(central);
	root->setContentsMargins(16, 16, 16, 16);
	root->setSpacing(12);

	auto *title = new QLabel(QStringLiteral("投影降级 · V7 (1.20.5+) → V6 (1.20.4-)"), central);
	title->setObjectName("titleLabel");
	auto *hint = new QLabel(QStringLiteral("拖入 .litematic 文件，或点击「添加文件」。输出保存在原文件同目录，不会覆盖源文件。"), central);
	hint->setObjectName("hintLabel");
	hint->setWordWrap(true);

	auto *header = new QVBoxLayout();
	header->setSpacing(4);
	header->addWidget(title);
	header->addWidget(hint);
	root->addLayout(header);

	m_list = new QListWidget(central);
	m_list->setObjectName("fileList");
	m_list->setAlternatingRowColors(true);
	m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_list->setUniformItemSizes(true);
	root->addWidget(m_list, 1);

	auto *btnRow = new QHBoxLayout();
	btnRow->setSpacing(8);
	m_btnAdd = new QPushButton(QStringLiteral("添加文件"), central);
	m_btnClear = new QPushButton(QStringLiteral("清空列表"), central);
	m_btnConvert = new QPushButton(QStringLiteral("开始转换"), central);
	m_btnConvert->setObjectName("primaryBtn");
	m_btnDetail = new QPushButton(QStringLiteral("查看失败原因"), central);
	m_btnDetail->setEnabled(false);
	m_btnOpenOut = new QPushButton(QStringLiteral("打开输出目录"), central);
	m_btnOpenOut->setEnabled(false);
	btnRow->addWidget(m_btnAdd);
	btnRow->addWidget(m_btnClear);
	btnRow->addStretch(1);
	btnRow->addWidget(m_btnDetail);
	btnRow->addWidget(m_btnOpenOut);
	btnRow->addWidget(m_btnConvert);
	root->addLayout(btnRow);

	m_progress = new QProgressBar(central);
	m_progress->setRange(0, 1);
	m_progress->setValue(0);
	m_progress->setTextVisible(true);
	m_progress->setFormat(QStringLiteral("%v / %m"));
	root->addWidget(m_progress);

	m_status = new QLabel(QStringLiteral("就绪 · 等待添加文件"), central);
	m_status->setObjectName("statusLabel");
	root->addWidget(m_status);

	m_log = new QPlainTextEdit(central);
	m_log->setObjectName("logView");
	m_log->setReadOnly(true);
	m_log->setMaximumBlockCount(2000);
	m_log->setPlaceholderText(QStringLiteral("转换日志将显示在这里…失败时会显示具体原因"));
	root->addWidget(m_log, 1);

	connect(m_btnAdd, &QPushButton::clicked, this, &MainWindow::onAddFiles);
	connect(m_btnClear, &QPushButton::clicked, this, &MainWindow::onClear);
	connect(m_btnConvert, &QPushButton::clicked, this, &MainWindow::onConvert);
	connect(m_btnOpenOut, &QPushButton::clicked, this, &MainWindow::onOpenOutput);
	connect(m_btnDetail, &QPushButton::clicked, this, &MainWindow::onShowFailureDetail);
	connect(m_list, &QListWidget::itemDoubleClicked, this, &MainWindow::onItemDoubleClicked);

	m_thread = new QThread(this);
	m_worker = new ConvertWorker();
	m_worker->moveToThread(m_thread);
	connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
	connect(m_worker, &ConvertWorker::fileStarted, this, &MainWindow::onFileStarted);
	connect(m_worker, &ConvertWorker::fileFinished, this, &MainWindow::onFileFinished);
	connect(m_worker, &ConvertWorker::allFinished, this, &MainWindow::onAllFinished);
	m_thread->start();

	applyStyle();
}

MainWindow::~MainWindow()
{
	m_thread->quit();
	m_thread->wait();
}

void MainWindow::applyStyle()
{
	setStyleSheet(QStringLiteral(R"(
QMainWindow, QWidget {
	background-color: #16191f;
	color: #e6e8eb;
	font-family: "Microsoft YaHei UI", "Segoe UI", sans-serif;
	font-size: 13px;
}
#titleLabel {
	font-size: 18px;
	font-weight: 600;
	color: #f2f4f7;
}
#hintLabel {
	color: #8b929e;
}
#fileList, #logView {
	background-color: #1f232b;
	border: 1px solid #2e3440;
	border-radius: 6px;
	padding: 4px;
	selection-background-color: #3d5a40;
	selection-color: #ffffff;
}
#logView {
	font-family: "Cascadia Mono", "Consolas", monospace;
	font-size: 12px;
}
QPushButton {
	background-color: #2a2f3a;
	color: #e6e8eb;
	border: 1px solid #3a4250;
	border-radius: 6px;
	padding: 8px 14px;
	min-height: 18px;
}
QPushButton:hover {
	background-color: #343b49;
	border-color: #4a5566;
}
QPushButton:pressed {
	background-color: #232833;
}
QPushButton:disabled {
	color: #6b7280;
	background-color: #22262e;
	border-color: #2a2f3a;
}
#primaryBtn {
	background-color: #3f7d4a;
	border-color: #4f945c;
	color: #ffffff;
	font-weight: 600;
}
#primaryBtn:hover {
	background-color: #4a9158;
}
#primaryBtn:pressed {
	background-color: #356a3f;
}
#primaryBtn:disabled {
	background-color: #2a3a30;
	border-color: #33443a;
	color: #7a8a80;
}
#statusLabel {
	color: #a8b0bc;
	padding: 2px 0;
}
QProgressBar {
	background-color: #1f232b;
	border: 1px solid #2e3440;
	border-radius: 6px;
	text-align: center;
	color: #e6e8eb;
	height: 18px;
}
QProgressBar::chunk {
	background-color: #4f945c;
	border-radius: 5px;
}
QScrollBar:vertical {
	background: #16191f;
	width: 10px;
	margin: 0;
}
QScrollBar::handle:vertical {
	background: #3a4250;
	border-radius: 4px;
	min-height: 24px;
}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
	height: 0;
}
QListWidget::item {
	padding: 6px 4px;
}
QListWidget::item:selected {
	background: #3d5a40;
}
)"));
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
	if (event->mimeData()->hasUrls())
	{
		event->acceptProposedAction();
	}
}

void MainWindow::dropEvent(QDropEvent *event)
{
	QStringList paths;
	const auto urls = event->mimeData()->urls();
	for (const QUrl &url : urls)
	{
		if (url.isLocalFile())
		{
			paths << url.toLocalFile();
		}
	}
	if (!paths.isEmpty())
	{
		addFiles(paths);
		event->acceptProposedAction();
	}
}

void MainWindow::onAddFiles()
{
	const QStringList paths = QFileDialog::getOpenFileNames(
		this,
		QStringLiteral("选择 Litematica 投影文件"),
		QString(),
		QStringLiteral("Litematica (*.litematic);;所有文件 (*.*)"));
	if (!paths.isEmpty())
	{
		addFiles(paths);
	}
}

void MainWindow::onClear()
{
	if (m_busy)
	{
		return;
	}
	m_files.clear();
	m_list->clear();
	m_progress->setRange(0, 1);
	m_progress->setValue(0);
	m_status->setText(QStringLiteral("就绪 · 等待添加文件"));
	m_btnOpenOut->setEnabled(false);
}

void MainWindow::onConvert()
{
	if (m_busy || m_files.isEmpty())
	{
		return;
	}

	setBusy(true);
	m_log->clear();
	m_progress->setRange(0, m_files.size());
	m_progress->setValue(0);
	m_status->setText(QStringLiteral("正在转换…"));
	appendLog(QStringLiteral("开始转换 %1 个文件").arg(m_files.size()));

	QMetaObject::invokeMethod(m_worker, "convertFiles", Qt::QueuedConnection,
		Q_ARG(QStringList, m_files));
}

void MainWindow::onOpenOutput()
{
	if (!m_lastOutputDir.isEmpty() && QDir(m_lastOutputDir).exists())
	{
		QDesktopServices::openUrl(QUrl::fromLocalFile(m_lastOutputDir));
	}
}

void MainWindow::onFileStarted(const QString &path, int index, int total)
{
	const QFileInfo info(path);
	m_status->setText(QStringLiteral("正在转换：%1").arg(info.fileName()));
	appendLog(QStringLiteral("\n[%1/%2] %3").arg(index).arg(total).arg(path));
}

void MainWindow::onFileFinished(const QString &path, bool success, const QString &errorReason,
	const QString &detailLog, qint64 elapsedMs)
{
	const QFileInfo info(path);
	m_lastOutputDir = info.absolutePath();
	m_btnOpenOut->setEnabled(true);

	if (success)
	{
		appendLog(QStringLiteral("结果：成功 · 用时 %1 ms").arg(elapsedMs));
	}
	else
	{
		appendLog(QStringLiteral("结果：失败 · 用时 %1 ms").arg(elapsedMs));
		appendLog(QStringLiteral("失败原因：%1").arg(errorReason.isEmpty()
			? QStringLiteral("未知错误")
			: errorReason));
	}

	if (!detailLog.isEmpty())
	{
		appendLog(detailLog);
	}

	for (int i = 0; i < m_list->count(); ++i)
	{
		QListWidgetItem *item = m_list->item(i);
		if (item->data(Qt::UserRole).toString() == path)
		{
			const QString mark = success ? QStringLiteral("✓") : QStringLiteral("✗");
			QString label = QStringLiteral("%1  %2").arg(mark, info.fileName());
			if (!success && !errorReason.isEmpty())
			{
				// 列表里直接露出失败原因（截断），完整内容见提示/弹窗
				QString shortReason = errorReason;
				if (shortReason.size() > 36)
				{
					shortReason = shortReason.left(36) + QStringLiteral("…");
				}
				label += QStringLiteral("  —  ").append(shortReason);
			}
			item->setText(label);
			item->setForeground(success ? QColor("#7dcea0") : QColor("#e07a7a"));
			item->setData(Qt::UserRole + 1, success);
			item->setData(Qt::UserRole + 2, errorReason);
			item->setData(Qt::UserRole + 3, detailLog);
			const QString tip = success
				? QStringLiteral("%1\n转换成功").arg(path)
				: QStringLiteral("%1\n\n失败原因：%2\n\n（双击可查看详情）").arg(path, errorReason);
			item->setToolTip(tip);
			break;
		}
	}

	m_btnDetail->setEnabled(true);
	m_progress->setValue(m_progress->value() + 1);
}

void MainWindow::onAllFinished(int successCount, int failCount)
{
	setBusy(false);
	m_status->setText(QStringLiteral("完成 · 成功 %1 · 失败 %2").arg(successCount).arg(failCount));
	appendLog(QStringLiteral("\n全部完成：成功 %1，失败 %2").arg(successCount).arg(failCount));
	if (failCount > 0)
	{
		m_btnDetail->setEnabled(true);
		appendLog(QStringLiteral("可点击「查看失败原因」或双击列表中的失败项查看详情。"));
	}
}

void MainWindow::onShowFailureDetail()
{
	QStringList failed;
	for (int i = 0; i < m_list->count(); ++i)
	{
		QListWidgetItem *item = m_list->item(i);
		const QVariant status = item->data(Qt::UserRole + 1);
		if (!status.isValid() || status.toBool())
		{
			continue; // 未转换 或 成功
		}
		const QString reason = item->data(Qt::UserRole + 2).toString();
		if (!reason.isEmpty())
		{
			failed << QStringLiteral("• %1\n  原因：%2")
				.arg(item->data(Qt::UserRole).toString(), reason);
		}
	}

	if (failed.isEmpty())
	{
		QMessageBox::information(this, QStringLiteral("失败原因"),
			QStringLiteral("当前没有失败的转换任务。"));
		return;
	}

	QMessageBox::warning(this, QStringLiteral("失败原因汇总"),
		QStringLiteral("以下文件转换失败：\n\n%1").arg(failed.join(QStringLiteral("\n\n"))));
}

void MainWindow::onItemDoubleClicked(QListWidgetItem *item)
{
	if (!item)
	{
		return;
	}
	const QVariant status = item->data(Qt::UserRole + 1);
	if (!status.isValid() || status.toBool())
	{
		return;
	}

	const QString path = item->data(Qt::UserRole).toString();
	const QString reason = item->data(Qt::UserRole + 2).toString();
	const QString detail = item->data(Qt::UserRole + 3).toString();

	const QString text = QStringLiteral(
		"文件：%1\n\n"
		"失败原因：%2\n\n"
		"详细日志：\n%3")
		.arg(path,
			reason.isEmpty() ? QStringLiteral("未知错误") : reason,
			detail.isEmpty() ? QStringLiteral("（无）") : detail);

	QMessageBox::warning(this, QStringLiteral("转换失败详情"), text);
}

QString MainWindow::failureReasonOf(QListWidgetItem *item) const
{
	if (!item)
	{
		return {};
	}
	return item->data(Qt::UserRole + 2).toString();
}

void MainWindow::appendLog(const QString &text)
{
	m_log->appendPlainText(text);
}

void MainWindow::addFiles(const QStringList &paths)
{
	for (const QString &raw : paths)
	{
		const QFileInfo info(raw);
		if (!info.exists() || !info.isFile())
		{
			continue;
		}

		const QString path = info.absoluteFilePath();
		if (m_files.contains(path))
		{
			continue;
		}

		m_files << path;
		auto *item = new QListWidgetItem(QStringLiteral("○  %1").arg(info.fileName()), m_list);
		item->setData(Qt::UserRole, path);
		item->setData(Qt::UserRole + 1, QVariant()); // 未转换
		item->setToolTip(path);
	}

	if (!m_files.isEmpty())
	{
		m_status->setText(QStringLiteral("已添加 %1 个文件").arg(m_files.size()));
	}
}

void MainWindow::setBusy(bool busy)
{
	m_busy = busy;
	m_btnAdd->setEnabled(!busy);
	m_btnClear->setEnabled(!busy);
	m_btnConvert->setEnabled(!busy && !m_files.isEmpty());
	if (busy)
	{
		m_btnDetail->setEnabled(false);
	}
}
