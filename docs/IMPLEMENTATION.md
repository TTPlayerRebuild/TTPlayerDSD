# DSD 插件实现与验收

2026-10-23，按 `rebuild/docs/AUDIO_PLUGIN_DEVELOPMENT_GUIDE.md` 创建独立项目。

## 数据路径和接口

沿用原版 AddIn / Reader / Metadata / Thumbnail ABI，ordinal 1 导出，不修改任何旧虚表。Probe/SourceInfo 为独立可选接口；DSF、DSDIFF 分别报告 `.dsf`、`.dff`，不因 PCM 输出建议改名为 WAV。

DSF/DSDIFF → DSD 或 DST 解码 → FIR 重采样 → 176.4 kHz（44.1 系列）或 192 kHz（48 系列）PCM → 宿主均衡器/回放增益/输出链。Open 的浮点标志控制 float64，否则提供 PCM24。DSD 本身位深报告为 1，PCM 输出位深通过 Format 单独报告。

DSF 按头部声明样本数裁剪，避免最后一个 4096 字节分声道块的填充被当作音频；解码输入帧数单独核对，输出重采样帧数按最近整数取整。DSD256 的分数尾帧不可用向上取整后误判为截断。文件声明长度和实际长度不符、I/O 失败均返回错误。

保持标准 WAVE 声道顺序和掩码。无法确定的自定义 DSDIFF 声道顺序返回不支持，不猜测映射。定位重放历史以保持 DSD FIR 和重采样相位，向后跳转远处的代价较高。尚未实现稀疏索引和安全预卷优化。

DSDIFF 未压缩流的时长根据 DSD 数据块字节数和声道数计算，不把标签/封面体积算进音频时长。已知长度文件检查 FRM8/DSF 声明范围，短读持续累积，真正的 STG_E_READFAULT 原样返回。

读取标签/附图，不提供标签或图片写回。DSDIFF 顶层 ID3 变体、SACD ISO、原生 DSD 和 DoP 不在本实现范围。

## 验收

私有记录：`rebuild/tests/wav_feedback_20261023/`，不提交到本仓库或 Action。

- 自动生成 DSF DSD64/128/256；DSD64 2/6 声道、LSB/MSB，尾块非整除；DSDIFF 2/6 声道未压缩 DSD。
- float64 / PCM24、停止与恢复、容量不足、非整帧容量、缓冲哨兵、EOF、前后定位、截断文件。
- 中文 ID3 标签、PNG 附图；只读能力位和规范 IUnknown 身份。
- 参考 FFmpeg CLI 的同参数重采样，输出长度相同，浮点差异在约 `5.96e-8` 内；PCM24 误差不超过半个最低有效位。
- XP / Win7 实际加载并解码六声道 DSF，与 Win11 输出 SHA-256 完全相同。
- 原版 5.7.9 / 重建版隔离目录启动、插件加载、无错误对话框及正常退出 smoke 通过。未把此测试当作原版宿主输出 PCM 逐样本对照。
- 长 DSF 在两个宿主中实际推进超过 3 秒；7 字节短读流、DSF 非 seek 流、注入读取失败另行通过。非 seek DSF 不报告可定位能力，可能无法预先报告时长或读取尾部标签。

### 原版宿主接口补验（2026-10-23）

用原版 5.7.9 EXE 和原 ttpcomm / ttpres，在隔离目录经私有转发探针调用发行插件的原始字节。原版实际调用 Open(3) → Format → Read：

- DSF：176400 Hz / 双声道 / float64 / WAVEFORMATEXTENSIBLE，398 次 Read，共 13,041,136 字节，播放进度 3611 ms。
- DFF：相同 PCM 输出格式，204 次 Read，共 13,368,816 字节，播放进度 3741 ms。
- 两者返回的浮点值均有限，峰值约 0.130042；没有 Reader 错误或宿主错误窗口，正常关闭。

因此原版能通过旧 Reader 接口接收 DSD 转换后的 PCM；不依赖新增 Probe/SourceInfo 或新版 ttpcomm 音频服务。原版识别 EXTENSIBLE IEEE_FLOAT（`004B0999`），64 位浮点可直接进入后续处理（`004B0D1A / 004B1375`）。这证明实际宿主读取，不只是 DLL 加载成功。

测试使用静音和原版默认 16 位输出设置，未采集声卡端数据。插件返回 float64 不等于原版以 64 位或原生 DSD 输出；启用 Winamp 音效时，原版 `004AC166` 仍转换为 PCM16。原生 DSD、DoP 和重建版新高精度处理模式不会因此自动出现在原版中。证据在私有 `rebuild/tests/wav_feedback_20261023/original-chain-results.json`。

DST、48 kHz 系列、更高倍率、非标准声道顺序和真实高采样率硬件仍需补样本验证；原版在 XP/Win7 的完整 GUI 输出链尚未在本轮补验（此前已验证 DLL 的旧 ABI 解码）；Win10 未独立运行，按用户要求由 Win11 承担现代系统验证。
