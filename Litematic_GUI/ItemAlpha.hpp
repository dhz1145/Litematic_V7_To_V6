#pragma once

#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QString>
#include <QVector>

#include <algorithm>

#include "LitematicFileConversion.h"

#include <nbt_cpp/NBT_All.hpp>

// α：模组创造物品栏 id 列表（itemlister/1 json）
struct AlphaItemSet
{
	QString path;
	QSet<QString> ids;
	bool loaded() const { return !ids.isEmpty(); }
};

struct ResourceIdCount
{
	QString id;
	int count = 0;
};

inline bool LooksLikeResourceId(const QString &s)
{
	const int colon = s.indexOf(QLatin1Char(':'));
	if (colon <= 0 || colon >= s.size() - 1)
	{
		return false;
	}
	if (s.size() > 128)
	{
		return false;
	}
	for (int i = 0; i < s.size(); ++i)
	{
		const QChar c = s.at(i);
		if (c == QLatin1Char(':') || c == QLatin1Char('_') || c == QLatin1Char('/') ||
			c == QLatin1Char('.') || c == QLatin1Char('-'))
		{
			continue;
		}
		if (c >= QLatin1Char('a') && c <= QLatin1Char('z'))
		{
			continue;
		}
		if (c >= QLatin1Char('A') && c <= QLatin1Char('Z'))
		{
			continue;
		}
		if (c >= QLatin1Char('0') && c <= QLatin1Char('9'))
		{
			continue;
		}
		return false;
	}
	return true;
}

inline bool LoadAlphaFile(const QString &path, AlphaItemSet &out, QString &errMsg)
{
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly))
	{
		errMsg = QStringLiteral("无法打开 α 文件：%1").arg(path);
		return false;
	}
	const QByteArray raw = f.readAll();
	f.close();

	QJsonParseError perr{};
	const QJsonDocument doc = QJsonDocument::fromJson(raw, &perr);
	if (perr.error != QJsonParseError::NoError)
	{
		errMsg = QStringLiteral("α JSON 解析失败：%1").arg(perr.errorString());
		return false;
	}
	if (!doc.isObject())
	{
		errMsg = QStringLiteral("α JSON 根节点不是对象");
		return false;
	}
	const QJsonObject root = doc.object();
	const QJsonValue itemsVal = root.value(QStringLiteral("items"));
	if (!itemsVal.isArray())
	{
		errMsg = QStringLiteral("α JSON 缺少 items 数组");
		return false;
	}

	QSet<QString> ids;
	const QJsonArray arr = itemsVal.toArray();
	ids.reserve(arr.size());
	for (const QJsonValue &v : arr)
	{
		if (!v.isString())
		{
			continue;
		}
		const QString id = v.toString().trimmed();
		if (!id.isEmpty())
		{
			ids.insert(id);
		}
	}
	if (ids.isEmpty())
	{
		errMsg = QStringLiteral("α 文件中没有有效物品 id");
		return false;
	}

	out.path = path;
	out.ids = ids;
	errMsg.clear();
	return true;
}

inline void CollectIdsFromNode(const NBT_Node &node, QHash<QString, int> &counts)
{
	const NBT_TAG tag = node.GetTag();
	switch (tag)
	{
	case NBT_TAG::String:
	{
		const auto *pStr = node.GetIfString();
		if (pStr == nullptr)
		{
			return;
		}
		try
		{
			const auto utf8 = pStr->ToCharTypeUTF8();
			const QString qs = QString::fromUtf8(
				reinterpret_cast<const char *>(utf8.data()),
				static_cast<qsizetype>(utf8.size()));
			if (LooksLikeResourceId(qs))
			{
				counts[qs] += 1;
			}
		}
		catch (...)
		{
		}
		return;
	}
	case NBT_TAG::List:
	{
		const auto *pList = node.GetIfList();
		if (pList == nullptr)
		{
			return;
		}
		for (const auto &it : *pList)
		{
			CollectIdsFromNode(it, counts);
		}
		return;
	}
	case NBT_TAG::Compound:
	{
		const auto *pCpd = node.GetIfCompound();
		if (pCpd == nullptr)
		{
			return;
		}
		for (const auto &kv : *pCpd)
		{
			CollectIdsFromNode(kv.second, counts);
		}
		return;
	}
	default:
		return;
	}
}

// 读取 V6 投影，统计其中所有形如 namespace:path 的字符串出现次数
inline bool CollectResourceIdsFromLitematic(
	const QString &litematicPath,
	QHash<QString, int> &counts,
	QString &errMsg)
{
	counts.clear();
	const std::filesystem::path fsPath = std::filesystem::path(litematicPath.toStdWString());

	std::vector<uint8_t> fileBytes{};
	if (!NBT_IO::ReadFile(fsPath, fileBytes))
	{
		errMsg = QStringLiteral("无法读取投影文件：%1").arg(litematicPath);
		return false;
	}

	std::vector<uint8_t> dataBytes{};
	if (!NBT_IO::DecompressDataNoThrow(dataBytes, fileBytes))
	{
		dataBytes = std::move(fileBytes);
	}

	NBT_Type::Compound root{};
	if (!NBT_Reader::ReadNBT(dataBytes, 0, root))
	{
		errMsg = QStringLiteral("无法解析投影 NBT：%1").arg(litematicPath);
		return false;
	}

	// ReadNBT 读到 root compound；再遍历其内容（以及整个树）
	for (const auto &kv : root)
	{
		CollectIdsFromNode(kv.second, counts);
	}
	// 有些 id 也可能直接以字符串形式出现在更深层，已递归覆盖

	if (counts.isEmpty())
	{
		errMsg = QStringLiteral("未在文件中扫描到物品/方块 id");
		return false;
	}
	return true;
}

inline QVector<ResourceIdCount> DiffMissingIds(
	const QHash<QString, int> &schematicCounts,
	const AlphaItemSet &alpha)
{
	QVector<ResourceIdCount> missing;
	missing.reserve(schematicCounts.size());
	for (auto it = schematicCounts.constBegin(); it != schematicCounts.constEnd(); ++it)
	{
		if (!alpha.ids.contains(it.key()))
		{
			missing.append(ResourceIdCount{it.key(), it.value()});
		}
	}
	std::sort(missing.begin(), missing.end(),
		[](const ResourceIdCount &a, const ResourceIdCount &b) {
			if (a.count != b.count)
			{
				return a.count > b.count;
			}
			return a.id < b.id;
		});
	return missing;
}
