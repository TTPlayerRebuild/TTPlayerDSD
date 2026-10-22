# ttp_dsd

TTPlayer 原版 AddIn ABI 的独立 x86 DSD 解码插件。面向原版播放器及重建版；同一 DLL 使用 VC-LTL / YY-Thunks 兼容 XP SP3、Win7 和现代 Windows，CPU 要求 SSE2。

## 功能

- DSF（`.dsf`）与 DSDIFF（`.dff`、`.dsdiff`），包含后端的 DST 解压分支。
- 裁剪、静态链接 FFmpeg 9.0.2 DSD / DST 解码器及 libswresample，无额外编解码 DLL。
- 44.1 kHz 系列 DSD 转换为 176.4 kHz PCM，48 kHz 系列转换为 192 kHz PCM；FIR 抗混叠后按 Open 标志返回 float64 或 24-bit PCM。保留标准声道布局。
- DSF 使用实际样本数排除块尾填充，支持停止、读取容量检查、可靠错误返回及前后定位。
- 读取 FFmpeg 识别的 ID3 / DSDIFF 标签和 JPEG、PNG、WebP 附图。标签和图片只读，不声明保存能力。DSDIFF 的 PROP/ID3、DIIN 可读取；未扩展后端尚未识别的顶层 ID3 变体。

这是 **DSD 转 PCM** 路径，可进入现有均衡器、回放增益及输出设备。不是原生 DSD / DoP 输出；不支持 SACD ISO。已验收 DSF DSD64/128/256、DSDIFF 未压缩 DSD、立体声和标准六声道；DST、48 kHz 系列及更高 DSD 倍率仍须真实样本验收。

## 构建与发行

现代 Visual Studio C++ x86 工具链、CMake、Python 3.12+、Git for Windows：

```powershell
./build.ps1 -Package -SourcePackage -PackageVersion 2026.10.23
```

首次构建按固定版本及 SHA-256 下载依赖。输出在 `build/Release/`。运行包仅含 `AddIn/ttp_dsd.dll` 和 `SHA256SUMS.txt`；完整后端源代码作为独立源码附件提供。许可证保存在仓库和源码附件，不嵌入 DLL。

Action 独立构建，**Release a Version** 使用北京时间日期版本。测试保存在工作区 `rebuild/tests/wav_feedback_20261023/`，不上传、不在 Action 执行、不进发行包。尚未创建或发布远程仓库；在线 CodecAddIn 聚合需要先登记实际仓库及已发布资产。

详见 [实现及验证](docs/IMPLEMENTATION.md)、[依赖](docs/DEPENDENCIES.md)。
