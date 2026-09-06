# Litematic_V7_To_V6
[![GitHub Releases](https://shields.io/github/v/release/chenjunfu2/Litematic_V7_To_V6)](https://github.com/chenjunfu2/Litematic_V7_To_V6/releases)
[![GitHub Releases downloads](https://shields.io/github/downloads/chenjunfu2/Litematic_V7_To_V6/total)](https://github.com/chenjunfu2/Litematic_V7_To_V6/releases)
[![GitHub Repo stars](https://shields.io/github/stars/chenjunfu2/Litematic_V7_To_V6)](https://github.com/chenjunfu2/Litematic_V7_To_V6/)  
本工具在尽可能 **保留方块、方块实体、实体等数据** 的情况下，以映射数据的方式降低投影原理图的数据版本(或称之为投影降级)。  
  
工具会将投影原理图 **从 V7(MC1.20.5+) 转换到 V6(MC1.20.4-)** ，使得旧版MC可以在尽量少数据损失的情况下打开新版本投影原理图。  
  
但由于不同 Minecraft / Litematica 版本之间存在数据格式差异，**不能保证所有新版本数据都能够在旧版本中完全保留或正常使用**。  
  
项目主要使用 C++ 编写，并支持跨平台构建。  
  
## 使用方法
### 输入方式
转换器可以一次性转换**一个或多个**投影原理图文件。  
  
**在windows系统中：**  
> 将一个或多个需要降低数据版本的投影文件，拖拽到程序上，然后松开。
>  
> *（当然你也可以在 Windows 中使用 Linux 的方式~）*
> 
> **特别注意，是拖拽投影文件到程序文件上，显示为用xxx（程序名称）打开，而不是启动程序后把投影文件拖拽到程序窗口里！**  
> **不要再因为这种奇怪的问题给我报程序不工作的BUG了！**  
  
**在linux系统与其它系统中：**  
> 将一个或多个需要降低数据版本的投影文件，作为启动命令参数传递，并执行。
> 
> - 单个文件
>   ```text
>   ./Litematic_V7_To_V6 <your_schematic>.litematic
>   ```
>   
> - 多个文件
>   ```text
>   ./Litematic_V7_To_V6 <your_schematic_1>.litematic <your_schematic_2>.litematic ...
>   ```

### 输出方式
生成的文件会保存在输入文件所在的同一目录中。  
**程序在任何情况下都不会覆盖原始文件，也不会覆盖任何已有的转换结果。**  
  
输出文件名格式为：  
> `<原文件名>_V6_[<时间戳(毫秒)>].<原扩展名>`
  
例如：  
> `example.litematic`
  
得到：
> `example\_V6\_\[1788703616542\].litematic`
  
**特别的：**  
程序会使用当前系统时间的毫秒级Unix时间戳生成新文件名，并检查生成的文件名是否已经存在。  
如果生成的文件名冲突，会等待一段时间后重新生成文件名并再次尝试，最多尝试 10 次，而后失败，  
如果发生此类失败情况，可以检查是否因为目录下有过多此类文件或更换干净目录重试转换。  
  
## 备注
**建议始终保留原始 Litematica 原理图文件，不要在获得转换结果后删除原文件。**  
如果未来程序修复了 Bug、改进了转换逻辑，保留原始文件可以重新进行转换，从而获得更好的转换结果。  
  
## 跨平台
本项目使用Github CI自动完成跨平台构建  
请在[Releases](../../releases)页面中下载所需平台的编译产物  

## 实现与依赖
本项目代码参考了[投影Mod](https://github.com/sakura-ryoko/litematica)的部分代码  
使用的NBT库为：[NBT_CPP](https://github.com/chenjunfu2/NBT_CPP/)  
其它库依赖：[zlib](https://github.com/madler/zlib)和[xxhash](https://github.com/Cyan4973/xxHash)  

## 1.20.x Mod版本
本项目还作为此Mod的JNI库被调用。  
详情请见：[litematica-extra](https://github.com/shuangshun/litematica-extra)  

------------------------------------------------------------------------------------------

# Litematic_V7_To_V6

[![GitHub Releases](https://shields.io/github/v/release/chenjunfu2/Litematic_V7_To_V6)](https://github.com/chenjunfu2/Litematic_V7_To_V6/releases)
[![GitHub Releases downloads](https://shields.io/github/downloads/chenjunfu2/Litematic_V7_To_V6/total)](https://github.com/chenjunfu2/Litematic_V7_To_V6/releases)
[![GitHub Repo stars](https://shields.io/github/stars/chenjunfu2/Litematic_V7_To_V6)](https://github.com/chenjunfu2/Litematic_V7_To_V6/)

This tool downgrades Litematica schematic data using a **data-mapping approach**, while attempting to **preserve blocks, block entities, entities, and other data** as much as possible.

It converts Litematica schematics **from V7 (MC 1.20.5+) to V6 (MC 1.20.4-)**, allowing older versions of Minecraft to open schematics created by newer versions while minimizing data loss.

However, due to differences in data formats between different Minecraft / Litematica versions, **there is no guarantee that all data from newer versions can be fully preserved or used correctly in older versions**.

The project is mainly written in C++ and supports cross-platform builds.

## Usage

### Input

The converter can process **one or multiple** Litematica schematic files at once.

**On Windows:**

> Drag and drop one or more schematic files onto the program and release them.
>
> *(You can also use the Linux-style command-line method on Windows~)*

**On Linux and other systems:**

> Pass one or more schematic files as command-line arguments when launching the program.
>
> * Single file:
>
>   ```text
>   ./Litematic_V7_To_V6 <your_schematic>.litematic
>   ```
>
> * Multiple files:
>
>   ```text
>   ./Litematic_V7_To_V6 <your_schematic_1>.litematic <your_schematic_2>.litematic ...
>   ```

### Output

Converted files are saved in the **same directory as the input files**.

**The program will never overwrite the original files or any previously generated conversion results.**

The output filename format is:

> `<original_filename>_V6_[<timestamp_in_milliseconds>].<original_extension>`

For example:

> `example.litematic`

becomes:

> `example_V6_[1788703616542].litematic`

**Filename generation:**

The program uses the current system time as a **millisecond-level Unix timestamp** to generate a new filename, and checks whether the generated filename already exists.

If a filename collision occurs, the program waits for a short period, generates a new filename, and retries. It will retry up to 10 times before failing.

If this happens, check whether the target directory contains too many similar conversion files, or try converting the file again in a clean directory.

## Notes

**It is strongly recommended to always keep the original Litematica schematic files instead of deleting them after conversion.**

If the program fixes bugs or improves its conversion logic in the future, keeping the original files allows you to convert them again and potentially obtain better results.

## Cross-Platform

This project uses GitHub CI to automatically perform cross-platform builds.

Please visit the [Releases](../../releases) page to download the appropriate build for your platform.

## Implementation and Dependencies

Parts of this project are based on code from [Litematica](https://github.com/sakura-ryoko/litematica).

NBT library:

* [NBT_CPP](https://github.com/chenjunfu2/NBT_CPP/)

Other dependencies:

* [zlib](https://github.com/madler/zlib)
* [xxHash](https://github.com/Cyan4973/xxHash)

## 1.20.x Mod Version

This project is also used as the JNI library for this mod.

For more information, see:

[litematica-extra](https://github.com/shuangshun/litematica-extra)

------------------------------------------------------------------------------------------

## Star History
[![Star History Chart](https://api.star-history.com/image?repos=chenjunfu2/Litematic_V7_To_V6&type=date&legend=top-left)](https://www.star-history.com/?repos=chenjunfu2%2FLitematic_V7_To_V6&type=date&legend=top-left)

