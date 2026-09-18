#include "ConvertWorker.h"

#include "LitematicFileConversion.h"

#include <QFile>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <string>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#include <windows.h>
#endif

namespace
{

// Redirect both stdout and stderr into a temp file so CLI-style printf/NBT_Print
// messages can be shown in the GUI log (GUI subsystem has no console).
class StdStreamCapture
{
public:
	StdStreamCapture()
	{
#ifdef _WIN32
		wchar_t tmpDir[MAX_PATH]{};
		wchar_t tmpFile[MAX_PATH]{};
		GetTempPathW(MAX_PATH, tmpDir);
		GetTempFileNameW(tmpDir, L"lmc", 0, tmpFile);
		m_path = QString::fromWCharArray(tmpFile);

		m_oldOut = _dup(_fileno(stdout));
		m_oldErr = _dup(_fileno(stderr));

		FILE *fpOut = nullptr;
		FILE *fpErr = nullptr;
		if (_wfreopen_s(&fpOut, tmpFile, L"w", stdout) == 0)
		{
			m_okOut = true;
		}
		if (_wfreopen_s(&fpErr, tmpFile, L"a", stderr) == 0)
		{
			m_okErr = true;
		}
#else
		char tmpl[] = "/tmp/lmcXXXXXX";
		int fd = mkstemp(tmpl);
		if (fd >= 0)
		{
			close(fd);
			m_path = QString::fromLocal8Bit(tmpl);
			fflush(stdout);
			fflush(stderr);
			m_oldOut = dup(fileno(stdout));
			m_oldErr = dup(fileno(stderr));
			m_okOut = freopen(tmpl, "w", stdout) != nullptr;
			m_okErr = freopen(tmpl, "a", stderr) != nullptr;
		}
#endif
	}

	~StdStreamCapture()
	{
		restore();
		if (!m_path.isEmpty())
		{
			QFile::remove(m_path);
		}
	}

	QString takeLog()
	{
		restore();
		QString content;
		QFile f(m_path);
		if (f.open(QIODevice::ReadOnly | QIODevice::Text))
		{
			const QByteArray raw = f.readAll();
			f.close();
			if (raw.isEmpty())
			{
				return {};
			}
			// 源码中的中文字符串在 MinGW 下以 UTF-8 存入二进制，printf 也写出 UTF-8。
			// 若误用 fromLocal8Bit（GBK）会得到“鏁版嵁…”这类乱码。
			content = QString::fromUtf8(raw);
		}
		return content.trimmed();
	}

	StdStreamCapture(const StdStreamCapture &) = delete;
	StdStreamCapture &operator=(const StdStreamCapture &) = delete;

private:
	void restore()
	{
		fflush(stdout);
		fflush(stderr);

#ifdef _WIN32
		if (m_okOut && m_oldOut >= 0)
		{
			_dup2(m_oldOut, _fileno(stdout));
			_close(m_oldOut);
			m_oldOut = -1;
			m_okOut = false;
		}
		if (m_okErr && m_oldErr >= 0)
		{
			_dup2(m_oldErr, _fileno(stderr));
			_close(m_oldErr);
			m_oldErr = -1;
			m_okErr = false;
		}
#else
		if (m_okOut && m_oldOut >= 0)
		{
			dup2(m_oldOut, fileno(stdout));
			close(m_oldOut);
			m_oldOut = -1;
			m_okOut = false;
		}
		if (m_okErr && m_oldErr >= 0)
		{
			dup2(m_oldErr, fileno(stderr));
			close(m_oldErr);
			m_oldErr = -1;
			m_okErr = false;
		}
#endif
	}

	QString m_path;
	int m_oldOut = -1;
	int m_oldErr = -1;
	bool m_okOut = false;
	bool m_okErr = false;
};

} // namespace

ConvertWorker::ConvertWorker(QObject *parent)
	: QObject(parent)
{
}

ConvertWorker::RunOutcome ConvertWorker::runOne(const QString &path, const QString &outputDir)
{
	RunOutcome out;
	const std::filesystem::path fsPath = std::filesystem::path(path.toStdWString());
	const std::filesystem::path fsOut = outputDir.isEmpty()
		? std::filesystem::path{}
		: std::filesystem::path(outputDir.toStdWString());

	const auto t0 = std::chrono::steady_clock::now();
	LitematicConvertResult result{};
	try
	{
		StdStreamCapture capture;
		result = ConvertLitematicFile_V7_To_V6(fsPath, fsOut);
		out.detailLog = capture.takeLog();
	}
	catch (const std::exception &e)
	{
		result.success = false;
		result.errorMessage = std::string("捕获异常：") + e.what();
	}
	catch (...)
	{
		result.success = false;
		result.errorMessage = "捕获未知异常";
	}
	const auto t1 = std::chrono::steady_clock::now();
	out.elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
	out.ok = result.success;

	QString outPath;
	try
	{
		if (!result.outputPath.empty())
		{
			outPath = QString::fromStdWString(result.outputPath.wstring());
		}
	}
	catch (...)
	{
	}

	if (out.ok)
	{
		out.errorReason.clear();
		const QString successLine = outPath.isEmpty()
			? QStringLiteral("转换成功")
			: QStringLiteral("转换成功\n输出：%1").arg(outPath);
		if (out.detailLog.isEmpty())
		{
			out.detailLog = successLine;
		}
		else
		{
			out.detailLog = successLine + QLatin1Char('\n') + out.detailLog;
		}
	}
	else
	{
		// 源码中文在 MinGW 下为 UTF-8；Qt6 fromStdString 亦按 UTF-8 解释
		out.errorReason = QString::fromUtf8(
			result.errorMessage.data(), static_cast<qsizetype>(result.errorMessage.size()));
		if (out.errorReason.isEmpty())
		{
			out.errorReason = QStringLiteral("未知错误（转换函数未返回原因）");
		}
		// detailLog 只保留底层附加信息，失败原因单独展示，避免重复
	}

	return out;
}

void ConvertWorker::convertFiles(const QStringList &files, const QString &outputDir)
{
	int success = 0;
	int fail = 0;
	const int total = files.size();

	for (int i = 0; i < total; ++i)
	{
		const QString &path = files.at(i);
		emit fileStarted(path, i + 1, total);

		const RunOutcome out = runOne(path, outputDir);
		if (out.ok)
		{
			++success;
		}
		else
		{
			++fail;
		}

		emit fileFinished(path, out.ok, out.errorReason, out.detailLog, out.elapsedMs);
	}

	emit allFinished(success, fail);
}
