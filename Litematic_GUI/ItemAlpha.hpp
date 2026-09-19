#pragma once

#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include <algorithm>
#include <string_view>

#include "LitematicFileConversion.h"

#include <nbt_cpp/NBT_All.hpp>

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

struct SectionIdCounts
{
	int palette = 0;
	int container = 0;
	int entity = 0;
	int other = 0;

	int totalIn(bool doPalette, bool doContainer, bool doEntity) const
	{
		int n = 0;
		if (doPalette) n += palette;
		if (doContainer) n += container;
		if (doEntity) n += entity;
		// 与替换逻辑一致：三项全勾时才纳入「其它」
		if (doPalette && doContainer && doEntity) n += other;
		return n;
	}
};

enum class ReplaceSection : uint8_t
{
	Other = 0,
	Palette = 1,
	Container = 2,
	Entity = 3,
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
		if ((c >= QLatin1Char('a') && c <= QLatin1Char('z')) ||
			(c >= QLatin1Char('A') && c <= QLatin1Char('Z')) ||
			(c >= QLatin1Char('0') && c <= QLatin1Char('9')))
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
		errMsg = QStringLiteral("无法打开 ItemList 文件：%1").arg(path);
		return false;
	}
	const QByteArray raw = f.readAll();
	f.close();

	QJsonParseError perr{};
	const QJsonDocument doc = QJsonDocument::fromJson(raw, &perr);
	if (perr.error != QJsonParseError::NoError || !doc.isObject())
	{
		errMsg = QStringLiteral("ItemList JSON 解析失败");
		return false;
	}
	const QJsonValue itemsVal = doc.object().value(QStringLiteral("items"));
	if (!itemsVal.isArray())
	{
		errMsg = QStringLiteral("ItemList JSON 缺少 items 数组");
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
		errMsg = QStringLiteral("ItemList 中没有有效物品 id");
		return false;
	}
	out.path = path;
	out.ids = ids;
	errMsg.clear();
	return true;
}

inline QString NbtStringToQ(const NBT_Type::String &s)
{
	try
	{
		const auto utf8 = s.ToCharTypeUTF8();
		return QString::fromUtf8(
			reinterpret_cast<const char *>(utf8.data()),
			static_cast<qsizetype>(utf8.size()));
	}
	catch (...)
	{
		return {};
	}
}

inline ReplaceSection SectionFromCompoundKey(const NBT_Type::String &key)
{
	const QString k = NbtStringToQ(key);
	if (k == QLatin1String("BlockStatePalette"))
	{
		return ReplaceSection::Palette;
	}
	if (k == QLatin1String("TileEntities") || k == QLatin1String("PendingBlockTicks") ||
		k == QLatin1String("PendingFluidTicks"))
	{
		return ReplaceSection::Container;
	}
	if (k == QLatin1String("Entities"))
	{
		return ReplaceSection::Entity;
	}
	return ReplaceSection::Other;
}

inline bool AssignNbtString(NBT_Node &node, const QString &utf8Text)
{
	auto *pStr = node.GetIfString();
	if (pStr == nullptr)
	{
		return false;
	}
	try
	{
		const QByteArray raw = utf8Text.toUtf8();
		*pStr = NBT_Type::String(std::basic_string_view<char>(raw.constData(), static_cast<size_t>(raw.size())));
		return true;
	}
	catch (...)
	{
		return false;
	}
}

inline void CollectIdsFromNodeSectioned(
	const NBT_Node &node,
	ReplaceSection sec,
	QHash<QString, SectionIdCounts> &counts)
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
		const QString qs = NbtStringToQ(*pStr);
		if (!LooksLikeResourceId(qs))
		{
			return;
		}
		SectionIdCounts &sc = counts[qs];
		switch (sec)
		{
		case ReplaceSection::Palette: sc.palette += 1; break;
		case ReplaceSection::Container: sc.container += 1; break;
		case ReplaceSection::Entity: sc.entity += 1; break;
		default: sc.other += 1; break;
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
			CollectIdsFromNodeSectioned(it, sec, counts);
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
			ReplaceSection childSec = sec;
			const ReplaceSection keySec = SectionFromCompoundKey(kv.first);
			if (keySec != ReplaceSection::Other)
			{
				childSec = keySec;
			}
			CollectIdsFromNodeSectioned(kv.second, childSec, counts);
		}
		return;
	}
	default:
		return;
	}
}

inline bool CollectResourceIdsFromLitematic(
	const QString &litematicPath,
	QHash<QString, SectionIdCounts> &counts,
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
	for (const auto &kv : root)
	{
		CollectIdsFromNodeSectioned(kv.second, ReplaceSection::Other, counts);
	}
	if (counts.isEmpty())
	{
		errMsg = QStringLiteral("未在文件中扫描到物品/方块 id");
		return false;
	}
	return true;
}

inline QHash<QString, int> FlattenSectionCounts(
	const QHash<QString, SectionIdCounts> &sectionCounts,
	bool doPalette = true,
	bool doContainer = true,
	bool doEntity = true)
{
	QHash<QString, int> flat;
	for (auto it = sectionCounts.constBegin(); it != sectionCounts.constEnd(); ++it)
	{
		const int n = it.value().totalIn(doPalette, doContainer, doEntity);
		if (n > 0)
		{
			flat.insert(it.key(), n);
		}
	}
	return flat;
}

inline QVector<ResourceIdCount> DiffMissingIds(
	const QHash<QString, int> &schematicCounts,
	const AlphaItemSet &alpha)
{
	QVector<ResourceIdCount> missing;
	for (auto it = schematicCounts.constBegin(); it != schematicCounts.constEnd(); ++it)
	{
		if (!alpha.ids.contains(it.key()))
		{
			missing.append(ResourceIdCount{it.key(), it.value()});
		}
	}
	std::sort(missing.begin(), missing.end(),
		[](const ResourceIdCount &a, const ResourceIdCount &b) {
			if (a.count != b.count) return a.count > b.count;
			return a.id < b.id;
		});
	return missing;
}

inline QStringList RankItemListSuggestions(const QSet<QString> &ids, const QString &query)
{
	const QString q = query.trimmed().toLower();
	QStringList matched;
	if (q.isEmpty())
	{
		QStringList all = ids.values();
		std::sort(all.begin(), all.end());
		if (all.size() > 200)
		{
			all = all.mid(0, 200);
		}
		return all;
	}
	for (const QString &id : ids)
	{
		if (id.toLower().contains(q))
		{
			matched << id;
		}
	}
	std::sort(matched.begin(), matched.end(),
		[&q](const QString &a, const QString &b) {
			const QString al = a.toLower();
			const QString bl = b.toLower();
			const bool ap = al.startsWith(q);
			const bool bp = bl.startsWith(q);
			if (ap != bp) return ap;
			if (al.size() != bl.size()) return al.size() < bl.size();
			return al < bl;
		});
	if (matched.size() > 300)
	{
		matched = matched.mid(0, 300);
	}
	return matched;
}

inline void ReplaceInNode(
	NBT_Node &node,
	ReplaceSection sec,
	const QHash<QString, QString> &map,
	bool doPalette,
	bool doContainer,
	bool doEntity,
	int &changed)
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
		const QString id = NbtStringToQ(*pStr);
		if (id.isEmpty() || !map.contains(id))
		{
			return;
		}
		bool allow = false;
		switch (sec)
		{
		case ReplaceSection::Palette: allow = doPalette; break;
		case ReplaceSection::Container: allow = doContainer; break;
		case ReplaceSection::Entity: allow = doEntity; break;
		default: allow = doPalette && doContainer && doEntity; break;
		}
		if (!allow)
		{
			return;
		}
		if (AssignNbtString(*const_cast<NBT_Node *>(&node), map.value(id)))
		{
			++changed;
		}
		return;
	}
	case NBT_TAG::List:
	{
		auto *pList = node.GetIfList();
		if (pList == nullptr)
		{
			return;
		}
		for (auto &it : *pList)
		{
			ReplaceInNode(it, sec, map, doPalette, doContainer, doEntity, changed);
		}
		return;
	}
	case NBT_TAG::Compound:
	{
		auto *pCpd = node.GetIfCompound();
		if (pCpd == nullptr)
		{
			return;
		}
		for (auto &kv : *pCpd)
		{
			ReplaceSection childSec = sec;
			const ReplaceSection keySec = SectionFromCompoundKey(kv.first);
			if (keySec != ReplaceSection::Other)
			{
				childSec = keySec;
			}
			ReplaceInNode(kv.second, childSec, map, doPalette, doContainer, doEntity, changed);
		}
		return;
	}
	default:
		return;
	}
}

inline bool ApplyIdReplacementsToLitematic(
	const QString &litematicPath,
	const QHash<QString, QString> &replaceMap,
	bool doPalette,
	bool doContainer,
	bool doEntity,
	QString &errMsg,
	int &changedCount)
{
	changedCount = 0;
	if (replaceMap.isEmpty())
	{
		errMsg = QStringLiteral("没有要应用的替换规则");
		return false;
	}

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

	for (auto &kv : root)
	{
		ReplaceInNode(kv.second, ReplaceSection::Other, replaceMap, doPalette, doContainer, doEntity, changedCount);
	}
	if (changedCount == 0)
	{
		errMsg = QStringLiteral("未匹配到任何要替换的 id（检查作用范围或替换表）");
		return false;
	}

	std::vector<uint8_t> outData{};
	if (!NBT_Writer::WriteNBT(outData, 0, root))
	{
		errMsg = QStringLiteral("替换后无法写出 NBT 数据");
		return false;
	}
	std::vector<uint8_t> outCompressed{};
	if (!NBT_IO::CompressDataNoThrow(outCompressed, outData))
	{
		errMsg = QStringLiteral("替换后无法压缩数据流");
		return false;
	}
	if (!NBT_IO::WriteFile(fsPath, outCompressed))
	{
		errMsg = QStringLiteral("无法写入文件（可能被占用）：%1").arg(litematicPath);
		return false;
	}
	return true;
}
