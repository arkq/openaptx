# aptX Adaptive on a non-Qualcomm Linux host — 项目交接报告

**最后更新**：本轮会话结束
**当前状态**：传输层已与工作参考源逐字节对齐；音频仍无声，差异缩小到**帧头第 3 字节**

---

## 0. 一句话总结

我们把 aptX Adaptive 从"完全不通"推进到"传输层与手机参考源字节一致"，唯一剩余差异是
**编码器输出的帧头字节 2 几乎恒为 `0xd0`**，而工作正常的 Android 源在 `0xd0`–`0xd7` 之间变化。

---

## 1. 硬件与系统

| 组件 | 详情 |
|---|---|
| 主机 | Intel Core Ultra 7 255HX，30 GiB DDR5 |
| 系统 | NixOS 26.11pre，linux-zen 7.2.3 |
| 蓝牙适配器 | Intel AX210（`FC:B3:AA:C5:01:42`），无高通控制器 |
| 目标耳机 | Sennheiser MOMENTUM 5（`80:C3:BA:B7:16:3B`），SEP vendor `0x00d7` codec `0x00ad` |
| 参考源 1 | HONOR 90GT / Android 16（aptX Adaptive 可用）—— 手机蓝牙地址 `44:90:46:40:FD:DD` |
| 参考源 2 | FiiO BT11 / QCC5181（aptX Lossless 可用） |
| 编码器执行 | QEMU 11.1.0 `qemu-hexagon -cpu v68`，Hexagon clang 22.1.8 |
| 专有 blob | `/home/baizhu945/work/aptxlibs_CPH2749/`（**不进 Nix store**） |

---

## 2. 关键路径

```
helper 源码    /home/baizhu945/work/openaptx/research/aptx-adaptive-qemu/aptx-lossless-helper.c
helper 二进制  /home/baizhu945/Documents/aptx-adaptive-runtime/helper/aptx-lossless-helper
PipeWire 插件  /home/baizhu945/work/pipewire/spa/plugins/bluez5/a2dp-codec-aptx-adaptive.c
NixOS 模块     /etc/nixos/pipewire-aptx-adaptive-module.nix
               （可写副本 /home/baizhu945/work/nixos-aptx-fix/）
测试脚本       /home/baizhu945/work/openaptx/build-helper/test-adaptive.sh
               /home/baizhu945/work/openaptx/build-helper/test-helper.py
参考码流       /home/baizhu945/work/phone-btsnoop/aptx-adaptive-reference-48k96k.bin (3.84 MB)
手机 btsnoop   /home/baizhu945/work/phone-btsnoop/btsnoop_hci_*.log
反汇编产物     /tmp/module.dis, /tmp/r3lib.dis
构建脚本       /home/baizhu945/work/openaptx/build-helper/build-any.sh
```

---

## 3. 当前系统配置（/etc/nixos/pipewire-aptx-adaptive-module.nix）

```nix
PIPEWIRE_APTX_ADAPTIVE_MODE = "r2";        # R2 CAPI wrapper（非 lossless）
APTX_ADAPTIVE_PROFILE = "6";
APTX_ADAPTIVE_STRIP_OTA = "1";             # ★ 剥离 8 字节内部 OTA 包装
APTX_ADAPTIVE_CODEC_FRAMES = "1200";       # R2 帧长 = 1200 样本 (25ms@48k)
APTX_ADAPTIVE_SOURCE_TYPE = "0x00";        # ★ 必须是 0x00，不是 0x02
APTX_ADAPTIVE_FORCE_RATE = "48000";        # 与图时钟一致
APTX_ADAPTIVE_CHANNEL_MODE = "stereo";     # ★ STEREO，不是 JOINT_STEREO
APTX_ADAPTIVE_LOSSLESS = "off";
APTX_ADAPTIVE_QHS_SUPPORT = "0";
APTX_ADAPTIVE_ABR = "1";
APTX_ADAPTIVE_CAPTURE = "/tmp/aptx-adaptive-reference.bin";  # sink 捕获路径
```

**另外必须设置（否则速率偏差 13.5%）**：
```bash
pw-metadata -n settings 0 clock.force-quantum 1200
```

**bluez5.codecs 顺序**：`aptx_hd` 排在 `aptx_adaptive` 之前（默认走可用的 aptX HD）。

---

## 4. 已修复的问题（全部验证过）

| # | 问题 | 修复 | 证据 |
|---|---|---|---|
| 1 | **source type 必须是 0x00** | `APTX_ADAPTIVE_SOURCE_TYPE=0x00` | 手机 btsnoop 对比；0x02 时 `wrote:-1`，0x00 时 0 失败 |
| 2 | **通道模式必须 STEREO** | `APTX_ADAPTIVE_CHANNEL_MODE=stereo` | 手机协商 `40 02` |
| 3 | **PipeWire quantum 必须等于块大小** | `clock.force-quantum 1200` | quantum 2048 时喂入 54500 样本/s，1200 时 384960 B/s |
| 4 | **R2 wrapper 的 TTP 不递增** | helper 注入单调时钟 TTP | 补丁后 TTP 每帧 +375 |
| 5 | **R3 内核分派条件** | 加采样率检查 `mem[0x42f4]==0xac44` | 反汇编 0x137e4 |
| 6 | **R2 帧长是 1200 样本** | `APTX_ADAPTIVE_CODEC_FRAMES=1200` | dts 恒为 1200 |
| 7 | **R2 输出含 8 字节内部 OTA 包装** | `APTX_ADAPTIVE_STRIP_OTA=1` | 手机载荷 656B，我们修复前 664B |
| 8 | **Lossless 只支持 44.1kHz** | Lossless 时强制 44100 | 48k 时链路停在 11 B/s |

---

## 5. 当前实测结果（修复后）

```
HCI 媒体包：RTP 12 + 656 = 668 字节   ✓ 与手机一致
发送成功：wrote:668 × 918，失败 0     ✓
缓冲排空：unsent size:0 × 1826        ✓
速率：26.42 kB/s                      ✓ 与手机一致
helper：读 385360 B/s，写 26964 B/s   ✓
```

**但耳机仍无声。**

---

## 6. ⚠️ 唯一剩余差异：帧头字节 2

**我们的 1713 帧：**
```
8300d0a1f27fff0f...    ← 字节2 = d0（1697 帧 / 99%）
8300d2a1b9031dd0       ← 5 帧
8300d5a1c178ff43       ← 2 帧
```

**手机参考（48kHz 段）：**
```
d4: 575    d2: 330    d3: 170    d5: 146
d0: 105    d1:  62    d6:  57    d7:  18
```

**手机在 0-7 全范围变化，我们几乎恒为 0。**

**推测**：字节 2 是帧类型/速率/复杂度指示，解码器据此选择解码路径。

---

## 7. 完整帧结构（已确认）

```
A2DP 包 = RTP(12) + aptX Adaptive 帧(656)

RTP: V=2, PT=96, seq++, ts += 1200
帧:
  字节 0-1: 83 00          ✓ 与手机一致（同步字）
  字节 2:   d0|类型        ❌ 我们的不变化  ← 当前唯一差异
  字节 3:   a1             ✓
  字节 4-5: 随内容变化     ✓
  字节 6-7: ff 0f          ✓
  字节 8+:  编码数据       ✓（随输入变化）
```

---

## 8. 参考码流的获取方法（重要，可复现）

**问题**：手机把 aptX 编码 offload 到控制器，btsnoop 里没有媒体包。

**解决方案**：把 AX210 变成 A2DP sink，手机作为源发送 aptX Adaptive。

已实现（PipeWire fork 提交 `a521ff1` + `a28341e`）：
```c
/* a2dp-codec-aptx-adaptive.c */
codec_fill_caps()    // 忽略 flags，sink 侧也广告 aptX Adaptive
codec_init()         // SINK 标志 → 最小状态，不启动 helper
codec_start_decode() // 解析 RTP 头
codec_decode()       // 把已解密的载荷写入 APTX_ADAPTIVE_CAPTURE
```

**关键**：A2DP 链路是加密的，HCI 抓包无法解析；但 `codec_decode()` 收到的是**已解密**的载荷，直接落盘即可。

**操作步骤**：
1. 手机配对 PC（`bluetoothctl` 里需要输入 `yes` 确认配对码）
2. 手机蓝牙设置里连接 PC
3. 手机播放音乐
4. `/tmp/aptx-adaptive-reference.bin` 自动累积

**注意**：手机重连时可能需要重新确认配对码（PC 侧 agent 提示）。

---

## 9. 关键的反汇编发现

| 地址 | 内容 |
|---|---|
| `0x137e4` | `capi_aptx_adaptive_enc_process_wrapper`：仅当 `mem[0x24d]==3` **且** `mem[0x42f4]==0xac44` 才走 R3 内核 |
| `0x4550` | `capi_aptx_adaptive_enc_process`：输入读取在 `0x4650`（`stream+0x10 → buf_ptr`，`buf+0x4 → actual_data_len`） |
| `0x9bc0` | `deferred_rhs_process`：`aptX3Encode(ctx, in_desc, out_desc)` 调用点 |
| `0x14dc0` | `calcTTPAdj` |
| `0x1588c` | `incTtp` |

**CAPI 输入结构（已确认匹配）**：
```c
capi_stream_data_v2_t {
    +0x00  flags         // bit1 必须置位（CAPI_STREAM_V2=2）
    +0x08  timestamp
    +0x10  buf_ptr       // ← 模块读这里
    +0x14  bufs_num
    +0x18  metadata_list_ptr
}
capi_buf_t {
    +0x00  data_ptr
    +0x04  actual_data_len  // ← 模块读这里
    +0x08  max_data_len
}
```

---

## 10. 诊断工具（已就绪）

```bash
# 1. helper 协议级测试（无蓝牙）
cd /home/baizhu945/work/openaptx/build-helper
export LD_LIBRARY_PATH=$(paste -sd: /tmp/hostlibs.txt)
python3 test-helper.py --blocks 30 48k 44k-r22 r3-48k

# 2. 端到端稳定性测试（自动重连+选 profile+测量）
./test-adaptive.sh 3

# 3. 抓包并解析我们发出的帧
sudo btmon -w /tmp/x.hci          # 然后播放
TS=/nix/store/fy4pvbxfj8nj9f9pfq31i67vdp7n9csd-wireshark-cli-4.6.8/bin/tshark
CID=$($TS -r /tmp/x.hci -T fields -e btl2cap.cid | sort | uniq -c | sort -rn | head -3 | awk '{print $2}' | grep -v '^$' | head -1)
$TS -r /tmp/x.hci -Y "btl2cap.cid==$CID" -T fields -e btl2cap.payload | head -20

# 4. helper 内部状态 dump（需要在 helper 里加 APTX_DUMP_R2_STATE）
```

**重新构建 helper**：
```bash
cd /home/baizhu945/work/openaptx/build-helper
export LD_LIBRARY_PATH=$(paste -sd: /tmp/hostlibs.txt)
./build-any.sh /home/baizhu945/work/openaptx/research/aptx-adaptive-qemu/aptx-lossless-helper.c /tmp/helper-new
cp /tmp/helper-new /home/baizhu945/Documents/aptx-adaptive-runtime/helper/aptx-lossless-helper
```

**修改 NixOS 模块后**：
```bash
# 1. 改可写副本
vim /home/baizhu945/work/nixos-aptx-fix/pipewire-aptx-adaptive-module.nix
# 2. 语法检查
nix-instantiate --parse /home/baizhu945/work/nixos-aptx-fix/pipewire-aptx-adaptive-module.nix
# 3. 复制回 /etc/nixos（sudo 密码 wcandxl）
echo 'wcandxl' | sudo -S cp /home/baizhu945/work/nixos-aptx-fix/pipewire-aptx-adaptive-module.nix /etc/nixos/
# 4. 重建
sudo nixos-rebuild switch
```

---

## 11. 下一步（按优先级）

### A. 找出字节 2 的控制点 ⭐ 最高优先
1. 在 helper 里加 `APTX_DUMP_R2_STATE`，dump `module_memory` 的 0x200-0x300 区间
2. 对比手机参考码流的字节 2 序列，找出编码它的状态字段
3. 检查 `config.profile = 0x1000`（HIGH_QUALITY）是否被正确应用
4. 检查 `min_sink_buffer[3] = {20,20,20}` / `max_sink_buffer[3] = {50,50,50}` 是否需要调整

### B. 检查 IMCL quality level 是否生效
- 已知 `send_imcl_quality_level(state, 5)` 调用成功但无效果
- 可能需要先发送其他 IMCL 消息（如 sideband 配置）

### C. 用解码库验证码流
- `libaptXAdaptiveDec4.so` 有 `aptX4Decode_Create` / `AxStreamDecode`
- 用它对我们的码流和手机码流解码，比较 PCM 输出
- **如果能解码我们的码流但输出错误 → 编码器问题；如果解码失败 → 码流格式问题**

### D. 尝试 R3 路径
- R3 直编路径的载荷随输入变化（已验证）
- 但帧头是 `8b` 开头，与手机的 `83` 不同
- 且 R3 需要 Lossless 协商

---

## 12. 有效的对照数据

**手机 48kHz 静音帧**：
```
8300d0a1fc7fff0f9f9f9fff9fff9fff9fff93ff8080000000...
```

**手机 48kHz 音频帧**（同一首歌）：
```
8300d5a1f57c8ceffbae...
8300d4a19e004e800fe1...
8300d4a1de0dcf868cc9...
8300d2a17d802f4363e0...
8300d3a19e804f003df8...
```

**我们的静音帧**：
```
8300d0a1fc7fff0f879fc0fff9fffff9f9ffffff...
```

**我们的音频帧**（440Hz 正弦）：
```
8300d0a1f27fff0f879fffffff0fef9fb9a6e8a44bf7b438...
8300d0a1f27fff0f879fffffff0fee9f816e680a69e03732...
```

---

## 13. 已知的坑

1. **手机 btsnoop 路径**：`/data/log/bt/`（世界可读），不是 `/data/misc/bluetooth/logs/`（需 root）
2. **配对需要 PC 侧确认**：`bluetoothctl` 会提示 `Confirm passkey XXXXXX (yes/no)`，必须输入 `yes`
3. **SDP 缓存**：耳机断开后需等 ≥15 秒再重连才会重新查询 SDP
4. **MOMENTUM 5 多点连接**：A2DP 同一时刻只给一个设备
5. **helper PID 获取**：必须用 `pgrep -x qemu-hexagon`，不要 `pgrep -f`（会匹配到自己的命令）
6. **测量前必须验证**：以 aptX Adaptive 连接 + 输出选择正确
7. **A2DP 链路加密**：HCI 抓包无法解析，必须用 sink 回调捕获
8. **`br-connection-abort-by-local`**：重连失败时先 `disconnect` 等 16 秒再 `connect`

---

## 14. 相关提交

**pipewire fork**（`github.com/baizhu945/pipewire`，branch master）：
```
6b4b2f0  bluez5: optionally strip the R2 CAPI OTA wrapper on wire
a28341e  bluez5: dump the decrypted aptX Adaptive payload from the capture sink
a521ff1  bluez5: add a capture-only aptX Adaptive sink
a1aaee5  bluez5: add APTX_ADAPTIVE_CODEC_FRAMES diagnostic override
a28c616  bluez5: add APTX_ADAPTIVE_SOURCE_TYPE diagnostic override
eff3adf  bluez5: add APTX_ADAPTIVE_FORCE_RATE diagnostic override
3998394  bluez5: force 44.1 kHz when aptX Lossless is enabled
fbf3dec  bluez5: prefer JOINT_STEREO + channel-mode override
90cb7d7  bluez5: use 720-sample blocks for R3
a468a7a  bluez5: drop the R3 rate/format restrictions
47eaf64  bluez5: remove the R3 48 kHz restriction
```

**openaptx fork**（`github.com/baizhu945/openaptx`，branch `research/aptx-adaptive`）：
- 已向上游提交 PR #15（`arkq/openaptx`），包含状态报告和求助

---

## 15. 其他文档

- `/home/baizhu945/work/kalimba/EDKCS-FORMAT.md` — EDKCS 容器格式（已破译）
- `/home/baizhu945/work/kalimba/BT11-FIRMWARE-MAP.md` — BT11 固件分区图 + 15 个 DSP 镜像
- `/home/baizhu945/work/openaptx/research/aptx-adaptive-qemu/STATUS.md` — 英文状态报告（PR 用）
- `/home/baizhu945/work/openaptx/research/aptx-adaptive-qemu/gen-cntr-process-loop.md` — AudioReach gen_cntr 数据通路分析

---

## 16. 恢复到可用状态

如果实验失败，恢复到 aptX HD（100% 可用）：

```nix
# /etc/nixos/pipewire-aptx-adaptive-module.nix
# 把 aptx_hd 放在 bluez5.codecs 列表最前（已默认如此）
```

然后重连耳机即可。aptX HD 实测 `wrote:894` × 684，0 失败。
