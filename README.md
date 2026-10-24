# ttp_dsd

TTPlayer 原版 AddIn ABI 的独立 x86 DSD 解码插件。面向原版播放器及重建版；同一 DLL 使用 VC-LTL / YY-Thunks 兼容 XP SP3、Win7 和现代 Windows，CPU 要求 SSE2。

## 功能

- DSF（`.dsf`）与 DSDIFF（`.dff`、`.dsdiff`），包含后端的 DST 解压分支。
- 裁剪、静态链接 FFmpeg 9.0.2 DSD / DST 解码器及 libswresample，无额外编解码 DLL。
- 44.1 kHz 系列 DSD 转换为 176.4 kHz PCM，48 kHz 系列转换为 192 kHz PCM；FIR 抗混叠后按 Open 标志返回 float64 或 24-bit PCM。保留标准声道布局。
- DSF 使用实际样本数排除块尾填充；流式 DSF 和 DST 声明帧数均核对解码长度。修复 DST 不压缩帧的排列及短帧检查。
- 普通 DSF／未压缩 DSDIFF 支持按数据位置计算的准确快速定位，预卷恢复滤波器历史；DST 继续顺序解码定位。
- 统一音轨号、专辑艺术家和歌词字段；读取 DSDIFF 顶层／PROP 内 ID3、DIIN，以及 JPEG、PNG、WebP 附图，保留封面类型和描述。
- 可写 IStream 上支持 ID3v2.3/v2.4 标签和封面修改、删除、保存及失败恢复。原版在最后释放时保存；重建版可查询独立 MetadataCommit 接口取得真实结果。复杂 ID3 标志、混合 ID3 版本等不能安全改写的文件保留只读回退。

这是 **DSD 转 PCM** 路径，可进入现有均衡器、回放增益及输出设备。不是原生 DSD / DoP 输出；不支持 SACD ISO。允许 44.1／48 kHz 基频的 DSD64～1024；生成样本覆盖高倍率边界，真实压缩 DST、部分分段形式及真实高采样率硬件仍有验收限制，详见实现文档。解码后端先产生 float32，再以 float64 重采样和输出，不宣称全程双精度。

## 构建与发行

现代 Visual Studio C++ x86 工具链、CMake、Python 3.12+、Git for Windows：

```powershell
./build.ps1 -Package -SourcePackage -PackageVersion 2026.10.23p1
```

首次构建按固定版本及 SHA-256 下载依赖。输出在 `build/Release/`。运行包仅含 `AddIn/ttp_dsd.dll` 和 `SHA256SUMS.txt`；完整后端源代码作为独立源码附件提供。许可证保存在仓库和源码附件，不嵌入 DLL。

Action 独立构建，**Release a Version** 使用北京时间日期版本。初版测试保存在工作区 `rebuild/tests/wav_feedback_20261023/`，本次补全测试在 `rebuild/tests/dsd_complete_20261023/`，不上传、不在 Action 执行、不进发行包。播放器的 `CodecAddIn-Rebuild` 已登记 [TTPlayerDSD](https://github.com/TTPlayerRebuild/TTPlayerDSD)，发布播放器时下载本仓库最新正式版的 `ttp_dsd-版本.zip`，将 `AddIn/ttp_dsd.dll` 合入合集。2026-10-23 核对时尚无正式 Release，需先发布本插件的运行包；仅有源码或 Git tag 不能用于合集下载。

详见 [实现及验证](docs/IMPLEMENTATION.md)、[依赖](docs/DEPENDENCIES.md)。
