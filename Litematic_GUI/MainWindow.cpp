#include "MainWindow.h"
#include "ConvertWorker.h"
#include "ItemAlpha.hpp"

#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QCompleter>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QSize>
#include <QMimeData>
#include <QMessageBox>
#include <QPainter>
#include <QPaintEvent>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QStringListModel>
#include <algorithm>
#include <QTableWidget>
#include <QHeaderView>
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

	// 文件列表 (左 2/3) + ItemList 面板 (右 1/3，默认隐藏)
	auto *listRow = new QHBoxLayout();
	listRow->setSpacing(8);

	m_list = new QListWidget(central);
	m_list->setObjectName("fileList");
	m_list->setAlternatingRowColors(true);
	m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_list->setUniformItemSizes(true);
	m_list->setContextMenuPolicy(Qt::CustomContextMenu);

	m_itemListPanel = new QWidget(central);
	m_itemListPanel->setObjectName("itemListPanel");
	m_itemListPanel->setAcceptDrops(true);
	m_itemListPanel->setMinimumWidth(200);
	auto *panelLay = new QVBoxLayout(m_itemListPanel);
	panelLay->setContentsMargins(10, 10, 10, 10);
	panelLay->setSpacing(8);

	auto *panelTitle = new QLabel(QStringLiteral("ItemList"), m_itemListPanel);
	panelTitle->setObjectName("itemListTitle");
	auto *panelHint = new QLabel(QStringLiteral("将 ItemList json 拖入此处，或点击下方按钮加载。\n转换后可对比投影中缺失的物品 id。"), m_itemListPanel);
	panelHint->setObjectName("hintLabel");
	panelHint->setWordWrap(true);

	m_btnLoadItemList = new QPushButton(QStringLiteral("加载 ItemList"), m_itemListPanel);
	m_btnLoadItemList->setToolTip(QStringLiteral("加载模组创造物品栏 id 列表 json"));
	m_btnShowDiff = new QPushButton(QStringLiteral("查看缺失对比"), m_itemListPanel);
	m_btnShowDiff->setEnabled(false);
	m_btnShowDiff->setToolTip(QStringLiteral("显示投影中存在但 ItemList 中没有的物品 id"));
	m_itemListLabel = new QLabel(QStringLiteral("ItemList：未加载"), m_itemListPanel);
	m_itemListLabel->setObjectName("outDirLabel");
	m_itemListLabel->setWordWrap(true);
	m_itemListLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

	panelLay->addWidget(panelTitle);
	panelLay->addWidget(panelHint);
	panelLay->addWidget(m_btnLoadItemList);
	panelLay->addWidget(m_btnShowDiff);
	panelLay->addWidget(m_itemListLabel);
	panelLay->addStretch(1);

	m_itemListPanel->setVisible(false);
	m_itemListPanel->installEventFilter(this);

	listRow->addWidget(m_list, 2);
	listRow->addWidget(m_itemListPanel, 1);
	root->addLayout(listRow, 1);

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
	m_chkItemList = new QCheckBox(QStringLiteral("ItemList"), central);
	m_chkItemList->setToolTip(QStringLiteral("勾选后显示右侧 ItemList 区域"));
	m_btnConvert = new QPushButton(QStringLiteral("开始转换"), central);
	m_btnConvert->setObjectName("primaryBtn");
	m_btnDetail = new QPushButton(QStringLiteral("查看失败原因"), central);
	m_btnDetail->setEnabled(false);
	m_btnOpenOut = new QPushButton(QStringLiteral("打开输出目录"), central);
	m_btnOpenOut->setEnabled(false);
	btnRow->addWidget(m_btnAdd);
	btnRow->addWidget(m_btnClear);
	btnRow->addWidget(m_chkItemList);
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
	connect(m_btnLoadItemList, &QPushButton::clicked, this, &MainWindow::onLoadItemList);
	connect(m_btnShowDiff, &QPushButton::clicked, this, &MainWindow::onShowItemListDiff);
	connect(m_chkItemList, &QCheckBox::toggled, this, &MainWindow::onToggleItemListPanel);
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

	m_alpha = new AlphaItemSet();
	{
		QSettings settings(QStringLiteral("LitematicTools"), QStringLiteral("Litematic_V7_To_V6_GUI"));
		const QString saved = settings.value(QStringLiteral("alphaPath")).toString();
		if (!saved.isEmpty() && QFileInfo::exists(saved))
		{
			QString err;
			if (LoadAlphaFile(saved, *m_alpha, err))
			{
				appendLog(QStringLiteral("已加载 ItemList：%1（%2 个 id）")
					.arg(saved).arg(m_alpha->ids.size()));
			}
			else
			{
				m_itemListError = err;
			}
		}
	}
	refreshItemListLabel();

	applyStyle();
}

MainWindow::~MainWindow()
{
	m_thread->quit();
	m_thread->wait();
	delete m_alpha;
	m_alpha = nullptr;
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
#itemListPanel {
	background-color: #1f232b;
	border: 1px solid #2e3440;
	border-radius: 6px;
}
#itemListTitle {
	font-size: 14px;
	font-weight: 600;
	color: #f2f4f7;
}
QCheckBox {
	color: #e6e8eb;
	spacing: 6px;
}
QCheckBox::indicator {
	width: 12px;
	height: 12px;
	border-radius: 2px;
}
QCheckBox::indicator:unchecked {
	background-color: #16191f;
	border: 1.2px solid #ffffff;
}
QCheckBox::indicator:checked {
	background-color: #ffffff;
	border: 1.2px solid #ffffff;
	image: url(:/icons/check_dark.png);
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
	background: #1a1d23;
	width: 10px;
	margin: 0;
	border: none;
}
QScrollBar::handle:vertical {
	background: #3a4250;
	border-radius: 4px;
	min-height: 24px;
	margin: 2px;
}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
	height: 0px;
	background: transparent;
	border: none;
}
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
	background: #1a1d23;
	border: none;
}
QScrollBar:horizontal {
	background: #1a1d23;
	height: 10px;
	margin: 0;
	border: none;
}
QScrollBar::handle:horizontal {
	background: #3a4250;
	border-radius: 4px;
	min-width: 24px;
	margin: 2px;
}
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
	width: 0px;
	background: transparent;
	border: none;
}
QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
	background: #1a1d23;
	border: none;
}
QTableWidget {
	background-color: #1f232b;
	border: 1px solid #2e3440;
	gridline-color: #2e3440;
	selection-background-color: #3d5a40;
}
QHeaderView::section {
	background-color: #242830;
	color: #c5cad3;
	border: none;
	border-right: 1px solid #2e3440;
	border-bottom: 1px solid #2e3440;
	padding: 4px;
}
QLineEdit {
	background-color: #1f232b;
	border: 1px solid #2e3440;
	border-radius: 4px;
	padding: 4px 6px;
	color: #e6e8eb;
}
QComboBox {
	background-color: #1f232b;
	border: 1px solid #2e3440;
	border-radius: 4px;
	padding: 4px 28px 4px 8px;
	color: #e6e8eb;
	min-height: 20px;
}
QComboBox::drop-down {
	subcontrol-origin: padding;
	subcontrol-position: top right;
	width: 26px;
	border: none;
	border-left: 1px solid #2e3440;
	border-top-right-radius: 4px;
	border-bottom-right-radius: 4px;
	background: #242830;
}
QComboBox::down-arrow {
	width: 0px;
	height: 0px;
	border-left: 5px solid transparent;
	border-right: 5px solid transparent;
	border-top: 6px solid #c8cdd6;
	margin-right: 8px;
}
QComboBox QAbstractItemView, QCompleter QAbstractItemView {
	background-color: #1f232b;
	color: #e6e8eb;
	border: 1px solid #2e3440;
	selection-background-color: #3d5a40;
	selection-color: #ffffff;
	outline: none;
}
QListWidget::item {
	padding: 6px 4px;
}
QListWidget::item:selected {
	background: #3d5a40;
}
)"));
}

bool MainWindow::handleDroppedUrls(const QList<QUrl> &urls)
{
	QStringList schematics;
	QStringList jsons;
	for (const QUrl &url : urls)
	{
		if (!url.isLocalFile())
		{
			continue;
		}
		const QString path = QFileInfo(url.toLocalFile()).absoluteFilePath();
		const QString suffix = QFileInfo(path).suffix().toLower();
		if (suffix == QLatin1String("json"))
		{
			jsons << path;
		}
		else
		{
			schematics << path;
		}
	}

	bool handled = false;
	if (!jsons.isEmpty())
	{
		// 拖入 json → 加载为 ItemList（取第一个）
		if (loadItemListFromPath(jsons.first()))
		{
			if (m_chkItemList && !m_chkItemList->isChecked())
			{
				m_chkItemList->setChecked(true);
			}
			handled = true;
		}
	}
	if (!schematics.isEmpty())
	{
		addFiles(schematics);
		handled = true;
	}
	return handled;
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
	if (m_busy)
	{
		event->ignore();
		return;
	}
	if (handleDroppedUrls(event->mimeData()->urls()))
	{
		event->acceptProposedAction();
	}
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == m_itemListPanel)
	{
		if (event->type() == QEvent::DragEnter)
		{
			auto *de = static_cast<QDragEnterEvent *>(event);
			if (!m_busy && de->mimeData()->hasUrls())
			{
				de->acceptProposedAction();
				return true;
			}
		}
		else if (event->type() == QEvent::Drop)
		{
			auto *de = static_cast<QDropEvent *>(event);
			if (!m_busy && handleDroppedUrls(de->mimeData()->urls()))
			{
				de->acceptProposedAction();
				return true;
			}
		}
	}
	return QMainWindow::eventFilter(watched, event);
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
	const QString &detailLog, qint64 elapsedMs, const QString &outputPath)
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
		if (!outputPath.isEmpty())
		{
			m_lastV6Path = outputPath;
		}
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

	// 仅在勾选 ItemList 且已加载列表时才自动对比
	if (m_chkItemList && m_chkItemList->isChecked() &&
		m_alpha && m_alpha->loaded() && successCount > 0 && !m_lastV6Path.isEmpty())
	{
		runItemListDiffOn(m_lastV6Path);
	}
}

void MainWindow::onToggleItemListPanel(bool on)
{
	if (m_itemListPanel)
	{
		m_itemListPanel->setVisible(on);
	}
}

bool MainWindow::loadItemListFromPath(const QString &path)
{
	AlphaItemSet loaded;
	QString err;
	if (!LoadAlphaFile(path, loaded, err))
	{
		m_itemListError = err;
		refreshItemListLabel();
		QMessageBox::warning(this, QStringLiteral("加载 ItemList 失败"), err);
		return false;
	}
	if (m_alpha == nullptr)
	{
		m_alpha = new AlphaItemSet();
	}
	*m_alpha = loaded;
	m_itemListError.clear();
	refreshItemListLabel();

	QSettings settings(QStringLiteral("LitematicTools"), QStringLiteral("Litematic_V7_To_V6_GUI"));
	settings.setValue(QStringLiteral("alphaPath"), path);

	appendLog(QStringLiteral("已加载 ItemList：%1（%2 个 id）").arg(path).arg(loaded.ids.size()));
	m_status->setText(QStringLiteral("ItemList 已加载：%1 个物品 id").arg(loaded.ids.size()));
	return true;
}

void MainWindow::onLoadItemList()
{
	if (m_busy)
	{
		return;
	}
	const QString startDir = (m_alpha && !m_alpha->path.isEmpty())
		? QFileInfo(m_alpha->path).absolutePath()
		: QString();
	const QString path = QFileDialog::getOpenFileName(
		this,
		QStringLiteral("选择 ItemList json"),
		startDir,
		QStringLiteral("物品列表 (*.json);;所有文件 (*.*)"));
	if (path.isEmpty())
	{
		return;
	}
	loadItemListFromPath(path);
}

void MainWindow::refreshItemListLabel()
{
	if (!m_itemListLabel || !m_btnShowDiff)
	{
		return;
	}
	if (m_alpha && m_alpha->loaded())
	{
		const QString name = QFileInfo(m_alpha->path).fileName();
		m_itemListLabel->setText(QStringLiteral("ItemList：%1 · %2 个 id").arg(name).arg(m_alpha->ids.size()));
		m_itemListLabel->setToolTip(m_alpha->path);
		m_btnShowDiff->setEnabled(!m_lastDiffV6Path.isEmpty() || !m_lastV6Path.isEmpty());
	}
	else
	{
		m_itemListLabel->setText(m_itemListError.isEmpty()
			? QStringLiteral("ItemList：未加载")
			: QStringLiteral("ItemList：%1").arg(m_itemListError));
		m_btnShowDiff->setEnabled(false);
	}
}

void MainWindow::onShowItemListDiff()
{
	const QString target = !m_lastDiffV6Path.isEmpty() ? m_lastDiffV6Path : m_lastV6Path;
	if (target.isEmpty())
	{
		QMessageBox::information(this, QStringLiteral("缺失对比"),
			QStringLiteral("还没有可对比的 V6 文件，请先转换。"));
		return;
	}
	if (m_alpha == nullptr || !m_alpha->loaded())
	{
		QMessageBox::information(this, QStringLiteral("缺失对比"),
			QStringLiteral("请先加载 ItemList。"));
		return;
	}
	if (target != m_lastDiffV6Path || m_lastAllCounts.isEmpty())
	{
		runItemListDiffOn(target);
		return;
	}
	showItemListReplaceDialog();
}

void MainWindow::runItemListDiffOn(const QString &v6Path)
{
	if (m_alpha == nullptr || !m_alpha->loaded())
	{
		return;
	}
	QHash<QString, SectionIdCounts> sectionCounts;
	QString err;
	if (!CollectResourceIdsFromLitematic(v6Path, sectionCounts, err))
	{
		appendLog(QStringLiteral("ItemList 对比失败：%1").arg(err));
		QMessageBox::warning(this, QStringLiteral("缺失对比失败"), err);
		return;
	}

	m_lastPaletteCounts.clear();
	m_lastContainerCounts.clear();
	m_lastEntityCounts.clear();
	m_lastOtherCounts.clear();
	for (auto it = sectionCounts.constBegin(); it != sectionCounts.constEnd(); ++it)
	{
		const SectionIdCounts &sc = it.value();
		if (sc.palette > 0)
		{
			m_lastPaletteCounts.insert(it.key(), sc.palette);
		}
		if (sc.container > 0)
		{
			m_lastContainerCounts.insert(it.key(), sc.container);
		}
		if (sc.entity > 0)
		{
			m_lastEntityCounts.insert(it.key(), sc.entity);
		}
		if (sc.other > 0)
		{
			m_lastOtherCounts.insert(it.key(), sc.other);
		}
	}

	const QHash<QString, int> counts = FlattenSectionCounts(sectionCounts, true, true, true);
	const QVector<ResourceIdCount> missing = DiffMissingIds(counts, *m_alpha);
	m_lastAllCounts = counts;
	m_lastDiffV6Path = v6Path;
	m_lastDiffTotalSchematicIds = counts.size();
	m_lastDiffMissingCount = missing.size();
	m_lastDiffLines.clear();
	for (const ResourceIdCount &rc : missing)
	{
		m_lastDiffLines << (rc.id + QLatin1Char('\t') + QString::number(rc.count));
	}

	m_lastDiffSummary = QStringLiteral(
		"对比文件：%1\nItemList：%2（%3 个 id）\n投影中扫描到 id 种类：%4\n"
		"其中 ItemList 中没有（缺失）：%5 种\n"
		"列表与次数会随「调色板/容器/实体」勾选变化。\n"
		"「NBT 出现次数」不等于建筑方块总数。")
		.arg(v6Path,
			QFileInfo(m_alpha->path).fileName(),
			QString::number(m_alpha->ids.size()),
			QString::number(m_lastDiffTotalSchematicIds),
			QString::number(m_lastDiffMissingCount));

	appendLog(QStringLiteral("ItemList 对比：%1 → 缺失 %2 种 id（投影共 %3 种）")
		.arg(QFileInfo(v6Path).fileName())
		.arg(m_lastDiffMissingCount)
		.arg(m_lastDiffTotalSchematicIds));

	m_btnShowDiff->setEnabled(true);
	refreshItemListLabel();
	showItemListReplaceDialog();
}

void MainWindow::showItemListReplaceDialog()
{
	if (m_alpha == nullptr || !m_alpha->loaded())
	{
		return;
	}
	if (m_lastDiffV6Path.isEmpty() || m_lastAllCounts.isEmpty())
	{
		return;
	}

	QDialog dlg(this);
	dlg.setWindowTitle(QStringLiteral("物品对比 / 替换（ItemList）"));
	dlg.resize(900, 580);
	dlg.setStyleSheet(styleSheet());
	auto *lay = new QVBoxLayout(&dlg);

	auto *info = new QLabel(m_lastDiffSummary, &dlg);
	info->setWordWrap(true);
	lay->addWidget(info);

	auto *optRow = new QHBoxLayout();
	auto *chkShowAll = new QCheckBox(QStringLiteral("显示全部"), &dlg);
	auto *chkPalette = new QCheckBox(QStringLiteral("调色板"), &dlg);
	auto *chkContainer = new QCheckBox(QStringLiteral("容器"), &dlg);
	auto *chkEntity = new QCheckBox(QStringLiteral("实体"), &dlg);
	chkPalette->setChecked(true);
	chkContainer->setChecked(true);
	chkEntity->setChecked(true);
	optRow->addWidget(chkShowAll);
	optRow->addStretch(1);
	optRow->addWidget(new QLabel(QStringLiteral("替换作用范围："), &dlg));
	optRow->addWidget(chkPalette);
	optRow->addWidget(chkContainer);
	optRow->addWidget(chkEntity);
	lay->addLayout(optRow);

	auto *table = new QTableWidget(&dlg);
	table->setColumnCount(3);
	table->setHorizontalHeaderLabels({
		QStringLiteral("物品 id"),
		QStringLiteral("NBT 出现次数"),
		QStringLiteral("替换为（ItemList）")});
	table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
	table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
	table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
	table->setSelectionBehavior(QAbstractItemView::SelectRows);
	table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	lay->addWidget(table, 1);

	QSet<QString> missingSet;
	for (const QString &line : m_lastDiffLines)
	{
		missingSet.insert(line.split(QLatin1Char('\t')).value(0));
	}

	auto *fullListModel = new QStringListModel(&dlg);
	auto *filterListModel = new QStringListModel(&dlg);
	fullListModel->setStringList(RankItemListSuggestions(m_alpha->ids, QString()));
	filterListModel->setStringList(QStringList());
	auto *completer = new QCompleter(filterListModel, &dlg);
	completer->setCaseSensitivity(Qt::CaseInsensitive);
	completer->setFilterMode(Qt::MatchContains);
	completer->setCompletionMode(QCompleter::PopupCompletion);

	auto rebuildTable = [&]() {
		const bool showAll = chkShowAll->isChecked();
		const bool usePal = chkPalette->isChecked();
		const bool useCon = chkContainer->isChecked();
		const bool useEnt = chkEntity->isChecked();
		const bool useOther = usePal && useCon && useEnt;

		QHash<QString, int> scoped;
		auto addHash = [&](const QHash<QString, int> &src) {
			for (auto it = src.constBegin(); it != src.constEnd(); ++it)
			{
				scoped[it.key()] += it.value();
			}
		};
		if (usePal)
		{
			addHash(m_lastPaletteCounts);
		}
		if (useCon)
		{
			addHash(m_lastContainerCounts);
		}
		if (useEnt)
		{
			addHash(m_lastEntityCounts);
		}
		if (useOther)
		{
			addHash(m_lastOtherCounts);
		}

		QVector<ResourceIdCount> rows;
		for (auto it = scoped.constBegin(); it != scoped.constEnd(); ++it)
		{
			if (it.value() <= 0)
			{
				continue;
			}
			if (!showAll && !missingSet.contains(it.key()))
			{
				continue;
			}
			rows.append(ResourceIdCount{it.key(), it.value()});
		}
		std::sort(rows.begin(), rows.end(),
			[](const ResourceIdCount &a, const ResourceIdCount &b) {
				if (a.count != b.count)
				{
					return a.count > b.count;
				}
				return a.id < b.id;
			});

		const bool updates = table->updatesEnabled();
		table->setUpdatesEnabled(false);
		table->setRowCount(rows.size());
		for (int i = 0; i < rows.size(); ++i)
		{
			auto *idItem = new QTableWidgetItem(rows.at(i).id);
			idItem->setFlags(idItem->flags() & ~Qt::ItemIsEditable);
			table->setItem(i, 0, idItem);
			auto *cntItem = new QTableWidgetItem(QString::number(rows.at(i).count));
			cntItem->setFlags(cntItem->flags() & ~Qt::ItemIsEditable);
			table->setItem(i, 1, cntItem);

			auto *combo = new QComboBox(table);
			combo->setEditable(true);
			combo->setInsertPolicy(QComboBox::NoInsert);
			combo->setModel(fullListModel);
			combo->setCompleter(completer);
			combo->blockSignals(true);
			combo->setCurrentText(QString());
			combo->blockSignals(false);
			if (combo->lineEdit())
			{
				combo->lineEdit()->setPlaceholderText(QStringLiteral("输入或点箭头选择"));
			}
			combo->setProperty("srcId", rows.at(i).id);
			if (combo->view())
			{
				combo->view()->setStyleSheet(QStringLiteral(
					"QAbstractItemView { background:#1f232b; color:#e6e8eb; "
					"border:1px solid #2e3440; selection-background-color:#3d5a40; "
					"selection-color:#ffffff; outline:none; }"));
			}
			table->setCellWidget(i, 2, combo);

			QObject::connect(combo, &QComboBox::editTextChanged, combo,
				[filterListModel, this](const QString &text) {
					if (m_alpha == nullptr)
					{
						return;
					}
					const QSignalBlocker blocker(filterListModel);
					filterListModel->setStringList(RankItemListSuggestions(m_alpha->ids, text));
				});
		}
		table->setUpdatesEnabled(updates);
		table->viewport()->update();
	};

	rebuildTable();
	QObject::connect(chkShowAll, &QCheckBox::toggled, &dlg, rebuildTable);
	QObject::connect(chkPalette, &QCheckBox::toggled, &dlg, rebuildTable);
	QObject::connect(chkContainer, &QCheckBox::toggled, &dlg, rebuildTable);
	QObject::connect(chkEntity, &QCheckBox::toggled, &dlg, rebuildTable);

	auto *btnRow = new QHBoxLayout();
	auto *btnApply = new QPushButton(QStringLiteral("应用全部替换"), &dlg);
	auto *btnClose = new QPushButton(QStringLiteral("关闭"), &dlg);
	btnRow->addStretch(1);
	btnRow->addWidget(btnApply);
	btnRow->addWidget(btnClose);
	lay->addLayout(btnRow);

	QObject::connect(btnClose, &QPushButton::clicked, &dlg, &QDialog::reject);
	QObject::connect(btnApply, &QPushButton::clicked, &dlg, [&]() {
		if (m_busy)
		{
			QMessageBox::information(&dlg, QStringLiteral("应用替换"),
				QStringLiteral("转换进行中，请稍后再应用替换。"));
			return;
		}
		if (!chkPalette->isChecked() && !chkContainer->isChecked() && !chkEntity->isChecked())
		{
			QMessageBox::warning(&dlg, QStringLiteral("应用替换"),
				QStringLiteral("请至少勾选一种替换作用范围。"));
			return;
		}

		QHash<QString, QString> map;
		for (int i = 0; i < table->rowCount(); ++i)
		{
			auto *combo = qobject_cast<QComboBox *>(table->cellWidget(i, 2));
			if (combo == nullptr)
			{
				continue;
			}
			const QString src = combo->property("srcId").toString();
			const QString dst = combo->currentText().trimmed();
			if (src.isEmpty() || dst.isEmpty() || src == dst)
			{
				continue;
			}
			map.insert(src, dst);
		}
		if (map.isEmpty())
		{
			QMessageBox::information(&dlg, QStringLiteral("应用替换"),
				QStringLiteral("还没有填写任何「替换为」目标。"));
			return;
		}

		const QString scopeText = QStringLiteral("%1%2%3")
			.arg(chkPalette->isChecked() ? QStringLiteral("调色板 ") : QString(),
				chkContainer->isChecked() ? QStringLiteral("容器 ") : QString(),
				chkEntity->isChecked() ? QStringLiteral("实体") : QString());
		const QString confirm = QStringLiteral(
			"将把 %1 条替换规则写回：\n\n%2\n\n"
			"作用范围：%3\n\n"
			"会直接覆盖该 V6 文件，且不可撤销。确定继续？")
			.arg(map.size())
			.arg(m_lastDiffV6Path, scopeText);
		if (QMessageBox::question(&dlg, QStringLiteral("确认覆盖 V6 文件"), confirm,
				QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
		{
			return;
		}

		QString err;
		int changed = 0;
		if (!ApplyIdReplacementsToLitematic(
				m_lastDiffV6Path, map,
				chkPalette->isChecked(), chkContainer->isChecked(), chkEntity->isChecked(),
				err, changed))
		{
			QMessageBox::warning(&dlg, QStringLiteral("替换失败"), err);
			appendLog(QStringLiteral("替换失败：%1").arg(err));
			return;
		}

		appendLog(QStringLiteral("已应用替换：%1 处 · 规则 %2 条 · 文件 %3")
			.arg(changed).arg(map.size()).arg(m_lastDiffV6Path));

		QHash<QString, SectionIdCounts> sectionCounts;
		QString err2;
		if (CollectResourceIdsFromLitematic(m_lastDiffV6Path, sectionCounts, err2))
		{
			m_lastPaletteCounts.clear();
			m_lastContainerCounts.clear();
			m_lastEntityCounts.clear();
			m_lastOtherCounts.clear();
			for (auto it = sectionCounts.constBegin(); it != sectionCounts.constEnd(); ++it)
			{
				const SectionIdCounts &sc = it.value();
				if (sc.palette > 0)
				{
					m_lastPaletteCounts.insert(it.key(), sc.palette);
				}
				if (sc.container > 0)
				{
					m_lastContainerCounts.insert(it.key(), sc.container);
				}
				if (sc.entity > 0)
				{
					m_lastEntityCounts.insert(it.key(), sc.entity);
				}
				if (sc.other > 0)
				{
					m_lastOtherCounts.insert(it.key(), sc.other);
				}
			}
			m_lastAllCounts = FlattenSectionCounts(sectionCounts, true, true, true);
			const QVector<ResourceIdCount> missing2 = DiffMissingIds(m_lastAllCounts, *m_alpha);
			m_lastDiffTotalSchematicIds = m_lastAllCounts.size();
			m_lastDiffMissingCount = missing2.size();
			m_lastDiffLines.clear();
			for (const ResourceIdCount &rc : missing2)
			{
				m_lastDiffLines << (rc.id + QLatin1Char('\t') + QString::number(rc.count));
			}
			refreshItemListLabel();
			appendLog(QStringLiteral("替换后重新扫描：缺失 %1 种（投影共 %2 种）")
				.arg(m_lastDiffMissingCount).arg(m_lastDiffTotalSchematicIds));
		}

		QMessageBox::information(&dlg, QStringLiteral("替换完成"),
			QStringLiteral("已写回：%1\n替换位置约 %2 处").arg(m_lastDiffV6Path).arg(changed));
		dlg.accept();
	});

	dlg.exec();
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
	if (m_btnLoadItemList)
	{
		m_btnLoadItemList->setEnabled(!m_busy);
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
