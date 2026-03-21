# Ohos

## 设计文档（对标 Android 插件）

- **概要设计**：[docs/DESIGN-OVERVIEW.md](docs/DESIGN-OVERVIEW.md)（目标、原则、总体架构、模块一览）  
- **详细设计索引**：[docs/README.md](docs/README.md)（分文件：Android 映射、架构、子模块、构建/部署/运行、路线图）  

详细说明按主题拆成多个 `docs/DESIGN-DETAIL-*.md`，避免单文件过长。

## Qt Creator version

**Minimum Qt Creator 19** (`QtTaskTree` baseline); **20+** supported via the same tree using version macros / optional sources. Details: **[VERSIONING.md](VERSIONING.md)**.

## How to Build

Create a build directory and run

    cmake -DCMAKE_PREFIX_PATH=<path_to_qtcreator> -DCMAKE_BUILD_TYPE=RelWithDebInfo <path_to_plugin_source>
    cmake --build .

where `<path_to_qtcreator>` is the relative or absolute path to a Qt Creator build directory, or to a
combined binary and development package (Windows / Linux), or to the `Qt Creator.app/Contents/Resources/`
directory of a combined binary and development package (macOS), and `<path_to_plugin_source>` is the
relative or absolute path to this plugin directory.

## How to Run

Run a compatible Qt Creator with the additional command line argument

    -pluginpath <path_to_plugin>

where `<path_to_plugin>` is the path to the resulting plugin library in the build directory
(`<plugin_build>/lib/qtcreator/plugins` on Windows and Linux,
`<plugin_build>/Qt Creator.app/Contents/PlugIns` on macOS).

You might want to add `-temporarycleansettings` (or `-tcs`) to ensure that the opened Qt Creator
instance cannot mess with your user-global Qt Creator settings.

When building and running the plugin from Qt Creator, you can use

    -pluginpath "%{buildDir}/lib/qtcreator/plugins" -tcs

on Windows and Linux, or

    -pluginpath "%{buildDir}/Qt Creator.app/Contents/PlugIns" -tcs

for the `Command line arguments` field in the run settings.
