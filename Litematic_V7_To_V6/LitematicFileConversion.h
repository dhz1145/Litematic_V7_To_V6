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

// outputDir 为空：输出到源文件同目录；否则输出到指定目录
LitematicConvertResult ConvertLitematicFile_V7_To_V6(
	const std::filesystem::path &sV7FilePath,
	const std::filesystem::path &outputDir = {});
