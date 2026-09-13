#pragma once

#include <filesystem>
#include <string>

struct LitematicConvertResult
{
	bool success = false;
	// 失败原因（中文）。成功时为空。
	std::string errorMessage;
	// 成功时的输出文件路径；失败时为空。
	std::filesystem::path outputPath;
};

// 接受原生路径（含中文），返回转换结果与失败原因
LitematicConvertResult ConvertLitematicFile_V7_To_V6(const std::filesystem::path &sV7FilePath);
