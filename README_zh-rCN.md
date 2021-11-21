# Path of Building Community SimpleGraphic.dll

## 简介

`SimpleGraphic.dll` 是一个 Lua 运行时。
它包含了应用程序 Lua 逻辑所使用的 API 以及一个 2D OpenGL 渲染器丶窗口管理丶输入处理和调试控制台。
导出的入口函数为 `RunLuaFileAsWin`，参数列表为 C 风格，脚本路径放在 `argv[0]`.

窗口功能有 5 个代码文件：
- `win\entry.cpp`：DLL 的导出，只有创建并运行系统主模块的功能
- `engine\system\win\sys_main.cpp`：系统主模块，初始化应用程序和包含通用的系统接口 如：输入和剪辑板处理
- `engine\system\win\sys_console.cpp`：管理调试控制台，在程序初始化的过程中显示调试控制台
- `engine\system\win\sys_video.cpp`：创建和管理主程序窗口
- `engine\system\win\sys_opengl.cpp`：初始化 OpenGL 上下文

## 编译

`SimpleGraphic.dll` 使用 Visual Studio 开发，支持 VS2017 + VC140 以上版本

该 DLL 依赖一些三方库，详细说明请参考 [DEPENDENCIES.md](DEPENDENCIES.md).

如果已经配置好所有三方库可按照以下步骤来编译 DLL： 
1) 在 VS 中打开 `SimpleGraphic.sln`
2) 选择 "Release_MT|x86" 配置
3) 生成解决方案
4) 文件 `SimpleGraphic.dll` 编译后输出在 `Release` 文件夹
5) 复制 `SimpleGraphic.dll` 到 Path of Building 安装目录并覆盖现有文件

## 调试

方法1： 
- 由于 SimpleGraphic.dll 是动态库，需要 `PathOfBuilding.exe` 调试它， 运行 `PathOfBuilding.exe` 后在 VS 中选择 "调试" > "附加到进程..." 进行调试
- 如果需要调试初始化代码则添加 `MessageBox` 或使用其它办法暂停运行给你充足的时间附加进程。

方法2： 
- 复制 Path of Building 程序到 `out` 目录下，在 VS 的 "属性" > "常规" > "输出目录" 填入程序文件夹名：`$(SolutionDir)\out\Path of Building\`，使用快捷键 F5 启动调试。

## Licence

[MIT](https://opensource.org/licenses/MIT)

For 3rd-party licences, see [LICENSE](LICENSE).
The licencing information is considered to be part of the documentation.