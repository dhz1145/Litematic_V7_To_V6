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

// ItemList 资源类别（itemlister/2）
enum class IdKind : uint8_t
{
	Item = 0,
	Block = 1,
	BlockEntity = 2,
	Entity = 3,
	Fluid = 4,
	Other = 5,
	KindCount = 6,
};

inline const char *IdKindName(IdKind k)
{
	switch (k)
	{
	case IdKind::Item: return "物品";
	case IdKind::Block: return "方块";
	case IdKind::BlockEntity: return "方块实体";
	case IdKind::Entity: return "实体";
	case IdKind::Fluid: return "流体";
	default: return "未知";
	}
}

// itemlister/2：按类别的 id 集合
struct AlphaItemSet
{
	QString path;
	QString format;      // 例如 itemlister/2
	QVector<QSet<QString>> idsByKind; // 下标为 IdKind

	AlphaItemSet()
	{
		idsByKind.resize(static_cast<int>(IdKind::KindCount));
	}

	bool loaded() const
	{
		for (const QSet<QString> &s : idsByKind)
		{
			if (!s.isEmpty())
			{
				return true;
			}
		}
		return false;
	}

	const QSet<QString> &idsOf(IdKind k) const
	{
		return idsByKind.at(static_cast<int>(k));
	}

	int totalCount() const
	{
		int n = 0;
		for (const QSet<QString> &s : idsByKind)
		{
			n += s.size();
		}
		return n;
	}

	// 用于「替换为」下拉：某类别的全部候选
	QSet<QString> allOf(IdKind k) const
	{
		return idsOf(k);
	}
};

struct ResourceIdCount
{
	QString id;
	int count = 0;
	IdKind kind = IdKind::Item;
};

// 投影 NBT 中某 id 在各区域的次数
struct SectionIdCounts
{
	int palette = 0;     // BlockStatePalette -> Block
	int container = 0;   // TileEntities 内物品 / 容器 -> Item
	int entity = 0;      // Entities 内物品 -> Item
	int blockEntity = 0; // TileEntities[].Id -> BlockEntity
	int entityType = 0;  // Entities[].Id -> Entity
	int fluid = 0;       // PendingFluidTicks -> Fluid
	int blockTick = 0;   // PendingBlockTicks -> Block
	int other = 0;       // 其它位置的裸 id

	// 按类别取属于该类的总次数（不含 other）
	int countOfKind(IdKind k) const
	{
		switch (k)
		{
		case IdKind::Item: return container + entity;
		case IdKind::Block: return palette + blockTick;
		case IdKind::BlockEntity: return blockEntity;
		case IdKind::Entity: return entityType;
		case IdKind::Fluid: return fluid;
		case IdKind::Other: return other;
		default: return 0;
		}
	}

	int totalAll() const
	{
		return palette + container + entity + blockEntity + entityType + fluid + blockTick + other;
	}
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

// 只支持 itemlister/2；五个数组分别可选，但至少要有 items
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
		errMsg = QStringLiteral("ItemList JSON 解析失败：%1").arg(perr.errorString());
		return false;
	}
	const QJsonObject root = doc.object();
	const QString fmt = root.value(QStringLiteral("format")).toString();
	if (fmt != QLatin1String("itemlister/2"))
	{
		errMsg = QStringLiteral("不支持的 ItemList 格式「%1」，需要 itemlister/2").arg(fmt);
		return false;
	}

	AlphaItemSet loaded;
	loaded.path = path;
	loaded.format = fmt;

	struct Field
	{
		const char *key;
		IdKind kind;
	};
	const Field fields[] = {
		{"items", IdKind::Item},
		{"blocks", IdKind::Block},
		{"blockEntities", IdKind::BlockEntity},
		{"entities", IdKind::Entity},
		{"fluids", IdKind::Fluid},
	};

	for (const Field &fd : fields)
	{
		const QJsonValue v = root.value(QString::fromLatin1(fd.key));
		if (v.isUndefined() || v.isNull())
		{
			continue;
		}
		if (!v.isArray())
		{
			errMsg = QStringLiteral("ItemList 字段 %1 不是数组").arg(QString::fromLatin1(fd.key));
			return false;
		}
		QSet<QString> &set = loaded.idsByKind[static_cast<int>(fd.kind)];
		const QJsonArray arr = v.toArray();
		set.reserve(arr.size());
		for (const QJsonValue &item : arr)
		{
			if (!item.isString())
			{
				continue;
			}
			const QString id = item.toString().trimmed();
			if (!id.isEmpty())
			{
				set.insert(id);
			}
		}
	}

	if (!loaded.loaded())
	{
		errMsg = QStringLiteral("ItemList 中没有有效 id");
		return false;
	}

	out = loaded;
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

// 各类别在 NBT 中的「容器键」→ 区域类型
enum class ScanRegion : uint8_t
{
	Other,
	Palette,          // BlockStatePalette -> Block
	TileEntities,     // 条目 id -> BlockEntity；条目内物品 -> Item
	Entities,         // 条目 id -> Entity；条目内物品 -> Item
	TileEntityItem,   // TileEntities 内的物品栈条目
	EntityItem,       // Entities 内的物品栈条目
	BlockTicks,       // -> Block
	FluidTicks,       // -> Fluid
};

inline ScanRegion RegionFromKey(const NBT_Type::String &key)
{
	const QString k = NbtStringToQ(key);
	if (k == QLatin1String("BlockStatePalette"))
	{
		return ScanRegion::Palette;
	}
	if (k == QLatin1String("TileEntities"))
	{
		return ScanRegion::TileEntities;
	}
	if (k == QLatin1String("Entities"))
	{
		return ScanRegion::Entities;
	}
	if (k == QLatin1String("PendingBlockTicks"))
	{
		return ScanRegion::BlockTicks;
	}
	if (k == QLatin1String("PendingFluidTicks"))
	{
		return ScanRegion::FluidTicks;
	}
	return ScanRegion::Other;
}

// 该键名是否是「物品栈/物品列表」容器键
inline bool IsItemContainerKey(const QString &k)
{
	return k == QLatin1String("Item") || k == QLatin1String("Items") ||
		k == QLatin1String("Inventory") || k == QLatin1String("sherds") ||
		k == QLatin1String("ArmorItems") || k == QLatin1String("HandItems") ||
		k == QLatin1String("OffhandItem") || k == QLatin1String("SaddleItem");
}

// 该键名是否是「资源 id 字段」（大小写都算）
inline bool IsIdKey(const QString &k)
{
	return k == QLatin1String("id") || k == QLatin1String("Id");
}

// 「条目」是方块实体/实体（有坐标），还是物品栈（有 Count/Slot）
inline bool CompoundLooksLikeEntityOrBlockEntity(const NBT_Type::Compound &cpd)
{
	for (const auto &kv : cpd)
	{
		const QString k = NbtStringToQ(kv.first);
		if (k == QLatin1String("x") || k == QLatin1String("y") || k == QLatin1String("z"))
		{
			return true;
		}
	}
	return false;
}

inline bool CompoundLooksLikeItemStack(const NBT_Type::Compound &cpd)
{
	for (const auto &kv : cpd)
	{
		const QString k = NbtStringToQ(kv.first);
		if (k == QLatin1String("Count") || k == QLatin1String("Slot") ||
			k == QLatin1String("components") || k == QLatin1String("tag"))
		{
			return true;
		}
	}
	return false;
}

// 在区域语义下记录一个字符串 id
inline void RecordId(
	const QString &id,
	ScanRegion region,
	const QString &parentKey,
	QHash<QString, SectionIdCounts> &counts)
{
	if (!LooksLikeResourceId(id))
	{
		return;
	}
	SectionIdCounts &sc = counts[id];
	switch (region)
	{
	case ScanRegion::Palette:
		sc.palette += 1;
		break;
	case ScanRegion::BlockTicks:
		if (parentKey == QLatin1String("block") || parentKey.isEmpty())
		{
			sc.blockTick += 1;
		}
		else
		{
			sc.other += 1;
		}
		break;
	case ScanRegion::FluidTicks:
		if (parentKey == QLatin1String("fluid") || parentKey.isEmpty())
		{
			sc.fluid += 1;
		}
		else
		{
			sc.other += 1;
		}
		break;
	case ScanRegion::TileEntities:
		if (IsIdKey(parentKey))
		{
			// 条目自身的 id -> 方块实体类型
			sc.blockEntity += 1;
		}
		else if (IsItemContainerKey(parentKey))
		{
			// 方块实体内嵌物品列表（sherds / Items 等）-> 物品
			sc.container += 1;
		}
		else
		{
			sc.other += 1;
		}
		break;
	case ScanRegion::Entities:
		if (IsIdKey(parentKey))
		{
			// 条目自身的 id -> 实体类型
			sc.entityType += 1;
		}
		else if (IsItemContainerKey(parentKey))
		{
			sc.entity += 1;
		}
		else
		{
			sc.other += 1;
		}
		break;
	case ScanRegion::TileEntityItem:
	case ScanRegion::EntityItem:
		// 物品栈 / 物品列表里的值 -> 物品
		if (IsIdKey(parentKey) || IsItemContainerKey(parentKey) || parentKey.isEmpty())
		{
			sc.container += 1;
		}
		else
		{
			sc.other += 1;
		}
		break;
	default:
		sc.other += 1;
		break;
	}
}

inline void ScanNode(
	const NBT_Node &node,
	ScanRegion region,
	const QString &parentKey,
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
		RecordId(NbtStringToQ(*pStr), region, parentKey, counts);
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
			ScanNode(it, region, parentKey, counts);
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

		// 条目级判别：Entities/TileEntities 下的条目可能是实体/方块实体，也可能是物品栈
		ScanRegion selfRegion = region;
		if (region == ScanRegion::TileEntities)
		{
			selfRegion = CompoundLooksLikeEntityOrBlockEntity(*pCpd)
				? ScanRegion::TileEntities
				: ScanRegion::TileEntityItem;
		}
		else if (region == ScanRegion::Entities)
		{
			selfRegion = CompoundLooksLikeEntityOrBlockEntity(*pCpd)
				? ScanRegion::Entities
				: ScanRegion::EntityItem;
		}
		else if (region == ScanRegion::TileEntityItem || region == ScanRegion::EntityItem)
		{
			// 物品栈条目的内嵌复合（如 components）仍是物品上下文
			selfRegion = region;
		}

		for (const auto &kv : *pCpd)
		{
			const QString key = NbtStringToQ(kv.first);
			ScanRegion childRegion = selfRegion;
			const ScanRegion keyRegion = RegionFromKey(kv.first);
			if (keyRegion != ScanRegion::Other)
			{
				childRegion = keyRegion;
			}
			// 物品栈/物品列表容器键 -> 物品上下文
			if (IsItemContainerKey(key) &&
				(keyRegion == ScanRegion::Other))
			{
				if (selfRegion == ScanRegion::Entities || selfRegion == ScanRegion::EntityItem)
				{
					childRegion = ScanRegion::EntityItem;
				}
				else
				{
					childRegion = ScanRegion::TileEntityItem;
				}
			}
			ScanNode(kv.second, childRegion, key, counts);
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
		ScanNode(kv.second, ScanRegion::Other, QString(), counts);
	}
	if (counts.isEmpty())
	{
		errMsg = QStringLiteral("未在文件中扫描到物品/方块 id");
		return false;
	}
	return true;
}

// 组合勾选后，某 id 归属的可用于比较的类别集合
inline QVector<IdKind> ActiveKinds(bool doPaletteItem, bool doContainerItem, bool doEntityItem,
	bool doBlockEntity, bool doEntityType, bool doFluid, bool doOther)
{
	QVector<IdKind> kinds;
	// 方块类：调色板勾选即视为要看方块
	if (doPaletteItem)
	{
		kinds.append(IdKind::Block);
	}
	// 物品类：容器 或 实体内物品 任一勾选
	if (doContainerItem || doEntityItem)
	{
		kinds.append(IdKind::Item);
	}
	if (doBlockEntity)
	{
		kinds.append(IdKind::BlockEntity);
	}
	if (doEntityType)
	{
		kinds.append(IdKind::Entity);
	}
	if (doFluid)
	{
		kinds.append(IdKind::Fluid);
	}
	if (doOther)
	{
		kinds.append(IdKind::Other);
	}
	return kinds;
}

// 某 id 在投影中的有效次数（按勾选的类别）
inline int ScopedCount(const SectionIdCounts &sc, const QVector<IdKind> &kinds)
{
	int n = 0;
	for (IdKind k : kinds)
	{
		n += sc.countOfKind(k);
	}
	return n;
}

// 判定缺失：该 id 所属的任一类别在 ItemList 中存在，即不算缺失
inline bool IsMissingId(const QString &id, const QVector<IdKind> &kinds, const AlphaItemSet &list)
{
	bool anyKindHas = false;
	bool anyKindChecked = false;
	for (IdKind k : kinds)
	{
		anyKindChecked = true;
		if (list.idsOf(k).contains(id))
		{
			anyKindHas = true;
			break;
		}
	}
	if (!anyKindChecked)
	{
		return false;
	}
	return !anyKindHas;
}

inline QVector<ResourceIdCount> SortCounts(const QVector<ResourceIdCount> &rows)
{
	QVector<ResourceIdCount> out = rows;
	std::sort(out.begin(), out.end(),
		[](const ResourceIdCount &a, const ResourceIdCount &b) {
			if (a.count != b.count)
			{
				return a.count > b.count;
			}
			return a.id < b.id;
		});
	return out;
}

// 补全排序：子串匹配 + 相关度（前缀 > 包含；更短优先；字典序）
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
			if (ap != bp)
			{
				return ap;
			}
			if (al.size() != bl.size())
			{
				return al.size() < bl.size();
			}
			return al < bl;
		});
	if (matched.size() > 300)
	{
		matched = matched.mid(0, 300);
	}
	return matched;
}

inline bool AssignNbtString(NBT_Type::String &str, const QString &utf8Text)
{
	try
	{
		const QByteArray raw = utf8Text.toUtf8();
		str = NBT_Type::String(std::basic_string_view<char>(raw.constData(), static_cast<size_t>(raw.size())));
		return true;
	}
	catch (...)
	{
		return false;
	}
}

// 替换：仅改与该 id 同类别的位置
inline void ReplaceInNode(
	NBT_Node &node,
	ScanRegion region,
	const QString &parentKey,
	const QHash<QString, QString> &map,
	const QHash<QString, IdKind> &kinds,
	bool doPaletteItem,
	bool doContainerItem,
	bool doEntityItem,
	bool doBlockEntity,
	bool doEntityType,
	bool doFluid,
	bool doOther,
	int &changed)
{
	const NBT_TAG tag = node.GetTag();
	switch (tag)
	{
	case NBT_TAG::String:
	{
		auto *pStr = node.GetIfString();
		if (pStr == nullptr)
		{
			return;
		}
		const QString id = NbtStringToQ(*pStr);
		if (id.isEmpty() || !map.contains(id))
		{
			return;
		}
		const IdKind kind = kinds.value(id, IdKind::Item);
		bool allow = false;
		switch (region)
		{
		case ScanRegion::Palette:
			allow = doPaletteItem && kind == IdKind::Block;
			break;
		case ScanRegion::BlockTicks:
			allow = doPaletteItem && kind == IdKind::Block &&
				(parentKey == QLatin1String("block") || parentKey.isEmpty());
			break;
		case ScanRegion::FluidTicks:
			allow = doFluid && kind == IdKind::Fluid &&
				(parentKey == QLatin1String("fluid") || parentKey.isEmpty());
			break;
		case ScanRegion::TileEntities:
			if (IsIdKey(parentKey))
			{
				allow = doBlockEntity && kind == IdKind::BlockEntity;
			}
			else if (IsItemContainerKey(parentKey))
			{
				allow = doContainerItem && kind == IdKind::Item;
			}
			break;
		case ScanRegion::Entities:
			if (IsIdKey(parentKey))
			{
				allow = doEntityType && kind == IdKind::Entity;
			}
			else if (IsItemContainerKey(parentKey))
			{
				allow = doEntityItem && kind == IdKind::Item;
			}
			break;
		case ScanRegion::TileEntityItem:
			allow = doContainerItem && kind == IdKind::Item &&
				(IsIdKey(parentKey) || IsItemContainerKey(parentKey) || parentKey.isEmpty());
			break;
		case ScanRegion::EntityItem:
			allow = doEntityItem && kind == IdKind::Item &&
				(IsIdKey(parentKey) || IsItemContainerKey(parentKey) || parentKey.isEmpty());
			break;
		case ScanRegion::Other:
			allow = doOther && kind == IdKind::Other;
			break;
		default:
			allow = false;
			break;
		}
		if (allow && AssignNbtString(*const_cast<NBT_Type::String *>(pStr), map.value(id)))
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
			ReplaceInNode(it, region, parentKey, map, kinds,
				doPaletteItem, doContainerItem, doEntityItem,
				doBlockEntity, doEntityType, doFluid, doOther, changed);
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
		ScanRegion selfRegion = region;
		if (region == ScanRegion::TileEntities)
		{
			selfRegion = CompoundLooksLikeEntityOrBlockEntity(*pCpd)
				? ScanRegion::TileEntities
				: ScanRegion::TileEntityItem;
		}
		else if (region == ScanRegion::Entities)
		{
			selfRegion = CompoundLooksLikeEntityOrBlockEntity(*pCpd)
				? ScanRegion::Entities
				: ScanRegion::EntityItem;
		}
		for (auto &kv : *pCpd)
		{
			const QString key = NbtStringToQ(kv.first);
			ScanRegion childRegion = selfRegion;
			const ScanRegion keyRegion = RegionFromKey(kv.first);
			if (keyRegion != ScanRegion::Other)
			{
				childRegion = keyRegion;
			}
			if (IsItemContainerKey(key) && keyRegion == ScanRegion::Other)
			{
				if (selfRegion == ScanRegion::Entities || selfRegion == ScanRegion::EntityItem)
				{
					childRegion = ScanRegion::EntityItem;
				}
				else
				{
					childRegion = ScanRegion::TileEntityItem;
				}
			}
			ReplaceInNode(kv.second, childRegion, key, map, kinds,
				doPaletteItem, doContainerItem, doEntityItem,
				doBlockEntity, doEntityType, doFluid, doOther, changed);
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
	const QHash<QString, IdKind> &idKinds,
	bool doPaletteItem,
	bool doContainerItem,
	bool doEntityItem,
	bool doBlockEntity,
	bool doEntityType,
	bool doFluid,
	bool doOther,
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
		ReplaceInNode(kv.second, ScanRegion::Other, QString(), replaceMap, idKinds,
			doPaletteItem, doContainerItem, doEntityItem,
			doBlockEntity, doEntityType, doFluid, doOther, changedCount);
	}
	if (changedCount == 0)
	{
		errMsg = QStringLiteral("未匹配到任何要替换的 id（检查类别勾选或替换表）");
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
