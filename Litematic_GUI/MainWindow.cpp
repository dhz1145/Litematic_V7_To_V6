#include "MainWindow.h"
#include "ConvertWorker.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QColor>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QSize>
#include <QMimeData>
#include <QMessageBox>
#include <QPainter>
#include <QPaintEvent>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QThread>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

namespace
{

// 自绘圆角弹出菜单：避开 QMenu 在 Windows 上边框直角/裁切问题
class FileListContextMenu : public QDialog
{
public:
	explicit FileListContextMenu(QWidget *parent = nullptr)
		: QDialog(parent, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint)
	{
		setModal(true);
		setAttribute(Qt::WA_TranslucentBackground, true);

		auto *lay = new QVBoxLayout(this);
		lay->setContentsMargins(6, 6, 6, 6);
		lay->setSpacing(0);

		m_remove = new QPushButton(QStringLiteral("从列表移除"), this);
		m_remove->setObjectName("menuRemoveBtn");
		m_remove->setCursor(Qt::PointingHandCursor);
		m_remove->setFocus();
		m_remove->setFixedHeight(28);
		lay->addWidget(m_remove);

		setStyleSheet(QStringLiteral(R"(
#menuRemoveBtn {
	background-color: transparent;
	color: #e6e8eb;
	border: none;
	border-radius: 5px;
	padding: 2px 8px 2px 8px;
	text-align: left;
	font-size: 13px;
	min-width: 0px;
	max-width: 140px;
}
#menuRemoveBtn:hover {
	background-color: #3d5a40;
}
#menuRemoveBtn:pressed {
	background-color: #356a3f;
}
)"));

		connect(m_remove, &QPushButton::clicked, this, &QDialog::accept);
		adjustSize();
	}

	QSize sizeHint() const override
	{
		return QSize(112, 40);
	}

protected:
	void paintEvent(QPaintEvent *event) override
	{
		Q_UNUSED(event);
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing, true);
		const QRectF frame = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
		p.setPen(QPen(QColor(0x2e, 0x34, 0x40), 1.0));
		p.setBrush(QColor(0x1f, 0x23, 0x2b));
		p.drawRoundedRect(frame, 8.0, 8.0);
	}

	void keyPressEvent(QKeyEvent *event) override
	{
		if (event->key() == Qt::Key_Escape)
		{
			reject();
			return;
		}
		QDialog::keyPressEvent(event);
	}

private:
	QPushButton *m_remove = nullptr;
};

} // namespace

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
	auto *hint = new QLabel(QStringLiteral("拖入 .litematic 文件，或点击「添加文件」。默认输出到源文件同目录；也可选择输出文件夹。不会覆盖源文件。"), central);
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
	m_list->setContextMenuPolicy(Qt::CustomContextMenu);
	root->addWidget(m_list, 1);

	// 输出目录行
	auto *outRow = new QHBoxLayout();
	outRow->setSpacing(8);
	m_btnPickOut = new QPushButton(central);
	m_btnPickOut->setObjectName("pickOutBtn");
	m_btnPickOut->setToolTip(QStringLiteral("选择输出文件夹"));
	m_btnPickOut->setFixedWidth(36);
	m_btnPickOut->setFixedHeight(32);
	const QIcon folderIcon(QStringLiteral(":/icons/folder.png"));
	m_btnPickOut->setIcon(folderIcon);
	m_btnPickOut->setIconSize(QSize(18, 18));
	m_btnResetOut = new QPushButton(QStringLiteral("恢复默认"), central);
	m_btnResetOut->setToolTip(QStringLiteral("输出到各源文件所在目录"));
	m_btnResetOut->setEnabled(false);
	m_outDirLabel = new QLabel(central);
	m_outDirLabel->setObjectName("outDirLabel");
	m_outDirLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
	m_outDirLabel->setMinimumWidth(0);
	refreshOutputDirLabel();
	outRow->addWidget(m_btnPickOut);
	outRow->addWidget(m_btnResetOut);
	outRow->addWidget(m_outDirLabel, 1);
	root->addLayout(outRow);

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
	connect(m_btnPickOut, &QPushButton::clicked, this, &MainWindow::onChooseOutputDir);
	connect(m_btnResetOut, &QPushButton::clicked, this, &MainWindow::onResetOutputDir);
	connect(m_list, &QListWidget::itemDoubleClicked, this, &MainWindow::onItemDoubleClicked);
	connect(m_list, &QListWidget::customContextMenuRequested, this, &MainWindow::onListCustomContextMenu);

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
#outDirLabel {
	color: #a8b0bc;
	font-size: 12px;
}
#pickOutBtn {
	background-color: #2a2f3a;
	border: 1px solid #3a4250;
	border-radius: 6px;
	padding: 0px;
}
#pickOutBtn:hover {
	background-color: #343b49;
	border-color: #4a5566;
}
#pickOutBtn:pressed {
	background-color: #232833;
}
#pickOutBtn:disabled {
	opacity: 0.5;
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
	if (m_busy)
	{
		event->ignore();
		return;
	}
	if (event->mimeData()->hasUrls())
	{
		event->acceptProposedAction();
	}
}

void MainWindow::dropEvent(QDropEvent *event)
{
	// 转换中禁止拖入，避免列表与本轮任务不同步
	if (m_busy)
	{
		event->ignore();
		return;
	}
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
	if (m_busy)
	{
		return;
	}
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
	m_btnDetail->setEnabled(false);
	updateActionButtons();
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
	appendLog(m_customOutputDir.isEmpty()
		? QStringLiteral("输出目录：源文件同目录")
		: QStringLiteral("输出目录：%1").arg(m_customOutputDir));

	QMetaObject::invokeMethod(m_worker, "convertFiles", Qt::QueuedConnection,
		Q_ARG(QStringList, m_files),
		Q_ARG(QString, m_customOutputDir));
}

void MainWindow::onOpenOutput()
{
	QString dir = m_customOutputDir;
	if (dir.isEmpty())
	{
		dir = m_lastOutputDir;
	}
	if (!dir.isEmpty() && QDir(dir).exists())
	{
		QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
	}
}

void MainWindow::onChooseOutputDir()
{
	if (m_busy)
	{
		return;
	}
	const QString dir = QFileDialog::getExistingDirectory(
		this,
		QStringLiteral("选择输出文件夹"),
		m_customOutputDir.isEmpty() ? m_lastOutputDir : m_customOutputDir,
		QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
	if (dir.isEmpty())
	{
		return;
	}
	m_customOutputDir = QDir(dir).absolutePath();
	refreshOutputDirLabel();
	m_status->setText(QStringLiteral("输出目录：%1").arg(m_customOutputDir));
	// 已选定输出目录即可打开，不必等转换完成
	if (QDir(m_customOutputDir).exists())
	{
		m_btnOpenOut->setEnabled(true);
	}
}

void MainWindow::onResetOutputDir()
{
	if (m_busy)
	{
		return;
	}
	m_customOutputDir.clear();
	refreshOutputDirLabel();
	m_status->setText(QStringLiteral("输出目录：源文件同目录"));
}

void MainWindow::refreshOutputDirLabel()
{
	if (!m_outDirLabel || !m_btnResetOut)
	{
		return;
	}
	if (m_customOutputDir.isEmpty())
	{
		m_outDirLabel->setText(QStringLiteral("输出目录：源文件同目录"));
		m_outDirLabel->setToolTip(QStringLiteral("转换结果将保存到每个源文件所在文件夹"));
		m_btnResetOut->setEnabled(false);
	}
	else
	{
		m_outDirLabel->setText(QStringLiteral("输出目录：%1").arg(m_customOutputDir));
		m_outDirLabel->setToolTip(m_customOutputDir);
		m_btnResetOut->setEnabled(true);
	}
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
	if (!m_busy && (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace))
	{
		if (m_list && m_list->hasFocus())
		{
			onRemoveSelectedFiles();
			event->accept();
			return;
		}
	}
	QMainWindow::keyPressEvent(event);
}

void MainWindow::onListCustomContextMenu(const QPoint &pos)
{
	if (m_busy || !m_list)
	{
		return;
	}

	QListWidgetItem *item = m_list->itemAt(pos);
	// 右键空白：清空选中，不弹菜单
	if (!item)
	{
		m_list->clearSelection();
		return;
	}

	// 右键项：强制选中该目标，避免 setCurrentItem 在多选下不生效
	m_list->clearSelection();
	item->setSelected(true);
	m_list->setCurrentItem(item);

	if (m_list->selectedItems().isEmpty())
	{
		return;
	}

	FileListContextMenu menu(m_list);
	menu.setWindowTitle(QStringLiteral("从列表移除"));
	const QPoint global = m_list->viewport()->mapToGlobal(pos);
	menu.move(global);
	if (menu.exec() == QDialog::Accepted)
	{
		onRemoveSelectedFiles();
	}
}

void MainWindow::onRemoveSelectedFiles()
{
	if (m_busy || !m_list)
	{
		return;
	}

	const auto selected = m_list->selectedItems();
	if (selected.isEmpty())
	{
		return;
	}

	int removed = 0;
	for (QListWidgetItem *item : selected)
	{
		const QString path = item->data(Qt::UserRole).toString();
		for (int i = m_files.size() - 1; i >= 0; --i)
		{
			if (QString::compare(m_files.at(i), path, Qt::CaseInsensitive) == 0)
			{
				m_files.removeAt(i);
			}
		}
		delete item;
		++removed;
	}

	if (m_files.isEmpty())
	{
		m_progress->setRange(0, 1);
		m_progress->setValue(0);
		m_btnDetail->setEnabled(false);
		m_btnOpenOut->setEnabled(false);
		m_status->setText(QStringLiteral("就绪 · 等待添加文件"));
	}
	else
	{
		m_status->setText(QStringLiteral("列表剩余 %1 个文件（已移除 %2 个）")
			.arg(m_files.size()).arg(removed));
		// 列表里可能已无失败项时，关闭「查看失败原因」
		bool hasFail = false;
		for (int i = 0; i < m_list->count(); ++i)
		{
			const QVariant st = m_list->item(i)->data(Qt::UserRole + 1);
			if (st.isValid() && !st.toBool())
			{
				hasFail = true;
				break;
			}
		}
		m_btnDetail->setEnabled(hasFail);
	}
	updateActionButtons();
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
	// 「打开输出目录」优先用自定义目录；否则用本次实际输出位置
	if (m_customOutputDir.isEmpty())
	{
		m_lastOutputDir = info.absolutePath();
	}
	else
	{
		m_lastOutputDir = m_customOutputDir;
	}
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

	// 仅失败时启用「查看失败原因」
	if (!success)
	{
		m_btnDetail->setEnabled(true);
	}
	m_progress->setValue(m_progress->value() + 1);
}

void MainWindow::onAllFinished(int successCount, int failCount)
{
	setBusy(false);
	m_status->setText(QStringLiteral("完成 · 成功 %1 · 失败 %2").arg(successCount).arg(failCount));
	appendLog(QStringLiteral("\n全部完成：成功 %1，失败 %2").arg(successCount).arg(failCount));
	m_btnDetail->setEnabled(failCount > 0);
	if (failCount > 0)
	{
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

void MainWindow::appendLog(const QString &text)
{
	m_log->appendPlainText(text);
}

void MainWindow::addFiles(const QStringList &paths)
{
	if (m_busy)
	{
		return;
	}
	for (const QString &raw : paths)
	{
		const QFileInfo info(raw);
		if (!info.exists() || !info.isFile())
		{
			continue;
		}

		const QString path = QDir::cleanPath(info.absoluteFilePath());
		// Windows 路径不区分大小写，避免 E:\a 与 e:\a 重复
		const bool exists = std::any_of(m_files.cbegin(), m_files.cend(),
			[&path](const QString &p) {
				return QString::compare(p, path, Qt::CaseInsensitive) == 0;
			});
		if (exists)
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
	// 移除后再添加时，必须重新启用「开始转换」
	updateActionButtons();
}

void MainWindow::updateActionButtons()
{
	const bool hasFiles = !m_files.isEmpty();
	m_btnAdd->setEnabled(!m_busy);
	m_btnClear->setEnabled(!m_busy);
	m_btnConvert->setEnabled(!m_busy && hasFiles);
	if (m_btnPickOut)
	{
		m_btnPickOut->setEnabled(!m_busy);
	}
	if (m_btnResetOut)
	{
		m_btnResetOut->setEnabled(!m_busy && !m_customOutputDir.isEmpty());
	}
}

void MainWindow::setBusy(bool busy)
{
	m_busy = busy;
	if (busy)
	{
		m_btnDetail->setEnabled(false);
	}
	updateActionButtons();
}
