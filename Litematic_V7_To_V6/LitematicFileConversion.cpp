#include "LitematicConversion.hpp"
#include "LitematicFileConversion.h"
#include "util/CodeTimer.hpp"

#include <filesystem>
#include <stdio.h>
#include <stdint.h>
#include <vector>
#include <unordered_map>
#include <format>

//找到一个唯一文件名（全程使用 filesystem::path，避免中文路径编码问题）
static std::filesystem::path GenerateUniqueFilename(const std::filesystem::path &dir,
	const std::filesystem::path &stemPrefix,
	const std::filesystem::path &extension,
	uint32_t u32TryCount = 10)//默认最多重试10次
{
	while (u32TryCount != 0)
	{
		// 时间用[]包围；用 path 拼接，兼容 Windows(wchar) 与 POSIX(char)
		auto fileName = stemPrefix;
		fileName += std::filesystem::path("[" + std::to_string(CodeTimer::GetSystemTime()) + "]");
		fileName += extension;

		auto tmpPath = dir / fileName;
		if (!NBT_IO::IsFileExist(tmpPath))
		{
			return tmpPath;
		}

		//等几ms在继续
		CodeTimer::Sleep(std::chrono::milliseconds(10));
		--u32TryCount;
	}

	//次数到上限直接返回空
	return std::filesystem::path{};
}

struct MyCompoundSort
{
	static inline uint8_t u8Enabled;

	static void Reset()
	{
		u8Enabled = 2;
	}

	/// @brief 对给定的 Compound 对象进行排序，返回指向其元素的迭代器向量。
	/// @param cpdSort 需要排序的 Compound 对象。
	/// @return `std::vector<NBT_Type::Compound::Const_Iterator>`，其中迭代器按排序顺序排列。
	std::vector<NBT_Type::Compound::Const_Iterator> operator()(const NBT_Type::Compound &cpdSort)
	{
		if (u8Enabled == 0 || u8Enabled-- > 1)
		{
			return cpdSort.KeySortIt<>();
		}

		//第二层使用自定义排序
		std::vector<NBT_Type::Compound::Const_Iterator> vSortCompound{};
		vSortCompound.reserve(cpdSort.Size());
		for (auto it = cpdSort.begin(), end = cpdSort.end(); it != end; ++it)
		{
			vSortCompound.push_back(it);
		}

		std::sort(vSortCompound.begin(), vSortCompound.end(),
			[](const auto &l, const auto &r) -> bool
			{
				static std::unordered_map<NBT_Type::String, uint64_t> mapPriority =
				{
					{MU8STR("MinecraftDataVersion"),	0},
					{MU8STR("Version"),					1},
					{MU8STR("SubVersion"),				2},
					{MU8STR("Metadata"),				3},
					{MU8STR("Regions"),					4},
				};

				auto itL = mapPriority.find(l->first);
				auto itR = mapPriority.find(r->first);

				uint64_t u64LPriority = itL == mapPriority.end() ? (uint64_t)-1 : itL->second;
				uint64_t u64RPriority = itR == mapPriority.end() ? (uint64_t)-1 : itR->second;

				if (u64LPriority != u64RPriority)//都没找到才不成立
				{
					return u64LPriority < u64RPriority;
				}
				else
				{
					return l->first < r->first;
				}
			}
		);

		return vSortCompound;
	}
};

static LitematicConvertResult Fail(std::string message)
{
	// 不再 printf：失败原因由调用方（CLI/GUI）按各自编码安全地展示
	return LitematicConvertResult{false, std::move(message), {}};
}

LitematicConvertResult ConvertLitematicFile_V7_To_V6(const std::filesystem::path &sV7FilePath)
{
	NBT_Type::Compound cpdV7Input{};
	NBT_Type::Compound cpdV6Output{};

	//从sV7FilePath读取到cpdV7Input
	{
		std::vector<uint8_t> vFileV7Stream{};
		if (!NBT_IO::ReadFile(sV7FilePath, vFileV7Stream))
		{
			return Fail("无法读取文件内容。请确认路径有效、文件未被占用，且是常规文件。");
		}

		//如果解压失败那么可能原先文件未压缩
		std::vector<uint8_t> vDataV7Stream{};
		const bool bDecompressOk = NBT_IO::DecompressDataNoThrow(vDataV7Stream, vFileV7Stream);
		if (!bDecompressOk)
		{
			vDataV7Stream = std::move(vFileV7Stream);//尝试以未压缩流处理，而不是失败
		}

		if (!NBT_Reader::ReadNBT(vDataV7Stream, 0, cpdV7Input))
		{
			if (!bDecompressOk)
			{
				return Fail("无法解析 NBT 数据。文件可能未压缩且已损坏，或不是有效的 Litematica 投影文件。");
			}
			return Fail("无法解析 NBT 数据。文件可能已损坏，或不是有效的 Litematica 投影文件。");
		}
	}

	//从cpdV7Input转换到cpdV6Output
	std::string strErrMsg;
	if (!ConvertLitematicData_V7_To_V6(cpdV7Input, cpdV6Output, strErrMsg))
	{
		return Fail("版本数据转换失败：" + strErrMsg);
	}

	//写出cpdV6Output到文件sV6FilePath
	{
		std::vector<uint8_t> vDataV6Stream{};
		MyCompoundSort::Reset();
		if (!NBT_Writer::WriteNBT<MyCompoundSort>(vDataV6Stream, 0, cpdV6Output))
		{
			return Fail("无法把转换结果写入内存数据流。");
		}

		//查找合法文件
		std::filesystem::path sV6FilePath{};
		{
			const auto dir = sV7FilePath.parent_path();
			auto stem = sV7FilePath.stem();
			stem += std::filesystem::path("_V6_");
			sV6FilePath = GenerateUniqueFilename(dir, stem, sV7FilePath.extension());
			if (sV6FilePath.empty())
			{
				return Fail("无法生成可用的输出文件名。可能是目录权限不足，或同名结果文件过多。");
			}
		}

		//压缩数据
		std::vector<uint8_t> vFileV6Stream{};
		if (!NBT_IO::CompressDataNoThrow(vFileV6Stream, vDataV6Stream))
		{
			return Fail("无法压缩输出数据流。");
		}

		//写入数据
		if (!NBT_IO::WriteFile(sV6FilePath, vFileV6Stream))
		{
			return Fail("无法写入输出文件。请检查目录是否可写、磁盘空间是否充足。");
		}

		return LitematicConvertResult{true, {}, sV6FilePath};
	}
}
