#include "LitematicFileConversion.h"
#include "util/CodeTimer.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <filesystem>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

#ifdef _WIN32
static std::vector<std::filesystem::path> GetArgsFromWideCommandLine()
{
	std::vector<std::filesystem::path> files;
	int argc = 0;
	LPWSTR *argvW = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (!argvW)
	{
		return files;
	}
	for (int i = 1; i < argc; ++i)
	{
		files.emplace_back(argvW[i]);
	}
	LocalFree(argvW);
	return files;
}
#endif

int main(int argc, char *argv[])
{
#ifdef _WIN32
	// Prefer wide command line so non-ASCII paths work.
	auto files = GetArgsFromWideCommandLine();
#else
	std::vector<std::filesystem::path> files;
	for (int i = 1; i < argc; ++i)
	{
		files.emplace_back(argv[i]);
	}
#endif

	printf("共收到 [%zu] 个文件\n", files.size());

	int iSuccess = 0;
	CodeTimer t;
	for (size_t i = 0; i < files.size(); ++i)
	{
		try
		{
			printf("\n[%zu]: [%s]\n", i + 1, files[i].string().c_str());
		}
		catch (...)
		{
			printf("\n[%zu]: <路径无法以窄字符显示>\n", i + 1);
		}
		t.Start();
		LitematicConvertResult result{};
		try
		{
			result = ConvertLitematicFile_V7_To_V6(files[i]);
		}
		catch (const std::exception &e)
		{
			result.success = false;
			result.errorMessage = std::string("捕获异常：") + e.what();
			printf("%s\n", result.errorMessage.c_str());
		}
		catch (...)
		{
			result.success = false;
			result.errorMessage = "捕获未知异常";
			printf("%s\n", result.errorMessage.c_str());
		}
		t.Stop();
		if (result.success)
		{
			++iSuccess;
			try
			{
				printf("输出文件：%s\n", result.outputPath.string().c_str());
			}
			catch (...)
			{
				printf("输出文件：<路径无法以窄字符显示>\n");
			}
		}
		else
		{
			printf("失败原因：%s\n", result.errorMessage.c_str());
		}
		t.PrintElapsed("用时：[", "]\n");
	}

	printf("\n总计：[%zu]，成功：[%d]，失败：[%zu]\n\n", files.size(), iSuccess, files.size() - (size_t)iSuccess);

#ifdef _WIN32
	system("pause");
#endif

	return 0;
}
