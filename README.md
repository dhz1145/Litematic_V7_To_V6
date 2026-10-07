# Litematic V7 → V6

[![Latest release](https://img.shields.io/github/v/release/dhz1145/Litematic_V7_To_V6)](https://github.com/dhz1145/Litematic_V7_To_V6/releases)
[![Release downloads](https://img.shields.io/github/downloads/dhz1145/Litematic_V7_To_V6/total)](https://github.com/dhz1145/Litematic_V7_To_V6/releases)
[![CI](https://github.com/dhz1145/Litematic_V7_To_V6/actions/workflows/c-cpp.yml/badge.svg)](https://github.com/dhz1145/Litematic_V7_To_V6/actions/workflows/c-cpp.yml)

一个离线的 Litematica 投影降级工具，将 **V7（Minecraft 1.20.5+）投影转换为 V6（Minecraft 1.20.4-）**。转换过程使用数据映射，尽量保留方块、方块实体、实体、容器物品、流体 tick 等数据，方便旧版本 Minecraft 打开新版本投影。

不同 Minecraft、Litematica 和模组版本之间存在数据格式差异，因此无法保证所有新版本数据都能在旧版本中完全保留或正常使用。程序不会覆盖原始投影文件，也不会覆盖已有的转换结果。

## 功能

- Windows 图形界面和跨平台命令行工具。
- 支持一次转换一个或多个 `.litematic` 文件。
- 输出安全写入输入文件所在目录，自动生成带毫秒时间戳的新文件名。
- GUI 支持加载 ItemList，转换后对比投影中缺失的资源 ID。
- ItemList 对比按 `items`、`blocks`、`blockEntities`、`entities`、`fluids` 分类显示，并支持分类筛选、候选替换和定向写回。

## ItemList 从哪里来

本项目使用的 ItemList JSON 由独立项目 [ItemLister](https://github.com/dhz1145/itemlister) 在 Minecraft 中导出。请先安装并运行 ItemLister，使用它的导出功能生成 JSON，再将该文件加载到本项目的 GUI 中。

本项目读取的是 ItemLister 的 `itemlister/2` 格式，包含以下资源类别：

- `items`：物品 ID
- `blocks`：方块 ID
- `blockEntities`：方块实体 ID
- `entities`：实体 ID
- `fluids`：流体 ID

ItemList 是外部导入的数据，本项目不会替代 ItemLister 生成它。若 ItemList 与 Minecraft 实例、模组版本不匹配，对比结果也会随之变化。

## Windows GUI 使用方法

1. 从 [Releases](https://github.com/dhz1145/Litematic_V7_To_V6/releases) 下载 Windows GUI 压缩包并解压。
2. 启动 `Litematic_GUI.exe`。
3. 勾选 **ItemList**，点击 **加载 ItemList**，选择由 ItemLister 导出的 JSON。
4. 点击 **添加文件**，选择一个或多个 `.litematic` 投影。
5. 点击 **开始转换**。
6. 转换完成后打开 ItemList 对比，查看缺失资源的类别、计数和筛选结果；需要时选择同类候选 ID 并应用替换。

默认输出目录是源文件所在目录，也可以在界面中选择其它输出目录。GUI 不会覆盖源文件。

## 命令行使用方法

Linux、macOS 和其它支持命令行的环境：

```text
./Litematic_V7_To_V6 <your_schematic>.litematic
```

也可以一次传入多个文件：

```text
./Litematic_V7_To_V6 <schematic_1>.litematic <schematic_2>.litematic
```

Windows 命令行同样支持上述参数方式；Windows 用户也可以直接将投影文件拖到程序图标上。

## 输出文件

输出文件会写入输入文件所在目录，命名格式为：

```text
<原文件名>_V6_[<毫秒时间戳>].litematic
```

例如：

```text
example.litematic
→ example_V6_[1788703616542].litematic
```

程序会检查目标文件名是否冲突，最多重试 10 次。建议始终保留原始投影，以便未来修复问题或改进转换逻辑后重新转换。

## 下载

预编译文件和每个版本的更新说明位于 [GitHub Releases](https://github.com/dhz1145/Litematic_V7_To_V6/releases)。GitHub Actions 会为 Windows、Linux、macOS 和 Android 构建对应产物；Windows GUI 目前提供 x64 构建。

## 从源码构建

项目使用 CMake。构建命令行版本：

```text
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

构建 Qt GUI 时启用 `BUILD_GUI`，并提供 Qt 6：

```text
cmake -S . -B build-gui -DBUILD_GUI=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-gui --target Litematic_GUI --config Release
```

## 实现与依赖

- 部分转换逻辑参考 [Litematica](https://github.com/sakura-ryoko/litematica)。
- NBT 库：[NBT_CPP](https://github.com/chenjunfu2/NBT_CPP/)。
- 其它依赖：[zlib](https://github.com/madler/zlib) 和 [xxHash](https://github.com/Cyan4973/xxHash)。
- 项目也可作为 [litematica-extra](https://github.com/shuangshun/litematica-extra) 的 JNI 库使用。

## English overview

Litematic V7 → V6 is an offline converter for Litematica schematics. It converts **V7 (Minecraft 1.20.5+)** data to **V6 (Minecraft 1.20.4-)** through data mappings and tries to preserve blocks, block entities, entities, inventories, fluid ticks, and other supported data.

The GUI can import `itemlister/2` JSON files, compare resource IDs found in a converted schematic, filter missing IDs by category, suggest replacements from the same category, and write selected replacements back to the output schematic.

Those ItemList files are exported in Minecraft by the separate [ItemLister project](https://github.com/dhz1145/itemlister). Generate the JSON with ItemLister first, then load it in the GUI. The converter itself does not generate ItemList files.

See [Releases](https://github.com/dhz1145/Litematic_V7_To_V6/releases) for prebuilt binaries.

---

## Star History

[![Star History Chart](https://api.star-history.com/image?repos=dhz1145/Litematic_V7_To_V6&type=date&legend=top-left)](https://www.star-history.com/?repos=dhz1145%2FLitematic_V7_To_V6&type=date&legend=top-left)
