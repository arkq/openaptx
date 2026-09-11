# aptX Adaptive on a non-Qualcomm Linux host — 项目交接报告

**最后更新**：2026-09-10 上午（第五轮）
**当前状态**：AVDTP 配置与 RTP 头已与手机**逐字节一致**（44.1k/48k/96k 三种采样率位
都已对齐）；耳机仍静音。

> ⚠️ **勘误（2026-09-10）**：本文 §0 / §3 / §6 / §7 的早期版本把 A2DP 采样率位
> `0x40` 当成 **192 kHz**，**这是错的**。用户实测确认：手机→耳机用的就是
> **44.1 kHz**（`0x40`）；`0x40`=44.1k、`0x10`=48k、`0x20`=96k。正确推导见 §12.1，
> 48 kHz / 24-bit 会话复抓见 §14。早期“码率 212 kbps 低于下限 279 kbps 才是根因”
> 的判断也已撤回（§13.10）。

---

## 0. 一句话总结

我们这轮做到了三件以前做不到的事：

1. **证明编码器输出正确**——用 Qualcomm 官方参考解码器（openaptx PR #9 里的
   `test-decoder.exe`，Wine 运行）解码我们经蓝牙实际发出的码流，FFT 主频正好
   `440.0 Hz`（即播放的测试正弦），48 kHz 立体声。
2. **证明传输层正确**——把手机抓下来的**原始帧**原样通过我们的 PipeWire/BlueZ
   链路回放，HCI 抓包逐帧比对完全一致，RTP 头与可用的 aptX HD 完全同构。
3. ~~**找到并修掉真正的根因**——插件把 aptX Adaptive 的采样率位掩码写错了：
   `44100` 写成 `0x40`（实际是 192 kHz），`96000` 写成 `0xa0`（未定义值）。
   所以“44.1 kHz 会话”实际向耳机声明 192 kHz，却喂 44.1 kHz 帧 → 耳机静音。~~
   **【2026-09-10 勘误】这条结论是错的。** `0x40` 本来就是 **44.1 kHz**
   （手机→耳机的可用会话就是 `40 02 …`，见 §12.1、§14）。当时的“修复”把
   44.1 kHz 从能力里删掉了，反而更糟，已在 `e7d50a6` 全部改回。

---

## 1. 硬件与系统

| 组件 | 详情 |
|---|---|
| 主机 | Intel Core Ultra 7 255HX，30 GiB DDR5 |
| 系统 | NixOS 26.11pre，linux-zen 7.2.3 |
| 蓝牙适配器 | Intel AX210（`FC:B3:AA:C5:01:42`），无高通控制器 |
| 目标耳机 | Sennheiser MOMENTUM 5（`80:C3:BA:B7:16:3B`），SEP 9 vendor `0x00d7` codec `0x00ad` |
| 参考源 1 | HONOR 90GT / Android 16（aptX Adaptive 可用）—— 手机蓝牙地址 `44:90:46:40:FD:DD` |
| 参考源 2 | FiiO BT11 / QCC5181（aptX Lossless 可用） |
| 编码器执行 | QEMU 11.1.0 `qemu-hexagon -cpu v68`，Hexagon clang 22.1.8 |
| 专有 blob | `/home/baizhu945/work/aptxlibs_CPH2749/`（**不进 Nix store**） |
| 参考工具 | `/tmp/pr9bin/archive/x86/*.exe`（Wine；见 §7 重新获取方法） |

---

## 2. 关键路径

```
helper 源码    /home/baizhu945/work/openaptx/research/aptx-adaptive-qemu/aptx-lossless-helper.c
helper 二进制  /home/baizhu945/Documents/aptx-adaptive-runtime/helper/aptx-lossless-helper
回放诊断二进制 .../helper/aptx-lossless-helper-replay（本轮的 APTX_ADAPTIVE_REPLAY 实验用）
PipeWire 插件  /home/baizhu945/work/pipewire/spa/plugins/bluez5/a2dp-codec-aptx-adaptive.c
               /home/baizhu945/work/pipewire/spa/plugins/bluez5/a2dp-codec-caps.h   ← 本轮修复点
NixOS 模块     /etc/nixos/pipewire-aptx-adaptive-module.nix
               可写副本 /home/baizhu945/work/nixos-aptx-fix/
参考码流       /home/baizhu945/work/phone-btsnoop/aptx-adaptive-reference-48k96k.bin (3.84 MB)
手机 btsnoop   /home/baizhu945/work/phone-btsnoop/btsnoop_hci_*.log
构建脚本       /home/baizhu945/work/openaptx/build-helper/build-any.sh
```

**pipewire fork 最新 rev**：`bf15869af6eb7ddcb2a653a0d6d6d2018e32bef1`（分支 master，
已推送 github.com/baizhu945/pipewire）。

---

## 3. 本轮的根因（最重要）

> ⚠️ **本节整节作废（2026-09-10 勘误）**：下面的推导全部建立在“`0x40` = 192 kHz”
> 这个错误前提上。用户实测确认：手机→耳机（可用会话）的 SET_CONFIG 就是
> `40 02 …`，即 **44.1 kHz**。Qualcomm `bthost_ipc.h` 里的
> `44100=0x08 / 192000=0x40` 是它自己 **DSP payload** 的编号，**不能**照抄到
> A2DP 字段。正确映射见 §12.1，最新复抓数据见 §14。

### 3.1 采样率位掩码错误

Qualcomm 官方 A2DP offload 解析器 `bthost_ipc.h`
（syberia-project/platform_hardware_qcom_bt, `bthost_ipc/bthost_ipc.h`）定义：

```c
#define A2D_APTX_ADAPTIVE_SAMP_FREQ_MASK  (0xF8)
#define A2DP_APTX_ADAPTIVE_SAMPLERATE_44100   (0x08)
#define A2DP_APTX_ADAPTIVE_SAMPLERATE_48000   (0x10)
#define A2DP_APTX_ADAPTIVE_SAMPLERATE_88000   (0x20)
#define A2DP_APTX_ADAPTIVE_SAMPLERATE_192000  (0x40)
```

插件里（修复前）：

```c
#define APTX_ADAPTIVE_SAMPLING_FREQ_44100  0x40   // 对：44.1 kHz（当时误判为 192k）
#define APTX_ADAPTIVE_SAMPLING_FREQ_48000  0x10   // 对：48 kHz
#define APTX_ADAPTIVE_SAMPLING_FREQ_96000  0xa0   // 错：未定义（正确值 0x20）
```

当时的推理（**错误**）：`APTX_ADAPTIVE_FORCE_RATE=44100` 协商出 `0x40`（误认为
192 kHz），编码器却按 44.1 kHz 出帧（帧头字节 2 = `0xc0`），耳机按 192 kHz 解码
→ 静音。实际上 `0x40` 与帧头 `c0` 完全自洽，根本不存在这个矛盾。
48 kHz（`0x10`）也一直是对的，但耳机同样静音，说明原因在别处（见 §13）。

当时的“修复”（`bf15869`，**已于 `e7d50a6` 全部改回**）：

```c
#define APTX_ADAPTIVE_SAMPLING_FREQ_44100  0x08
#define APTX_ADAPTIVE_SAMPLING_FREQ_48000  0x10
#define APTX_ADAPTIVE_SAMPLING_FREQ_88200  0x20
#define APTX_ADAPTIVE_SAMPLING_FREQ_96000  0x40
#define APTX_ADAPTIVE_SAMPLING_FREQ_192000 0x40
```

广告能力从 `0xf0` 变成 `0x58`，与 MOMENTUM 5 的能力 `0x70` 交集只剩 `0x50`
（48 kHz、96 kHz）——**等于把 44.1 kHz 从能力里删掉了**，这也是那次改动
“看起来改对了”却依然静音的原因：它修的是一个不存在的 bug。

### 3.2 耳机能力 / 手机配置（HCI 实测）

从我们自己的 btmon 抓包里解出耳机 aptX Adaptive 能力（GET_CAP 响应）：

```
d7 00 00 00 ad 00 71 0a 37 6a c8 7d 64 7d 00 01 82 00 00 0f 02 03 03 03 00 aa
                  ^^    ^^                                        ^^
            采样率0x71 通道0x0a                              features 0x0f000082
```

手机（HONOR 90GT）发给耳机的 SET_CONFIG（手机 btsnoop 原始字节）：

```
d7 00 00 00 ad 00 40 02 50 64 64 64 ff ff 00 01 92 00 00 0f 02 03 03 03 00 aa
                  ^^    ^^                    ^^
            采样率0x40  通道0x02         features 0x0f000092（含 R2.2 位）
```

**【2026-09-10 勘误】** `0x40` **不是** 192 kHz，而是 **44.1 kHz**（用户实测确认：
手机→耳机的默认会话就是 44.1 kHz；48 kHz 内容时才 RECONFIGURE 成 `0x10`，见 §14）。
`0x92` 比我们默认能力里的 `0x17` 多一个 R2.2 位。

---

## 4. 已确认正确的东西（本轮新增证据）

| 项 | 证据 |
|---|---|
| **编码器输出可被官方解码器正确解码** | `test-decoder.exe` 解我们实际发出的码流 → 48 kHz 立体声、FFT 主频 **440.0 Hz** |
| **编码器 44.1 kHz 输出也有效** | 同法解码 → 44100 Hz、主频 ~440 Hz |
| **传输层逐帧正确** | 回放手机原始帧，HCI 抓包比对 `matches secA frame 790/791/...` 全部命中 |
| **RTP 头与可用的 aptX HD 同构** | 两者都是 `80 60 <seq> <ts> 00000000`，PT=96、SSRC=0、seq/ts 递增正常 |
| **AVDTP 信令完整** | DISCOVER → GET_ALL_CAP → **SET_CONFIG** → OPEN → **START** 全部被耳机 ACCEPT |
| **量化时钟正确** | `clock.force-quantum 1200`；RTP ts 每帧 +1200（25 ms@48k） |
| **耳机只连电脑** | 用户确认，排除多点连接 |

---

## 5. 诊断工具（本轮新增，非常有价值）

### 5.1 Qualcomm 官方参考工具（Wine）

从 openaptx **PR #9** 取（`archive/x86/*.exe`，PE32 Windows 控制台程序）：

```bash
# 重新获取（PR #9 的 patch 含二进制 blob）
curl -sL https://github.com/arkq/openaptx/pull/9.patch -o /tmp/pr9.patch
mkdir -p /tmp/pr9bin && cd /tmp/pr9bin && git init -q && git apply /tmp/pr9.patch

export WINEPREFIX=/tmp/wineprefix-aptx WINEDEBUG=-all
W=/tmp/pr9bin/archive/x86

# 1) 码流清洗（关键：aptX Adaptive 帧必须先做 16-bit 字节交换，用 -e）
wine $W/aptx-adaptive-packet-header-strip_NEW_BYTE_SWAP.exe -e -d outdir stream.bin

# 2) 解码成 WAV
wine $W/test-decoder.exe -i outdir/stream-clean.bin -o out.wav -x hq

# 3) 其余工具
wine $W/ax3-test-decoder.exe -i ... -o ... -x hq     # R3/v3 解码器
wine $W/test-stream-deinterleave.exe -d dir stream.bin
wine $W/slimbus2aptx-adaptive3.exe -d dir -r 48000 stream.bin
```

**注意**：`aptx-adaptive-packet-header-strip` 不加 `-e` 会在 ~9 帧后丢失同步；
加 `-e` 后整段通过（0 sync losses）。这说明参考解码器期望的是字节交换后的形式，
而手机/我们的线上码流是同一种（未交换）形式。

### 5.2 回放诊断（`APTX_ADAPTIVE_REPLAY`）

helper 新增：设 `APTX_ADAPTIVE_REPLAY=<file>` 时，每个音频请求返回文件里的
664 字节 OTA 记录（8 字节传输头 + 656 字节 codec 帧），跳过编码器。
用来把**手机原始帧**塞进同一条 PipeWire/BlueZ 链路。

构造回放文件（把手机的 656 字节帧加上 8 字节 OTA 头）：

```python
d=open('aptx-adaptive-reference-48k96k.bin','rb').read()
out=bytearray()
for i in range(3868,5852):           # secB = 解码器判定的 48 kHz 段
    ttp=(i*375)&0xffff
    out+=bytes([ttp&0xff,(ttp>>8)&0xff,0x64,0x01,0x00,0x00,0x00,0xae])+d[i*656:(i+1)*656]
open('/tmp/replay.bin','wb').write(out)
```

### 5.3 抓包与分析

```bash
# 抓包（必须用 setuid 的 /run/wrappers/bin/sudo，PATH 里的 sudo 不是 setuid）
echo 'wcandxl' | /run/wrappers/bin/sudo -S btmon -w /tmp/x.hci
TS=$(ls /nix/store/*wireshark-cli*/bin/tshark | head -1)

# 媒体 CID 与载荷
$TS -r /tmp/x.hci -T fields -e btl2cap.cid -e btl2cap.payload | awk -F'\t' 'length($2)>200{print $1}' | sort | uniq -c
# AVDTP 信令序列
$TS -r /tmp/x.hci -Y "btavdtp" -T fields -e frame.number -e btavdtp.signal_id -e btavdtp.message_type | awk -F'\t' '$2!="0x20"'
# 某帧原始字节
$TS -r /tmp/x.hci -Y "frame.number==269" -x
```

### 5.4 系统状态操作

```bash
export PATH=/run/current-system/sw/bin:$PATH
# 重连并选 aptX Adaptive（脚本已就绪）
/tmp/setup_adaptive.sh          # 断开→等16s→重连→set-profile 131093→打印协商配置
# 运行时用 systemd drop-in 改实验参数（注意必须排在 overrides.conf 之后，故用 zz- 前缀）
mkdir -p ~/.config/systemd/user/wireplumber.service.d
cat > ~/.config/systemd/user/wireplumber.service.d/zz-aptx-rate.conf <<'EOF'
[Service]
Environment=APTX_ADAPTIVE_FORCE_RATE=96000
EOF
systemctl --user daemon-reload && systemctl --user restart pipewire.socket pipewire wireplumber
```

**坑**：`~/.config/systemd/user/<svc>.service.d/` 里的 drop-in 按文件名排序，
NixOS 生成的 `overrides.conf` 排在 `a*` 之后，所以必须用 `zz-` 前缀才能覆盖。

---

## 6. 本轮实测矩阵（修复常量并重建后）

> 说明（2026-09-10 勘误后重新标注）：表中 `0x40` 一律读作 **44.1 kHz**（曾误标
> 192 kHz），`0x10` = 48 kHz；`0x12 / 0x92` 是 features 低字节。结论不变——
> 这些组合全部静音。

| # | 协商配置（byte6/7/features） | 帧来源 | 结果 |
|---|---|---|---|
| 1 | `0x40 02 12`（44.1k） | 我们的 44.1k 帧 | 静音 |
| 2 | `0x10 02 12`（48k） | 我们的 48k 帧 | 静音 |
| 3 | `0x10 02 12`（48k） | **手机原始 48k 帧**（回放） | 静音 |
| 4 | `0x40 02 92`（44.1k + R2.2 位） | 手机原始帧 | 静音 |
| 5 | `0x10 02 92`（48k + R2.2 位） | 我们的 48k 帧 | 静音 |
| 6 | `0x40 02 12`（`bf15869` 里想当 96k，实际是 44.1k） | 我们的 96k 帧 | 静音 |
| — | aptX HD（同一链路、同一 RTP 头） | — | **正常出声** |

补充事实：

- R2 wrapper 的真实帧长实测就是 **1200 样本/帧**（喂 600/672/1200 样本块，
  输出都稳定在 1200 样本一帧），即 48 kHz 下 25 ms。所以帧率不是问题。
- **helper 的 96 kHz 模式有 bug**：配置 `encoder_rate=96000`（selector 0）时，
  输出帧头仍是 `8300d0a1`，用参考解码器解出来是 **48000 Hz**（输入 1000 Hz →
  解出 504 Hz）。手机真正的 96 kHz 段帧头是 `8300b0a1`。这说明
  `capi_rate_selector()` 把 96000 映射到 selector 0 并不产生真正的 96 kHz 码流，
  需要重新确认 selector 与采样率的对应关系（Qualcomm 官方把 88000/192000 都映射
  到 0）。→ **已解决**：selector `3` 才是 96 kHz（§12.4），与 192 kHz 无关。
- 耳机能力 `0x71` 的采样率位 `0x70` = {0x40, 0x20, 0x10} = **{44.1k, 96k, 48k}**
  （2026-09-10 勘误：**不是** {48k, 88.2k, 192k}）。手机 SET_CONFIG 选 `0x40`
  = **44.1 kHz**（用户实测确认），遇到 48 kHz 内容会 RECONFIGURE 成 `0x10`（§14）。
  我们的编码器能出 44.1k/48k/96k 三种码流（§12.4、§13.9）。

---

## 7. 下一步（按优先级）

### A. ~~打通真正的 192 kHz 编码路径~~ ⭐ 最高优先

> ⚠️ **已作废（2026-09-10 勘误）**：`0x40` 是 44.1 kHz，耳机能力也不是
> {48k,88.2k,192k}；**192 kHz 从来没有出现在任何可用链路上**，此方向没有依据。
> 下面三步仅作历史记录保留。

原（错误）证据链：手机用 `0x40`（误认为 192 kHz）与耳机通信，耳机能力里也只有
48k/88.2k/192k。原计划：

1. 在 helper 的 `capi_rate_selector()` 里增加 192000（和 88200）分支，
   并确认 CAPI `sampling_rate` 字段对 192k 的取值（Qualcomm 表里 192000→0，
   与 88000 相同，需要实验确认）。
2. 用参考解码器验证输出帧头变成 `8300b0a1`（96k 族）或 192k 对应值，
   且解出的 WAV 采样率正确。
3. 在插件的 `adaptive_rates[]` 里把 192000 的 codec_rate 改成 192000
   （当前是 96000 下采样）。

### B. 用手机作对照源抓 RTP 头 / 时序
让手机作为 A2DP 源连电脑并播放，用 btmon 抓包，逐字节对比手机 RTP 头、
每帧间隔（是否真的 25 ms）与 AVDTP 时序。

### C. ~~确认 headphone 能力的真实语义~~（2026-09-10 已解决）
`0x71` 的采样率位 `0x70` = **{44.1k, 48k, 96k}**，features `0x0f000082`（无 R2.2 位）。
证据：§12.1 的手机 SET_CONFIG + §14 的复抓。

### D. 检查是否存在厂商专有控制通道
aptX Adaptive 的 IMCL / sideband 反馈可能走独立的 L2CAP PSM；对比手机
btsnoop 里除 AVDTP 之外的控制通道。

---

## 8. 参考数据

**手机 48 kHz 段（secB，解码器判定 48 kHz 立体声，1984 帧）帧头分布**：
```
0xd0:111  0xd1:83  0xd2:512  0xd3:226  0xd4:765  0xd5:178  0xd6:84  0xd7:25
```

**手机 96 kHz 段（secA，解码器判定 96 kHz 单声道，3868 帧）帧头分布**：
```
0xb0:556  0xb1:755  0xb2:82  0xb3:1275  0xb4:785  0xb5:415
```

**我们的 48 kHz 编码输出帧头**：`8300d0a1 ...` 起，字节 2 = `d0..d4`（与 secB 同族）。

**我们的 44.1 kHz 编码输出帧头**：`8300c0a1 ...`。

**回放验证**：HCI 抓包中的 RTP 载荷与 `replay_secA.bin` 的第 790/791/... 帧
逐字节相等；RTP = `80 60 <seq:2> <ts:2+2> 00 00 00 00`，ts 每帧 +1200。

---

## 9. 相关提交

**pipewire fork**（`github.com/baizhu945/pipewire`，master）：
```
bf15869  bluez5: fix the aptX Adaptive sampling-frequency bitmask   ← 本轮根因修复
6b4b2f0  bluez5: optionally strip the R2 CAPI OTA wrapper on wire
a28341e  bluez5: dump the decrypted aptX Adaptive payload from the capture sink
a521ff1  bluez5: add a capture-only aptX Adaptive sink
```

**openaptx fork**（`github.com/baizhu945/openaptx`，`research/aptx-adaptive`）：
```
fe5d973  helper: add a reference-bitstream replay diagnostic        ← 本轮
bcd5c80  research: handoff report for the aptX Adaptive bridge
e5d2be3  research: status report for aptX Adaptive/Lossless on a non-Qualcomm host
```

**上游**：openaptx PR #15（状态报告）、PR #12（parser + QEMU adapter）、PR #9（参考工具）。

---

## 10. 恢复到可用状态

aptX HD 100% 可用（实测 `wrote:894` × 684，0 失败）。若实验失败：

1. 删除实验用 systemd drop-in：
   `remove-without-permission ~/.config/systemd/user/{pipewire,wireplumber}.service.d/zz-*.conf`
2. `systemctl --user daemon-reload && systemctl --user restart pipewire.socket pipewire wireplumber`
3. `wpctl set-profile <card> 131079`（aptX HD）或重连耳机走默认。

模块里 `bluez5.codecs` 已把 `aptx_hd` 排在 `aptx_adaptive` 之前，默认即走 HD。

---

## 11. 其他文档

- `/home/baizhu945/work/openaptx/research/aptx-adaptive-qemu/STATUS.md` — 英文状态报告
- `/home/baizhu945/work/openaptx/research/aptx-adaptive-qemu/gen-cntr-process-loop.md` — AudioReach gen_cntr 数据通路分析
- `/home/baizhu945/work/kalimba/EDKCS-FORMAT.md`、`BT11-FIRMWARE-MAP.md`

---

## 12. 第二轮进展（2026-09-09 深夜）

### 12.1 采样率位映射最终确定

用户澄清：抓到的手机码流是**手机→电脑**（电脑当 sink），而 btsnoop 日志里出现的是
**耳机**的 MAC（`80:C3:BA:B7:16:3B`）→ 那是**手机→耳机**的会话。据此：

| 来源 | SET_CONFIG byte6/7 | features |
|---|---|---|
| 手机→耳机（btsnoop，默认 44.1k） | `0x40 0x02` | `0x0f000092` |
| 手机→电脑（btmon 抓包，48k） | `0x10 0x02` | **`0x0f000017`** |

结论：**`0x40 = 44.1k`、`0x10 = 48k`、`0x20 = 96k`**（耳机能力 `0x71` 的采样率位
`0x70` 正好是这三个）。Qualcomm `bthost_ipc.h` 的 `44100=0x08` 是它自己 DSP
payload 的编号，**不能**照抄到 A2DP 字段上。

> 曾按 Qualcomm 表把 44100 改成 `0x08`、96000 改成 `0x40`（commit `bf15869`），
> 已按上表**改回** `0x40/0x10/0x20`（commit `e7d50a6`）。

### 12.2 与手机逐字节一致仍静音

现在我们的 SET_CONFIG 是
`d7 00 00 00 ad 00 10 02 50 64 64 64 ff ff 00 01 17 00 00 0f 02 03 03 03 00 aa`
——与手机→电脑的配置**完全相同**（含 features `0x17`）。RTP 头也与手机逐字节一致
（`80 60 <seq> <ts> 00000000`，PT=96、SSRC=0、ts 每帧 +1200）。**耳机仍静音**。

新增的诊断开关（PipeWire 插件，env 可改、无需重建）：

- `APTX_ADAPTIVE_FREQ_BITS=0xNN` — 直接钉住协商出的采样率位
- `APTX_ADAPTIVE_FEATURES=0xNNNNNNNN` — 钉住 features 字

### 12.3 真正的疑点：码率/帧长

| 指标 | 我们的 48k 流 | 期望（用户实测手机） |
|---|---|---|
| 净载荷速率 | 656 B / 25 ms ≈ **212 kbps（26.4 kB/s）** | ≈ **420 kbps（~50 kB/s）** |
| R2 帧长（实测） | 1200 样本 @48k、1102.8 @44.1k、1923.7 @96k | ? |

**212 kbps 低于 aptX Adaptive 的下限 279 kbps**，很可能是耳机拒绝解码的原因。
要拿到 420 kbps，帧长必须是 12.5 ms（48k→600 样本），而不是我们现在的 25 ms。

模块内部日志里有
`AVS_ENCODER_PARAM_ID_BIT_RATE_LEVEL_MAP N Selected Period=%f PCMinterval=%d`
和 `... Bitrate selected is %d`，说明**帧长由码率等级决定**；但 helper 发的
bitrate map（279–420 kbps × 5 级）似乎没被采纳（`set_quality_level(5)` 无效果）。

**补充实测（第三轮开始）**：给 helper 加了 `APTX_R2_PROFILE` 覆盖，遍历
`0,1,2,3,4,5,6,7,0x1000,0x2000,0x4000` 十一个 profile 值，输出**恒为
1200 样本/帧、664 字节包**。所以 R2 wrapper 在 48 kHz 下的帧长与 profile 无关，
212 kbps 是它的固有值——而手机→电脑的 48 kHz 流（RTP ts +1200、656 B 载荷）
也正好是 212 kbps。**因此码率不是静音的原因**；用户的 50 kB/s 观察对应的是
96 kHz 段（656 B / 12.5 ms）。

### 12.4 本轮修掉的真 bug：96 kHz selector

helper 的 `capi_rate_selector()` 原来把 96000 映射到 **0**，而实测：
`1→48k、2→44.1k、**3→96k**`（帧头 `8300b0a1`，与手机 96k 段一致），其它值回落 48k。
已改为 `3`。用参考解码器验证：selector 3 的输出解码为 **96000 Hz**。

### 12.5 下一步（第三轮）

1. **把帧长/码率做上去** ⭐ 最高优先。方向：
   - 查 `AVS_ENCODER_PARAM_ID_BIT_RATE_LEVEL_MAP` 的正确 param ID / payload
     （helper 用的 `0x000132e1` 可能不是模块认的那个）。
   - 试不同 `profile` 值（helper 现在硬编码 `0x1000`），看 Period 是否变成 12.5 ms。
   - 直接给模块一个 12.5 ms 的 PCM interval（`APTX_ADAPTIVE_CODEC_FRAMES=600`）看
     它是否输出 600 样本/帧、总码率是否翻倍。
2. 若码率上去了仍静音：抓手机→耳机侧无法抓包，只能反向用 FiiO BT11（QCC5181）
   作为参考源，或继续查 controller/link 层。
3. 模块日志：`compat.c` 里的 `HAP_debug`/`HAP_debug_v2` 已改为可透出
   （`APTX_HAP_LOG=1`），但实测模块的日志仍未出现，需要进一步确认它走的是哪条通道。

---

## 13. 第三轮进展（2026-09-09 深夜至 09-10 凌晨）

### 13.1 本轮修掉的部署回归：OTA 头被剥掉

部署的 NixOS 模块里写着 `APTX_ADAPTIVE_STRIP_OTA = "1"`（来自 `6b4b2f0`），
于是插件把 helper 输出的 8 字节 OTA 前缀删掉再上 RTP。**这是错的**：
手机（电脑当 sink 抓包，`/tmp/48k.hci`、`/tmp/abr0.hci`、`/tmp/abr1.hci`）的真实
L2CAP 载荷恒为 **676 字节 = RTP 12 + OTA 8 + 帧 656**。

```
80 60 <seq> <ts> 00000000 | a8 61 64 01 00 00 00 ae | 83 00 d0 a1 ...
   RTP 12 B                 OTA 8 B (TTP/period/ptype/chan/ver)   656 B frame
```

已通过用户级 systemd drop-in 去掉该变量
（`~/.config/systemd/user/{pipewire,wireplumber}.service.d/zz-aptx-phone-exact.conf`，
注意 `UnsetEnvironment` 在 `Environment` **之后**生效，旧的 zz- 文件必须先删掉）。
现在我们的 wire 载荷与手机**逐字节一致**（回放实验 266/266 命中）。

### 13.2 OTA 头格式（完全解出）

`src/aptx-adaptive-stream.c` 的解析器 + 实测：

| 偏移 | 含义 |
|---|---|
| 0..1 | TTP，16 bit LE，单位 1/15000 s |
| 2 | **period**，单位 0.25 ms（0x64=25 ms、0x40=16 ms、0x32=12.5 ms） |
| 3 | **packet_type**，索引 `{348,656,140,152,560,760,960,348,980}` 字节 |
| 4 | channel_mode（0x00/0x80/0xa0/0xc0 → 解码器 mode 2/1/4/5） |
| 5..6 | 0 |
| 7 | 版本：0xae=R2、0xad=R3、0xaf=R2.2 |

### 13.3 模块内部的周期/码率表（关键）

反汇编 `aptx_adaptive_enc_module.so.1` 得到两张按“等级”索引的静态表：

```
period (ms)   @0x1CDA0: [20, 19, 18, 17, 16, 15, 14, 13, 12]
pcm_interval  @0x1CDC8: [960,912,864,816,768,720,672,624,576]   (48 kHz)
```

`AVS_ENCODER_PARAM_ID_BIT_RATE_LEVEL_MAP`（0x000132e1）的载荷布局是
`{num_levels:u32, 4B 保留, 每项 8B 中取前 4B}`，而**每项的值是 kbps**，
按阈值 `262/275/290/307/327/348/373/402/436` 映射到等级 0..8。
helper 之前写的是 **bps**（279000…），全部超过最高阈值，等级全部塌缩，
所以“bitrate map 无效果”。

### 13.4 模块日志终于可读（重大能力）

`compat.c` 的 `HAP_debug_v2` 之所以没输出，不是通道问题，而是那段代码的
`getenv("APTX_HAP_LOG")` 门控——把门控去掉（无条件打印）后，模块把**整个决策链**
都吐出来了（用 `SRC_COMPAT=/tmp/compat_log.c build-any.sh ...` 重建 helper）。
关键日志（`HAPv2` 的 arg2 起才是格式串实参，`%f` 占两个 32 位字）：

```
INIT: Final bitrate after all limiting conditions in Kona is 364000 bps
INIT: totalSamplesPerPacket 1344
INIT: Actual Period selected 14.0          # 编码器内部想要 14 ms
INIT DONE: Period encoded is 56            # 0x38 → 14 ms
PROCESS(): me->period = 100 , ttpadj = 24000   # wrapper 改成 25 ms！
PROCESS(): aptXEncode_SetBitRate() set to desired value 204000
BIT_RATE_LEVEL_MAP 1..7 Selected Level/Period/PCMinterval ...   # map 确实被应用
```

**结论：R2.2 wrapper 在 PROCESS 时把周期钉在 100（25 ms）/656 B，内部编码器
想用的 14 ms/364 kbps 被覆盖。** 实测 profile/MTU/sink buffer/IMCL br_level/
输入块长（600/672/1200/1344）都不改变这个 25 ms。

### 13.5 本轮否掉的假设

| 假设 | 实验 | 结果 |
|---|---|---|
| 缺 OTA 头导致静音 | 去掉 STRIP_OTA，逐字节对齐手机 | 仍静音 |
| SET_CONFIG 不对 | 用手机完全相同的 `40 02 … 92`（44.1k STEREO） | 仍静音 |
| 码流本身有问题 | 回放手机原始 664 字节记录（266/266 逐字节一致） | 仍静音 |
| 码率/帧长偏低 | 用手机真实帧 + period 改 16 ms（62.5 fps、338 kbps、连续 25 s） | 仍静音 |
| 流不连续 | 30 s 连续流，1233 帧、零 >60 ms 间隙 | 正常 |
| 耳机不回反馈 | 30 s 内除 L2CAP config 外**没有任何** 反向 ACL | 确认无反馈 |

### 13.6 参考对照：aptX HD（可用）vs aptX Adaptive（静音）

同一链路同一耳机，`/tmp/cap_both.hci`：

| | 帧长 | 速率 |
|---|---|---|
| aptX HD（有声音） | 894 B（1515 帧，混 676 B） | **55 038 B/s ≈ 440 kbps** |
| aptX Adaptive（静音） | 676 B | **27 073 B/s ≈ 217 kbps** |

所以用户说的“~50 kB/s”确实对应可用链路的量级，我们的 27 kB/s 偏一半；
但 13.5 的 16 ms 实验说明**单把速率做上去并不能出声**，静音另有原因。

### 13.7 下一步（第四轮，按优先级）

**⭐ 13.7.0 本轮最后的关键发现：OTA 版本字节由等级决定（0xae ↔ 0xaf）**

反汇编 `encLevelHqStateMachine`（模块内 0x12eb0 起）得到：

```
r3 = level - 6
r4 = 0xae                        ; 默认版本 = R2
if (level-6) > 9  -> 保持 0xae
else switch(level-6):            ; level 6..15 全部
    memb(state+0x231) = 0xaf     ; 版本改成 R2.2
    ...
```

即 **等级 ≥6 时 wrapper 才发 0xaf（R2.2）包**，否则发 0xae（R2）。
而 R2.2 的包格式也不同：等级表（48k 表 @0x2097C，每项 4 字节）给出
`{0x02, period_hi, period_lo, packet_type=5}`，**packet_type 5 = 760 字节**，
加 8 字节 OTA 正好 **768 字节**（= helper 里的 `R2_2_LOSSLESS_OBSERVED_PACKET_SIZE`）。
实测 lossless=AUTO（44.1k/S16/mtu≥768/features 0x92）已经能产出
`ota=41 6f 70 05 a0 00 00 ae`（768 字节、channel_mode 0xa0、period 0x70），
**只差版本字节不是 0xaf**。

链路证据完全吻合这个假设：

| 会话 | features | 期望帧 |
|---|---|---|
| 手机→耳机（可用） | `0x0f000092`（含 R2.2 位） | 0xaf / 768 B |
| 手机→电脑（抓包） | `0x0f000017`（无 R2.2 位） | 0xae / 664 B |
| 我们（现在） | `0x0f000092`（宣称 R2.2） | **0xae / 664 B ← 不匹配！** |

**假设：我们向耳机声明了 R2.2，却发 R2 帧。**
已做的**最小验证（失败）**：把回放记录的 `packet[7]` 从 0xae 改成 0xaf，
其余字节不变（仍是手机原始帧），连续 22 s —— **仍然完全静音**。
所以“只改版本字节”不够；若这条假设成立，必须连**帧格式**一起换成
R2.2 的 760 B/ptype 5 形态（lossless=AUTO 已经能产出这种包，见上）。
两种修法：
1. 把等级顶到 6/7，让 wrapper 自己发 0xaf（需要搞清楚 br_level 的钳位：
   日志显示我们送 level 7，模块内部只认 `br_level = 3`）；
2. 用 lossless=AUTO 产出的 768 B 包 + 版本字节 0xaf 组成完整 R2.2 包回放
   （需要把 helper 的回放缓冲区从 664 改成 768）。

### 13.7.1 其他待办

1. 搞清楚 IMCL `br_level` 为什么被钳到 3（0x7888 起的处理代码），
   以及 `setEncoderCfgVals Inside tblidx for 2.0 mode is %d`（实测 1）。
2. 让 wrapper 别再钉 25 ms：继续用模块日志 + 反汇编找 `me->period` 的写入点
   （`capi_aptx_adaptive_enc_process_wrapper` 附近）。
3. 若 1/2 都不成立，考虑**绕过 R2.2 wrapper 直接调用内层 R2 编码器**
   （helper 已经用 `dlsym` 拿到 `aptXEncode_SetBitRate`，找它的 Encode 入口）。
4. 用户已明确：**先只做普通 aptX Adaptive，Lossless 暂缓**。

### 13.8 本轮新增工具（已入库）

| 文件 | 用途 |
|---|---|
| `aptx_test.py` | 配置矩阵实验驱动：写 drop-in→重启→选 profile→回放→抓包→统计 |
| `rtp_analyse.py` | 从 btmon(hci_mon) 抓包统计 RTP/TTP 速率、帧长、间隔、帧头分布 |
| `acl.py` | btsnoop 的 HCI ACL 重组器（按 PB 标志），解手机 btsnoop |
| `helper_probe.py` / `helper_sweep.py` | 直接驱动 QEMU helper，扫参数看 OTA 头 |
| `raw_probe.py` | 通过 helper 的 raw set_param 通道探测模块 param ID |

构建带日志的 helper（本轮关键）：
```bash
# compat.c 去掉 APTX_HAP_LOG 门控后
SRC_COMPAT=/tmp/compat_log.c \
  LD_LIBRARY_PATH=/nix/store/66lzffbmnizxfp8h1k03wbl8yhbsazgg-libcxx-22.1.8/lib:/nix/store/imladhnprigm0ffyr8ka5ymxbh5r3lxa-libunwind-22.1.8/lib \
  bash /home/baizhu945/work/openaptx/build-helper/build-any.sh <helper.c> /tmp/helper-log
# 注意：libgcc.so 要复制到 helper 同目录（rpath $ORIGIN）
```

### 13.9 本轮补充的实测（第四轮上半）

- **我们的 96 kHz 帧型和手机不一样**：手机 96k 段帧头 `8300b0a1`，我们
  （selector 3 / FORCE_RATE=96000）是 `8300f0a1`。用参考解码器
  （`aptx-adaptive-packet-header-strip -e` + `test-decoder.exe -x hq`）
  确认两者都解出 **96000 Hz 立体声**，所以 0xb0/0xf0 不是采样率差异，
  可能是信道模式或版本子码差异（待查）。48k 我们 `8300d0a1`、44.1k
  `8300c0a1`，与手机一致。
- **selector 映射实测**（encoder_rate=96000，CIE freq=0x20，1920 样本块）：
  `0/1→48k(d0)`、`2→44.1k(c0)`、`3→96k(f0)`、`4..7→48k(d0)`。
  96k 时 wrapper 每 2 个块出一个包 → 3840 样本/包（40 ms、period 0xa0），
  码率只有 ~133 kbps。
- **DELAY_REPORT 实测**：耳机对 aptX Adaptive 报 **260.0 ms**、对 aptX HD 报
  **235.0 ms**（差 25 ms = 一个 AD 帧）。我们都正确回了 ResponseAccept。
- **AVRCP 状态**：我们这边的 PlaybackStatus 一直是 `Stopped`（手机在 START
  后立刻发 `Playing`）。但 aptX HD 在同样 `Stopped` 状态下用户能听到，
  所以 AVRCP 状态不是门控条件。
- 96 kHz 端到端（OTA 头 + SOURCE_TYPE=0x00 + features 0x92 + STEREO）：
  流正常（25 fps、3840 样本/包、RTP 时钟 95.6k），仍静音。

### 13.10 第四轮上半：所有“码流/格式”假设都被否掉

用**手机自己的帧**做回放，四种组合全部静音：

| # | 回放内容 | 配置 | 结果 |
|---|---|---|---|
| 1 | 手机 48k 帧（OTA 原样） | 48k / stereo / f92 | 静音 |
| 2 | 手机 44.1k 帧（OTA 原样，含手机原始 TTP） | 44.1k / stereo / f92 | 静音 |
| 3 | 手机 44.1k 帧 + 干净单调 TTP | 44.1k / stereo / f92 | 静音 |
| 4 | 完整 R2.2 包（768 B、版本 0xaf、chan 0xa0、28 ms） | 44.1k / stereo / f92 | 静音 |

再加之前否掉的：OTA 头、SET_CONFIG、16 ms/338 kbps、96 kHz、只改版本字节。
**结论：静音与码流内容、OTA 头、帧长/码率、R2.2 格式都无关。**

同时确认（本轮新增证据）：

- 用参考解码器解手机 `aptx-adaptive-reference-48k96k.bin`：
  **secA（8300b0）解出 96000 Hz 单声道**、secB（8300d0）解出 48000 Hz 立体声；
  我们的 96k（8300f0）解出 96000 Hz 立体声。所以帧头第二字节同时编码
  采样率**和声道数**（b0=96k mono、f0=96k stereo、c0=44.1k、d0=48k），
  不是 bug。
- 手机的 44.1k 流（abr0）本来就是 **656 B / 25 ms = 210 kbps**，和我们一样。
  用户说的 ~50 kB/s 实测对应的是 **aptX HD**（我们实测 55 kB/s）。
  **所以码率也不是异常**——之前的“码率偏低”判断要撤回。
- 链路设置：我们的 HCI 里**没有** Write Link Policy / Packet Type / Flush
  Timeout / Sniff 相关命令；手机的日志里这些都有（还有 Enhanced Flush）。
  AD 媒体是 1 个 ACL 包/PDU，不分片。
- AVRCP：我们 PlaybackStatus 一直是 Stopped（手机 START 后立刻 Playing），
  但 aptX HD 在同样 Stopped 下用户能听到 → 不是门控。

### 13.11 第四轮下半：剩下的方向

到这里，“我们发的东西和手机发的东西一样”已经被穷尽验证。剩下的差异只在：

1. **RTP 头的起始 seq/ts**（我们从 0 开始，手机是 0x3a2/0x110760 之类的随机值）
   —— 需要改插件才能测。
2. **控制器/链路层**：手机是 Qualcomm 控制器 + A2DP offload，我们是 AX210 +
   主机侧 L2CAP；aptX Adaptive 的**反向反馈/时钟同步**在手机上走的是控制器→DSP
   的私有通道，我们根本收不到（30 s 抓包零反向 ACL）。
   → 下一步优先：用 **FiiO BT11（QCC5181）** 接耳机验证“Qualcomm 源能出声”，
   再用**手机当 sink**（需要 USB+adb）验证我们的源。
3. 若要继续纯本地推进，只能做**直接调用内层 R2 编码器**（绕过 wrapper）以
   拿到 12.5 ms 帧，但 13.10 已经说明帧长不是门控，优先级应降低。

### 13.12 第四轮下半：设备类型实验与手机日志新证据

- **Class of Device 实验（失败）**：把适配器 CoD 从 `0x7c010c`（Computer/Laptop）
  改成 `0x5a020c`（Phone/Smart phone）并重连耳机，AD 仍静音；已改回。
  → 耳机不按源设备类型做门控。
- **实时流内容校验**：抓下我们经蓝牙发出的 AD 流，用参考解码器解出
  44100 Hz 立体声、FFT 主峰 440.1 Hz，RMS -31 dBFS —— 确实是真实音频，
  不是静音帧（相对源文件 -4.3 dBFS 有约 24 dB 衰减，原因待查，但不影响
  可听性判断）。
- **手机最新 btsnoop（从 /data/log/bt 拉取）**：
  - 手机自己的 aptX Adaptive 源能力 = `f0 3e 50 64 64 64 ff ff 00 01 17 ...`
    （采样率位全开、信道模式 0x3e、features `0x0f000017`）。
  - 手机→耳机侧有一次 **RECONFIGURE 到 48 kHz 立体声**
    （value `10 02 50 64 64 64 ff ff 00 01 92 ...`）——说明手机会按内容
    采样率重配流，我们已测过 48k，不是门控。
  - 同一日志里还有我们电脑当 sink 的会话（我们的 SET_CONFIG
    `41 08 ... 92`，耳机侧 capabilities `71 0a ... 82`）。
- **TTP 复核**：模块自身 TTP 每 25 ms 只走 4 个单位（基本冻结），helper 的
  覆盖值每 25 ms 走 375~405（≈15000/s）——覆盖是正确的，模块原值不可用。
- **手机 A2DP sink 不可用**：`bluetooth.profile.a2dp.sink.enabled=false`，
  非 root 无法开启（用户明确不 root），实验 B 走不通。

### 13.13 剩下的可能性

到这一步，能被主机侧观测的差异已经全部对齐，只剩两类：

1. **控制器/空口层**：手机是 Qualcomm 控制器 + A2DP offload，BT11 是 QCC5181，
   我们 AX210 走主机 L2CAP。三者中只有我们不行。
   → 最便宜的判定实验：借一个**非 Intel 的 USB 蓝牙棒**（Realtek/CSR 等）插上
   试 AD。若换控制器能出声，说明是 AX210/Intel 侧的空口行为；若仍静音，
   说明是主机栈或耳机固件对 Qualcomm 源的依赖。
2. **耳机固件对 Qualcomm 源的依赖**：若属此类，主机侧再怎么改都无解，
   只能换 sink 或换控制器（且控制器要真能触发 Qualcomm 私有行为，
   而 Linux 的 BlueZ 不会用 QCNCM865 的 aptX offload，所以换卡大概率无效）。

---

## 14. 第五轮：手机 48 kHz / 24-bit 会话复抓（2026-09-10 上午）

用户把手机用 USB 连到电脑，让手机以 aptX Adaptive 播放 **48 kHz / 24-bit** 音乐给
MOMENTUM 5。本轮从 `/data/log/bt/btsnoop_hci_20260910_080910.log`（8849 帧，
拉到 `/tmp/phone-logs/live_080910.log`）复抓分析。

### 14.1 会话时间线（2026-09-10 09:11–09:26，ACP SEID 9 = 耳机 AD sink）

| 时刻 | 事件 | aptX Adaptive value（`d7 00 00 00 ad 00` 之后） |
|---|---|---|
| 09:11:43.871 | SET_CONFIG | `40 02 50 64 64 64 ff ff 00 01 92 00 00 0f 02 03 03 03 00 aa` → **44.1 kHz** |
| 09:11:43.963 | Open | — |
| 09:12:15.758 | **RECONFIGURE** | `10 02 50 64 64 64 ff ff 00 01 92 00 00 0f 02 03 03 03 00 aa` → **48 kHz**（48 kHz/24-bit 音乐） |
| 09:12:41.190 | Start | — |
| 09:12:50.441 | Suspend | — |
| 09:12:57.529 | Start（48 kHz 段持续播放） | — |
| 09:25:33.631 | Suspend（用户切歌到 44.1 kHz） | — |
| 09:25:41.017 | **RECONFIGURE** | `40 02 50 64 64 64 ff ff 00 01 92 00 00 0f 02 03 03 03 00 aa` → **44.1 kHz** ← 同一会话内切回 |
| 09:26:35.602 | Start（44.1 kHz 段持续播放到抓包结束） | — |

### 14.1.1 采样率位的双重确认（本轮最强证据）

同一条 手机→耳机 会话里，手机按内容采样率**先后重配**了两次，两次的 features /
其余字节完全相同，只有 byte6 不同：

| 内容采样率 | byte6 | 结论 |
|---|---|---|
| 44.1 kHz（曲目 1 / 用户切换后） | `0x40` | `0x40` = 44.1 kHz |
| 48 kHz / 24-bit（曲目 2） | `0x10` | `0x10` = 48 kHz |
| （无内容用 96 kHz，由耳机能力 `0x71` 位掩码 + 排除法） | `0x20` | `0x20` = 96 kHz |

两次切换都是用户按曲目实际采样率触发的、可复现的独立观测，因此
**“`0x40` = 192 kHz”彻底否定**——把 `bthost_ipc.h` 的 DSP 内部编号当成 A2DP
字段语义是早期那次误判的根源（§3 已加勘误）。

要点：

1. 手机**先按 44.1 kHz 建流（SET_CONFIG），Open 之后、Start 之前再 RECONFIGURE 到
   48 kHz**。也就是说 `0x40`/`0x10` 这对采样率位在同一次可用会话里先后出现，
   **再次证实 `0x40` = 44.1k、`0x10` = 48k**（与 §12.1 一致，与早期“192 kHz”说法相反）。
   本次勘误的直接依据就是这条：用户看到的 44.1 kHz 就是那个 `0x40`。
2. RECONFIGURE 后的 value 与我们主机侧能协商出来的**逐字节相同**（我们设
   `FORCE_RATE=48000` / `FREQ_BITS=0x10` 时；设 44100/`0x40` 时则与手机首次
   SET_CONFIG 一致）。features `0x0f000092`、`50 64 64 64`、`ff ff 00 01` 全部吻合。
   → “我们的 SET_CONFIG 不对”这条**彻底排除**。
3. 耳机能力（GET_CAP 响应）复抓一致：
   `71 0a 37 6a c8 7d 64 7d 00 01 82 00 00 0f 02 03 03 03 00 aa`
   → 采样率位 `0x70` = {44.1k, 48k, 96k}，features `0x0f000082`（无 R2.2 位）。
4. **手机侧 HCI 里依然完全没有媒体**：全文件 AVDTP 只有 24 帧信令；ACL 载荷里
   搜不到 `83 00 d0 a1` / `83 00 c0 a1` 之类的 AD 帧头，也没有 RTP（`80 60`）媒体包
   （两次抓取分别 8849 帧 / 4 MB 都验证过）。
   → 与之前结论一致：**手机把 aptX Adaptive 的编码/发送全部 offload 给高通控制器**，
   主机侧只能看到 AVDTP 信令。所以“44.1 kHz 或 48 kHz / 24-bit 的音乐在空口上长什么样”
   从手机日志里拿不到，只能靠 BT11 或空口抓包（§13.13 的判定实验）。
5. 日志里唯一的大包（约 508 B 载荷、成簇出现、间隔 ~1.5 ms）载荷中反复出现
   `20 04 01 00 6c 07 bd` 这类私有 TLV，是 HONOR 自己的私有/BLE 通道流量，
   **与音频无关**，不要误当成 AD 媒体（本次已专门核查）。
6. **耳机从不查询源端能力**：整段会话里耳机只发 ResponseAccept（12 条），
   没有一次 GetCapabilities 指向手机的 source SEP，也没有 DelayReport。
   → 我们**对外广告的 source capabilities 不可能是静音原因**，这条假设可以永久排除
   （此前一直没验证过）。
7. 顺带核对：手机日志里有大量 `Vendor Command 0xFD53/0xFD57/0xFD5E/0xFC17`
   （高通控制器私有 HCI 命令），我们这边完全没有——属于控制器侧差异，见 §13.13。

### 14.2 对本项目的意义

- 采样率位映射**完全确定**（`0x40`/`0x10`/`0x20` = 44.1k/48k/96k），
  **192 kHz 假设彻底作废**；§0/§3/§6/§7 的相应段落已加勘误标注。
- “耳机可能要求 24-bit 帧”这条不成立：§13.10 已经用**手机自己的 48k 原始帧**
  回放（逐字节一致）依然静音，与位深无关。
- 因此剩下的差异仍只有 §13.11 / §13.13 列的两类：**控制器/空口层**
  （AX210 主机侧 L2CAP vs 高通/QCC 链路）与**耳机固件对高通源的依赖**。
  最便宜的判别实验仍是插一个非 Intel 的 USB 蓝牙棒。
- 复抓脚本/工具沿用 §5：`adb pull /data/log/bt/<最新>.log` + `tshark`（AVDTP 信令）
  + `acl.py`（ACL 重组）；媒体存在性判据 = 搜 `83 00 ?? a1` 帧头与 `80 60` RTP 头。


---

## 15. 第五轮续：R2 入口点实验与模块内部决策链（2026-09-10 上午）

### 15.1 实验：改用模块的“纯 R2”入口（`aptx_adaptive2_enc_*`）

模块导出了三个 CAPI 入口，此前 helper 的 R2 路径只用其中一个：

| 入口 | 用途 | helper 是否使用 |
|---|---|---|
| `capi_aptx_adaptive_enc_init` / `_process_wrapper` | R2/R2.2 外层 wrapper | ✅ R2 路径用的就是它 |
| `aptx_adaptive3_enc_init` + `get_aptx_adaptive3_vtable` | R3（lossless） | ✅ R3 路径 |
| **`aptx_adaptive2_enc_init` + `get_aptx_adaptive2_vtable`** | 纯 R2（mode 2） | ❌ **从未用过** |

于是给 helper 加了 `APTX_R2_ENTRY=1` 开关切到 mode-2 入口
（源码 `/tmp/helper_r2entry.c`，`SRC_COMPAT=/tmp/compat_log.c` 构建，
产物 `/tmp/aptx-r2dir/helper-r2entry`，用 `helper_probe.py` 直接驱动）：

| 变体 | 输出 |
|---|---|
| 基线（CAPI wrapper） | 664 B 包（OTA 8 + 帧 656），OTA `… 64 01 00 00 00 ae`，TTP ≈15000/s |
| `APTX_R2_ENTRY=1`（mode 2） | **完全相同**：664 B、period `0x64`（25 ms）、ptype 1、版本 `0xae` |

→ **“25 ms 是 R2.2 wrapper 钉的”这个结论要修正**：纯 R2 入口一样是 25 ms/656 B。
包周期由编码器内部状态决定，与入口点无关。

### 15.2 模块日志：内部其实选的是 12 ms / 437 kbps

去掉 `compat.c` 的日志门控重建 helper 后拿到完整决策链（两种入口点都一样）：

```
INIT: Final bitrate after all limiting conditions in Kona is 364000 bps
INIT: Actual Period selected 14.0            # f64 0x402C000000000000
INIT DONE: Period encoded is 56              # 0x38 → 14 ms
AVS_ENCODER_PARAM_ID_BIT_RATE_LEVEL_MAP 3    # 我们送的表（kbps）已被采纳
   Selected Level=1..5  Bitrate=437          # 0x1b5 = 437 kbps
   Selected Period=12.0  PCMinterval=576     # 576 样本 = 12 ms @48k
IMCL_PARAM_ID_BT_BIT_RATE_LEVEL_ENCODER_FEEDBACK br_level = 3
PROCESS(): OUTPUT_BUFFER_HEADER written, me->period = 100   # 0x64 = 25 ms
```

两点要记下来：

1. **437 kbps × 12 ms ÷ 8 = 655.5 B ≈ 656 B**——656 字节载荷正好等于“12 ms @ 437 kbps”，
   而不是“25 ms @ 210 kbps”。所以这个 656 B 包应当是**若干内部帧的聚合**
   （2 × 12 ms ≈ 24 ms 音频），OTA 的 `period=0x64` 是**包间隔**（25 ms）而非单帧时长。
   实测相符：`helper_probe.py` 喂 1200 样本（25 ms）块时**每块恰好出 1 包**。
   → 不要再用“212 kbps 低于 279 kbps 下限”解释静音（该判断已在 §12.3/§13.6 撤回）。
2. 模块内部想要 12–14 ms，外层 wrapper 发 25 ms 包；而手机自己发出的包也是
   656 B / 25 ms 间隔（§8、§13.1），形态一致，所以这**仍然不是**静音原因。

### 15.3 结论（本轮最终）

- 源端可观测的一切均已对齐：SET_CONFIG 字节、RTP 头、OTA 头、帧形态、包间隔、
  控制流，外加“回放手机原始 PDU 仍静音”（§13.10）。
- 因此差异只能落在**源设备/链路层**：AX210 是 Intel 控制器 + 主机侧 L2CAP，
  而两个能出声的源（HONOR 90GT、FiiO BT11）都是**高通控制器 + AD offload**。
- ⚠️ 因此“换一个非 Intel 的蓝牙棒”**不能**区分“Intel 特有毛病”与“必须是高通源”；
  要区分必须用**高通控制器**的适配器（换 Realtek/CSR 仍失败则说明是高通源依赖）。

---

## 16. 第六轮：BT11 判定实验 + 首次产出真正的 R2.2/R3 包形（2026-09-10 上午）

### 16.1 BT11 能否当“高通控制器”用？——**不能**

用户把 FIIO BT11 插到本机并连上耳机（默认协商为 **aptX Lossless**），本机蓝牙关闭。
`lsusb` / USB 描述符实测：

```
0a12:4007  FIIO BT11 (UAC1.0)   bNumConfigurations=1  bDeviceClass=0
  iface 0: HID      1 endpoint  (consumer control / hidraw9)
  iface 1: HID      2 endpoints (IN+OUT, 厂商自定义 = hidraw10)
  iface 2: Audio    Control
  iface 3: Audio    Streaming, 两个 altset（384 B / 576 B 包）
```

- **没有任何 Wireless(0xE0) 接口** → 不提供 HCI 传输，`btusb` 无法绑定，
  也就**不能把 BT11 当作本机的蓝牙控制器**。这条“不用买硬件就能拿到高通控制器”
  的路走不通。
- 厂商 HID 通道（Usage Page `0xFF00`）有 report ID 1/2/3/4/5/6/7/8/9
  （62 / 12 / 11 / 446 字节等）——这是 FiiO 上位机/DFU 协议；**空闲时设备不主动上报**
  （读 hidraw10 五秒零数据），需要按协议轮询。
- 结合 §16.2 的固件映射：BT11 的链路逻辑在**主机固件分区**（熵 7.07、无字符串、
  非 ARM/XAP 可识别代码）里，改造它加 HCI 模式不现实。
- BT11 在本机呈现为 PipeWire sink（设备 94 “Mpow HC5 …”，= 0a12:4007 的 UAC 口），
  所以“PC → BT11 → 耳机”这条外接路径可用（但不是本项目要的路径）。

### 16.2 关键收获：本机第一次产出真正的 768 B / ptype 5 包

用 `helper_probe.py` 直接驱动 helper（`/tmp/probe6.py`、`/tmp/probe7.py`）扫参数，
发现之前 all “lossless 实验”其实**都没有真正进入 2.2/Lossless 形态**，原因有两个：

1. helper 里有降级闸门：`lossless=force` 且 mode≠R3 时，**除非**
   `APTX_ADAPTIVE_ALLOW_UNSTABLE_LOSSLESS=1`，否则会把 `lossless_mode` 改回 OFF
   （并打印 “downgrading to ordinary aptX Adaptive”）。→ 之前的 lossless 配置全都
   静默退化成普通 R2（664 B / ptype 1 / `0xae`）。
2. 2.2/Lossless 形态**要求源字长提示为 16 bit**：同一配置下 `bits=32` 出
   **664 B（R2 形态）**、`bits=16` 才出 **768 B（ptype 5、chan 0xa0）**。

修正后的实测（44.1 kHz、CIE 44.1k stereo f92）：

| 配置 | 输出 |
|---|---|
| `mode=2 lossless=force qhs=1 bits=16` | **768 B**，OTA `.. 90 05 a0 00 00 ae`，帧头 **`21 87`**（R3 形态） |
| 同上 + `APTX_OTA_VERSION=0xad` | **768 B**，OTA `.. 90 05 a0 00 00 ad` ← 与 BT11 的 Lossless 形态一致 |
| 同上 + `APTX_OTA_VERSION=0xaf` | **768 B**，OTA `.. 90 05 a0 00 00 af` ← R2.2 形态 |
| `bits=32` 同配置 | 664 B（R2 形态，`83 00 ..`） |

顺带修掉/查明两个真 bug：

- R3 入口的静态属性用的是 R2 入口的 `capi_aptx_adaptive_enc_get_static_properties`，
  应为 `aptx_adaptive3_enc_get_static_properties`（已修，见 `/tmp/helper_r3fix.c`）。
- `aptx_adaptive3_enc_init` + `set_profile` 只接受 **profile ∈ {2, 3, 6}**（0 视作 6），
  其它值（1/4/5/0x1000）直接 `EIO` 让整个 config 失败。

### 16.3 新增的两个运行期开关（helper 侧，禁用时完全无副作用）

```c
APTX_OTA_VERSION=0xNN   /* 覆盖 OTA 版本字节：0xad=R3、0xaf=R2.2、0xae=R2 */
APTX_FORCE_BITS=16|32   /* 覆盖交给 2.2 状态机的源字长提示 */
```

实验 helper 已装到运行时目录（**不覆盖**原 helper）：
`/home/baizhu945/Documents/aptx-adaptive-runtime/helper/aptx-lossless-helper-exp`，
配套 drop-in 暂存于 `/tmp/zzz-aptx-losstest.conf`（含
`APTX_ADAPTIVE_LOSSLESS=force`、`APTX_ADAPTIVE_QHS_SUPPORT=1`、`APTX_FORCE_BITS=16`、
`APTX_OTA_VERSION=0xad`），**尚未启用**。

### 16.4 下一步

把上面三种“与 BT11 同形”的流真正放到空口上试（需要耳机空闲 + 本机蓝牙开启）：

1. `0xad`（R3/Lossless 形态，与 BT11 相同）——第一次测，最有信息量；
2. `0xaf`（R2.2 形态，普通 Adaptive 的 Snapdragon Sound 变体）；
3. 对照组仍是 `0xae`（R2 形态，已知静音）。

判定逻辑：若 `0xad`/`0xaf` 出声而 `0xae` 不出声 → 耳机只吃 Snapdragon Sound
包形，问题不在链路而在**包形/版本**，普通 AD 就需要把 wrapper 顶到等级 ≥6（§13.7.0）；
若三者都静音 → 差异确定在控制器/空口层（§13.13）。

---

## 17. 第七轮：部署方式说明 + 普通 AD 的最后几个变量（2026-09-10）

### 17.1 部署结构（为什么改代码不用改 `/etc/nixos`、不用 rebuild）

系统里只有**两样东西**是 Nix 构建的（`/etc/nixos/pipewire-aptx-adaptive-module.nix`）：

| 组件 | 来源 | 改动代价 |
|---|---|---|
| PipeWire 插件 `libspa-codec-bluez5-aptx-adaptive.so` | `pkgs.pipewire.overrideAttrs`，pin fork rev `2c1c2ca` | **必须** nixos-rebuild（或改 `src` 指向本地工作树重建） |
| openaptx 库（插件的 C API） | `fetchFromGitHub` rev `c5fdab9` | 同上 |
| **Hexagon helper 二进制** | `~/Documents/aptx-adaptive-runtime/helper/aptx-lossless-helper`，由 env
`PIPEWIRE_APTX_ADAPTIVE_HELPER` 指定 | **纯运行期文件**：用 hexagon-clang + qemu 重新编译后直接覆盖，**不需要 Nix** |

因此本轮的“切换传输方案”全部走**运行期**两条路，都不动 `/etc/nixos`：

1. **换 helper 二进制**：`build-any.sh` 编出 /tmp 里的实验版，再拷成
   `.../helper/aptx-lossless-helper-exp`（**原 helper 文件保持不动**）。
2. **用户级 systemd drop-in**：`~/.config/systemd/user/{pipewire,wireplumber}.service.d/zzz-aptx-losstest.conf`
   里改 `PIPEWIRE_APTX_ADAPTIVE_HELPER` 与各项 env 开关，然后
   `systemctl --user daemon-reload && systemctl --user restart pipewire.socket pipewire wireplumber`。

插件的各种 env 开关（`APTX_ADAPTIVE_FORCE_RATE`/`FEATURES`/`FREQ_BITS`/`LOSSLESS`/
`CODEC_FRAMES`/`QHS_SUPPORT`/`STRIP_OTA`/`REPLAY`…）**在已部署的 pin rev 里就有**，
所以调它们也不需要重编。只有改**插件源码本身**（例如格式列表顺序、MTU 处理）才需要 rebuild；
那时可以（且只能）把模块的 `src` 指向本地工作树 `/home/baizhu945/work/pipewire` 重建，
**不必**向 pipewire fork 提交（用户要求只提交 openaptx）。

回退：删掉上面的 drop-in → `daemon-reload` → 重启 pipewire 即可（原 helper 未被覆盖）。

### 17.2 helper 新增的运行期开关（默认全部无效）

| env | 作用 |
|---|---|
| `APTX_FORCE_BITS=16\|32` | 覆盖交给 2.2 状态机的源字长提示 |
| `APTX_OTA_VERSION=0xNN`（或写 `/tmp/aptx_ota_version`） | 覆盖 OTA 版本字节（0xae/0xad/0xaf），文件方式无需重启 |
| `APTX_TTP_MODE=audio` + `APTX_TTP_OFFSET` | TTP 改用**音频时钟域**：`TTP = samples/rate*15000 + offset` |
| `APTX_DUMP_PORTS` / `APTX_DUMP_R2_STATE` | 打印 CAPI 两个输出口长度与模块内部状态 |

### 17.3 普通 AD（非 Lossless）本轮结论

用户明确：**手机抓到的是非 lossless 的 AD，只测非 lossless**。于是把 Lossless 线路全部撤掉，
只保留普通 AD，并把剩下所有可观测变量都过了一遍：

| 变量 | 我们的值 | 与手机（可用源）对比 | 结果 |
|---|---|---|---|
| SET_CONFIG | `d7 00 00 00 ad 00 40 02 50 64 64 64 ff ff 00 01 92 …` | **逐字节相同** | 静音 |
| RTP 头 | `80 60 <seq> <ts> 00000000`，ts 每包 +1102 | 相同 | 静音 |
| 帧 | 656 B，帧头 `8300c0a1`（44.1k 立体声） | 相同 | 静音 |
| 节奏 / 速率 | 25 ms/包，**217 kbps** | 手机流同为 656 B / 25 ms ≈ 210–217 kbps | **无异常** |
| OTA | period `0x64`、ptype 1、chan 0、ver `0xae` | 相同 | 静音 |
| TTP 时基 | 墙钟 / 冻结 / **音频时钟域**（= RTP 时间戳 + 235–260 ms，实测 15000/s、每包 +375） | — | **仍静音** |

**结论**：普通 AD 的可观测部分（含把手机原始包逐字节回放）已经全部对齐且无速率异常，
TTP 时基也排除。剩下的差异只能在**控制器/空口层**：我们是 Intel AX210 + 主机侧 L2CAP，
而两个可用源（HONOR 90GT、FIIO BT11）都是**高通控制器 + A2DP offload**（§13.13）。

可选下一步：

1. 换一个**非 Intel 的蓝牙适配器**（30 元级 Realtek/CSR）：能出声 → 是 Intel/AX210 的问题；
   仍静音 → 说明必须是高通源。
2. 换 M.2 的**高通控制器**（如 QCNCM865，本机内部、不算外接 USB）：只有在“耳机需要高通源”
   这一类原因下才有意义；注意此前的答复需要修正——**不能**用“BlueZ 不用高通的 AD offload”
   直接否定它，因为码流本来就是我们主机侧编的，控制器只需提供链路行为。
3. 暂时使用已在工作的 **aptX HD**（实测 55 kB/s ≈ 440 kbps，反而比我们 AD 的 217 kbps 更高）。

---

## 18. 第八轮：找到“手机为什么行、我们为什么不行”的直接证据（2026-09-10）

在手机 btsnoop（`/tmp/phone-logs/live2.log`）里把 **AD 流建立前后的所有 HCI 命令**按时间排开，
发现高通主机栈在 AD 流建立的每一步都向**控制器**下发厂商专用命令（我们这边完全没有）：

| 时刻 | 命令 | 说明 |
|---|---|---|
| 09:12:15.748（RECONFIGURE 到 48 kHz 的同一毫秒） | `Write` **Vendor 0xFC0A**，参数 `130105020010` | 切换/配置 offload |
| 09:12:28.632 | Vendor `0xFD59`（无参数） | 链路/测量配置 |
| 09:12:34.26～34.28 | Vendor **`0xFD57` × 5**（参数 `0001`） | 反复配置 |
| 09:12:41.330（Start 的同一毫秒） | Vendor **`0xFC0A`**，参数 **66 字节** | **把整份 AD 编解码配置交给控制器** |
| 09:11:43～09:12:41 | `Write Link Policy Settings`（`0x0007`→`0x0005`→`0x0002`）、`Change Connection Packet Type (0xcc18)`、`Sniff Subrating`、`Exit Sniff Mode` | 流期间关闭 sniff |

关键的是 09:12:41 那条 `0xFC0A` 的 66 字节参数，逐字节就是 A2DP 的 AD 配置：

```
0a0a000a0000000002020002004602eb0301000096002a00ff
d7 00 00 00  ad 00  10 02  50 64 64 64 ff ff 00 01 92 00 00 0f 02 03 03 03 00 aa …
  vendor id   codec  freq chan  ← 与 AVDTP SET_CONFIG 的 value 完全相同
```

也就是说：**手机并不是自己在主机里编 AD，而是把整份配置交给高通控制器，由控制器的
DSP/offload 完成编码、OTA 头、TTP、以及链路调度**（这也解释了为什么手机的 btsnoop 里
媒体永远不可见）。BT11（QCC5181）是同一套 offload 路径。

而我们的路径是：Intel AX210 + BlueZ（**不做 A2DP offload**）+ 主机侧编码。
即使把码流做到与手机**逐字节相同**（§13.10），甚至把 TTP 也改到音频时钟域（§17.3），
耳机依旧静音——因为耳机面对的是一个**没有走高通 offload 管线的对端**。

### 18.1 结论与建议

- 主机侧软件可观测/可控制的变量已经**穷尽**（配置、RTP/OTA、帧型、节奏、速率、TTP 时基、
  回放手机原始包），全部与可用源一致且无速率异常。
- 直接证据表明差异在**控制器侧的高通 offload/厂商命令管线**。
- 由此对“换硬件”的判断也要修正为：
  1. 换**非 Intel 适配器**：能判别“是不是 Intel/AX210 特有的问题”，便宜（30 元级）；
  2. 换**高通控制器**（如 M.2 的 QCNCM865）：只有当耳机只是要求对端为高通控制器时才有用。
     注意：Linux/BlueZ **不会**下发上面这些厂商命令，所以即使换高通卡，也**不会**复现
     手机的 offload 管线；能否出声取决于耳机要的到底是“高通控制器”还是“高通 offload 管线”。
  3. 若耳机要的是后者（从证据看很可能），则**在不外接高通硬件的前提下无法实现**，
     只能继续用已经可用的 aptX HD，或接受 BT11/手机作为源。

---

## 19. 第九轮：空口取证（Ubertooth）准备 + 链路层实验否掉（2026-09-10）

### 19.1 先把 Android 的链路设置原样套上——**仍静音**

从手机 btsnoop 解出的 A2DP 链路设置（§18）逐条在我们的链路上重放：

| 命令 | 参数 | 结果 |
|---|---|---|
| `Change Connection Packet Type`（0x01/0x000F） | `0xcc18`（只用 BR：DH1/DM1/DH3/DM3/DH5/DM5） | Command Status 0x00 ✓ |
| `Write Link Policy Settings`（0x02/0x000D） | `0x0005`（hold 开、sniff 关） | Command Complete 0x00 ✓ |

链路仍然健康（656 B/25 ms = 217 kbps、无间隙、无速率异常），**耳机依旧静音**。
→ 链路层"包型 / 链路策略"这一项也排除。脚本：`/tmp/apply_link_mode.sh`。

### 19.2 嗅探计划（Ubertooth One）

剩下的可能只有两类：**(A) 对端控制器侧的高通行为**、**(B) 耳机固件内部判断**。
空口嗅探正好能一刀切开这两类。

地址（跟随时需要 master 的 LAP，解码需要 UAP）：

| 设备 | BD_ADDR | LAP | UAP |
|---|---|---|---|
| 本机 AX210 | `FC:B3:AA:C5:01:42` | `0xC50142` | `0xAA` |
| 手机 HONOR 90GT | `44:90:46:40:FD:DD` | `0x40FDDD` | `0x46` |

三份抓包：

1. **手机 → 耳机（播放中）= 可用参照**：看**反向**（耳机→手机）有没有说 LMP/厂商 LMP/ACL
   —— 这一层 HCI 完全看不到，是我们至今的盲区；
2. **本机 → 耳机（我们的 AD 流）= 失败对照**：同一套分析，第一次能看到我们链路的 LMP 层、
   空口包型/长度、重传与 ACK 时序（AX210 是 Wi-Fi/BT 二合一，共存抖动只能在空口看到）；
3. 手机 → 耳机（暂停）= 基线，区分"连接固有"与"播放相关"。

判读：

- ①有、②没有的反向交互 → 原因是**耳机要求高通对端控制器**（并能进一步区分是"控制器级"
  还是"offload 管线级"，后者在 Linux 侧无解）；
- 两条链路空口行为完全一致 → 判断发生在**耳机内部**，与空口无关 → 主机侧任何改动都无效。

**Ubertooth One 的能力边界**：能完整解 BR(GFSK) 包与 **LMP PDU**（LMP 不加密）；
**解不了 EDR(2-DHx/3-DHx) 载荷**，但 EDR 包头也是 GFSK，所以类型/长度/时序仍可见
（媒体载荷本来就有，不需要）。加密 ACL 载荷需要 link key（`/var/lib/bluetooth/**/info` 里有）。

工具：

```bash
nix-shell -p ubertooth        # 只带 host 工具（ubertooth-rx / -util / -dfu），不带 .dfu 固件
ubertooth-util -v             # 验固件与设备（现在报 could not open device = 未插）
ubertooth-rx -l 0x40FDDD -r phone-air.pcap   # 跟随手机↔耳机
ubertooth-rx -l 0xC50142 -r host-air.pcap    # 跟随本机↔耳机
python3 air_analyse.py phone-air.pcap host-air.pcap   # 出对比摘要
```

`air_analyse.py`（本轮新增，已随仓库提交）会打印协议层次、方向统计、消息直方图、
LMP opcode 直方图、空口包型直方图与 L2CAP/AVDTP/AVRCP 帧数，对 HCI 抓包也能优雅降级。

---

## 20. 第十轮：代码审查与修复（2026-09-10 晚）

用户提供了自己的对比调查（`~/Documents/aptx-adaptive-vs-qualcomm/`，官方 offload 基线 +
桥接层逐行证据），要求**先全面审阅并修掉 BUG/风险，再插 Ubertooth 做空口取证**。
完整清单见 `CODE-REVIEW.md`，要点：

### 20.1 修掉的 BUG（14 项，含 1 项 high）

| 编号 | 位置 | 问题 |
|---|---|---|
| B1 | 插件 `write_full()` | **写路径没有超时**：helper 卡住 → 管道写满 → PipeWire 数据线程永久阻塞（唯一能把数据线程挂死的点） |
| B2 | 插件 OTA 剥离 | `APTX_ADAPTIVE_STRIP_OTA` 用 `getenv()!=NULL` 判定，**设成 `0` 也会剥 OTA**（即已知的错误线格式），且无法关闭 |
| B3 | 插件 `select_config` | `APTX_ADAPTIVE_FEATURES` 在"清 R2.2 位"之后整字重写，**把刚清掉的位又加回来**，只靠 helper 兜底 |
| B4 | 插件 ABR | `abr_level_for_unsent()` 把 `media-sink` 传来的 **backlog（未发字节）当成空闲空间**，等级映射完全反向 |
| B5 | helper R3 | R3 模式却向 **R2 wrapper** 要静态属性 → 初始化内存需求错误 → `aptx_adaptive3_enc_init` 直接 `EIO`（直编 R3 一直起不来的原因） |
| B6 | helper R3 | 实验版**无条件**把 `packet[7]` 改成 `0xad`，可能与模块自身状态机产出的载荷不一致 → 已回退为"仅当模块留 0 时兜底" |
| B7 | helper OTA 覆盖 | 版本字节从**固定且全局可写的 `/tmp/aptx_ota_version`** 读取（任何本地用户都能改音频流头）→ 改为 `APTX_OTA_VERSION_FILE` 指定路径 + 只接受 0xad/0xae/0xaf |
| B8 | 插件 `spawn_helper()` | 子进程继承父进程**被屏蔽的信号掩码**（PipeWire 屏蔽 SIGINT/SIGTERM → `SIGTERM` 收不到，reap 只能靠 SIGKILL）和**全部 fd** → 已清掩码、重置 SIGPIPE、关闭 ≥3 的描述符 |
| B9–B11 | 插件 | 超大应答静默丢包、MTU 过小无提示、控制载荷只按 `UINT32_MAX` 限制 → 均已加日志/上限 |
| B12 | 运行时 | 部署的是 `aptx-lossless-helper-exp`，而其源码（`/tmp/helper_ttpaudio.c`）**已不在磁盘** → 不可复现。现已把开关折回仓库源码并**用仓库源码重建**部署二进制，删掉 `-exp` 与对应 drop-in |
| B13 | NixOS 模块 | `adaptiveEnv` 里一直带着 `STRIP_OTA=1`、`CAPTURE=/tmp/...`、`FORCE_RATE=44100`，**每个会话**都生效（哪怕在用 aptX HD）→ 已移除 |
| B14 | NixOS 模块 | 同一组变量通过 `environment.variables` 泄漏到**全系统每个进程** → 已改为只给两个音频服务 |
| B15 | 插件 `get_delay()` | 上报 0 采样（仅抽取时 7），节点延迟漏掉编码块 → 改为"一个编码块 + 半带滤波器"（与 LDAC 的 one-frame 惯例一致） |
| B16 | 模块注释 | 原文称 `bluez5.codecs` 顺序决定优先级（错，优先级来自 `codec_order()`，AD 排在 HD 之前）→ 已更正并写明"想彻底避免误选静音就从白名单删掉 aptx_adaptive" |

### 20.2 记录在案但不修的风险

W^X 关闭（QEMU TCG 必需，`enable=false` 可恢复）、专有 blob 许可、
**ABR 实际无效（固定码率）**、TTP 为主机合成（默认已改为音频时钟域）、
R3 路径不完整（游标保护返回 `-EOVERFLOW`）、以及耳机静音本身（待空口取证）。

### 20.3 本轮验证

- helper 用仓库源码 + 仓库 `compat.c` 重建，普通 AD 回归通过：664 B / 25 ms /
  OTA `… 64 01 00 00 00 ae` / 帧头 `8300c0a1` / TTP 每包 +375（音频时钟域，基准 3900）；
- 开关逐项验证：`APTX_TTP_MODE=wall`（旧行为）、`APTX_OTA_VERSION=0xaf`（生效）、
  `=0x99`（拒绝）、`APTX_OTA_VERSION_FILE=<路径>`（生效）、`APTX_FORCE_BITS=16`
  （切到 768 B/ptype 5 形态）、`mode=r3`（配置成功）；
- 插件改动前后用真实头文件做 `-fsyntax-only` 对比，诊断数量一致（无新增错误/告警）；
- 运行时 drop-in 只剩 `zzz-aptx-phone-exact.conf`（`SOURCE_TYPE=0x00` +
  `CHANNEL_MODE=stereo`，`FORCE_RATE` 注释掉、`FEATURES` 不再设置），
  已用 `/proc/<wireplumber>/environ` 核对生效结果。

新增工具：`link_mode.sh`（把 Android 的 BR-only 包型 + 链路策略套到本机链路，
用于对照实验）。空口取证计划仍见 §19，等设备接入即可开始。

### 20.4 部署与部署后验证（本轮完成）

- 插件修复提交到 fork：`baizhu945/pipewire` `b97eae8`，并把
  `/etc/nixos/pipewire-aptx-adaptive-module.nix` 的 pin 从 `2c1c2ca` 换到
  `b97eae8c84b3b253429461abbb7c6c099cbe36cc`，`nixos-rebuild switch` 成功；
  用 `/proc/<wireplumber>/maps` 确认运行中的插件确实来自新 store 路径。
- 模块同时删掉了 `STRIP_OTA=1` / `CAPTURE=…` / `FORCE_RATE=44100` 三个"实验遗留"
  以及 `environment.variables = adaptiveEnv`（那行把编码器配置泄漏到全系统）。
  注意还有一个**陈旧状态**要手工清：用户级 systemd manager 的环境里仍留着旧值，
  用 `systemctl --user unset-environment APTX_ADAPTIVE_FORCE_RATE
  APTX_ADAPTIVE_CAPTURE APTX_ADAPTIVE_STRIP_OTA` 清掉后重启音频服务。
- 运行时 drop-in 收敛为 `zzz-aptx-phone-exact.conf`（`SOURCE_TYPE=0x00` +
  `CHANNEL_MODE=stereo`；`FORCE_RATE` 注释掉、`FEATURES` 不再设置）。
- 部署后端到端验证（耳机已连、AD 编码）：487 个媒体包、656 B 帧、25 ms 节奏、
  OTA `… 64 01 00 00 00 ae` **保留**（证明 STRIP_OTA 遗留已消失）、
  帧头 `8300d0a1`（48 kHz 立体声，速率不再被强制成 44.1 kHz）、
  217 kbps —— 与手机抓到的形态一致。

---

## 21. 第十一轮：空口取证执行（Ubertooth One 上场，2026-09-11）

### 21.1 设备与工具链

- Ubertooth One（`1d50:6002`），固件 **2020-12-R1 (API 1.07)**，与 host 工具版本一致，
  可直接工作；设备节点是 `root:root`，需 root 调用。
- 工具：`ubertooth-rx -z`（survey）、`-l <LAP> -u <UAP>`（跟随）、`-e <n>`（接入码容错）、
  `ubertooth-btle -n`（BLE 广播）、`ubertooth-afh`（AFH 信道图）。
- `air_analyse.py` 里**写死的 tshark store 路径会随 GC 失效**（本轮踩到：store 路径残缺时
  tshark 直接 SIGBUS）——需要改成动态解析，见 §21.6。

### 21.2 方法学上踩到的三个坑（都会造成"假阴性"）

1. **survey 的检测能力带 ~60 秒周期**：`6a8fcc` 的静默间隔是 55/56/56/56 秒、
   `ee02f8` 是 60/61/61/60 秒。所以"survey 里没有某个 LAP"**不能**证明链路不存在。
2. **跟随模式命中率极低**：我们自己的 AD 链路（RSSI 饱和）80 秒只抓到 7 个包，
   且帧头解白化失败（`packet_header=0`）→ **EDR 载荷与 LMP 基本拿不到**，
   只有 BR 包才有载荷。
3. **只用 `-l` 不给 `-u` 时不会进入跳频跟随**（会停在默认信道 39），必须给 UAP。

### 21.3 正对照：我们自己的 AD 链路是可见的

`ubertooth-rx -z` 直接看到 `LAP=c50142`（本机 AX210），RSSI 饱和（`s=0/-16`、`snr=55`）；
`-l 0xC50142 -u 0xAA` 能锁定（`CLK6 found`）并抓到包。
**⇒「aptX Adaptive 本身导致抓不到」不成立**，差异只能在链路层模式上。

### 21.4 手机链路只在"重连瞬间"出现在标准 BR/EDR 上

地址核实：手机 = HONOR 90 GT `44:90:46:40:FD:DD`（BlueZ 配对记录，CoD `0x5a420c`），
LAP `0x40FDDD` / UAP `0x46`；耳机 = MOMENTUM 5 `80:C3:BA:B7:16:3B`（CoD `0x2c0404`，
LMP version `0x0D`、**manufacturer `0x001D` = Qualcomm/CSR (41)**、subversion `0x75D4`）。
注意 `0x40FCD8` 是**另一台**设备（UAP 0x4D/0xE9），与手机只差 3 bit，容易误判。

三次让用户做对照动作的实验：

| 实验 | 动作 | 观测 |
|---|---|---|
| A | 关手机蓝牙 40 秒再开 | survey 里 `40fddd` 只在**恢复瞬间**出现（t=68s，RSSI 0） |
| B | 关耳机电源 40 秒再开 | survey 里 `40fddd` 只在**重连瞬间**出现（bin 60-70，RSSI −5…−36） |
| C | 重连瞬间跟随 `0x40FDDD/0x46` | 跟随器**锁定成功**（`CLK100ns Trim: 5439`）并抓到 3 个包（ch 19/33，RSSI −28…−40），**之后 180 秒 0 个标准包** |

稳态播放中（用户确认耳机一直有声、手机贴着 Ubertooth）：

- 跟随 `0x40FDDD/0x46` 三次（100s / 60s / 90s，含 `-e 4`）→ **全部 0 包**；
- 跟随耳机自己的 LAP `0xB7163B/0xBA` 60 秒 → 0 包；
- 180 秒 survey → `40fddd` 0 次，而同一次 survey 里 `6a8fcc` 68 次、`ee02f8` 57 次、
  `9ba8a3` 29 次（探测能力是够的）。

### 21.5 结论：手机的 AD 音频不在标准 BR/EDR 上 ⇒ 高通 QHS

- 高通 **QHS（Qualcomm High Speed）是专有物理层，速率最高 6 Mbps**，而传统蓝牙只有
  1/2/3 Mbps；**仅在两端都支持 QHS 时生效**，且要求"不影响 LMP 状态机、可重新协商回
  BR/EDR"（见 [高通平台蓝牙学习——QHS](https://blog.csdn.net/weixin_47456647/article/details/150919770)）。
- 观测形态与之完全吻合：**连接/重连时走标准 BR/EDR（LMP 协商），协商完成后整条链路切到
  QHS**；Ubertooth 只能解 1 Mbps GFSK、只能读 2/3 Mbps 的包头 → 切换后彻底看不见。
- 反证：若音频走标准 EDR，跟随器锁定后必然持续看到 GFSK 包头（对照组 `c50142` 就是这样）。
  **"锁定成功 + 之后完全静默"只有换物理层能解释。**
- 手机 UI 显示 aptX Adaptive ⇒ 传输是 A2DP/AD（不是 LE Audio/LC3）⇒ 与 QHS 自洽。
- 三个数据点一致：**能出声的源（HONOR 90 GT、FiiO BT11/QCC5181）都是高通+QHS；
  不出声的源（AX210/Intel）没有 QHS。**

### 21.6 对项目的影响与下一步

- 若耳机"只在 QHS 链路上播 AD"，则**主机侧标准 EDR 的 AD 码流无论编码多正确都不会出声**
  —— 这正好解释 §13.10「回放手机原始 PDU 仍静音」。
- 但**尚未证明 QHS 是必要条件**（只证明了手机确实用它）。可做的廉价实验：
  1. `APTX_ADAPTIVE_QHS_SUPPORT=1`（helper 已有该开关，当前部署为 `0`）；
  2. OTA 版本切到 **R2.2 / Snapdragon Sound 形态 `0xaf`**（`APTX_OTA_VERSION=0xaf`）——
     手机是骁龙设备，很可能用 Snapdragon Sound 变体而不是我们一直发的 `0xae`；
  3. 两者组合。
- 附带待修：`air_analyse.py` 写死的 tshark 路径（已在 `b664a90` 修掉）。

### 21.7 实测：`APTX_ADAPTIVE_LOSSLESS=auto` 是一个**速率陷阱**（本轮发现）

想“只恢复 R2.2 能力位、不产生 lossless 码流”，最省事的做法是把 `LOSSLESS` 从 `off`
改成 `auto`：48 kHz/24 bit 下 `lossless_eligible` 恒为假（它要求 44.1 kHz + 16 bit），
所以 helper 不会发 QHS/16-bit 边带反馈、也不会进 lossless 状态。**实测证明这个推理错了。**

用 `btmon` 抓本机 HCI（`datalink 2001`，记录里能直接搜到 `d7 00 00 00 ad 00` 之后的
CIE 字节）对比同一个 48 kHz/24 bit 音源：

| 配置 | 媒体包 | L2CAP 长度 | 包间隔 | 吞吐 | OTA 头 |
|---|---|---|---|---|---|
| `LOSSLESS=off`（正常） | 487 | 676 | 25 ms | ~27 kB/s | `3c 0f 64 01 **00** 00 00 ae` |
| `LOSSLESS=auto` | 6104 | 676 | **50 ms** | **13.53 kB/s** | `e8 83 78 01 **a0** 00 00 ae` |

即：帧大小没变，但 **`packet_type` 从 `0x00` 变成 `0xa0`、包间隔翻倍到 50 ms，
码率被砍半到 ~107 kbps** —— 这是一个用户肉眼可见的速率异常（用户实测 13.37 kB/s，
与这里的 13.53 kB/s 一致）。**结论：`auto` 会让编码器进入 R2.2 形态，而不是“只改一个位”。**

同一次实验还从 HCI 里读到了双方的 CIE（这是第一次在**我们自己的链路**上逐字节对照）：

- 耳机 `GetAllCapabilities`：`d7 00 00 00 ad 00 **71 0a** … **82** 00 00 0f …`
  （采样率 0x71、通道 0x0a、features `0x0f000082` —— 耳机**自己是支持 R2.2 的**）
- 我们 `SetConfiguration`（`auto` 时）：`d7 00 00 00 ad 00 40 02 50 64 64 64 ff ff 00 01
  **92** 00 00 0f 02 03 03 03 00 aa` —— 与手机 btsnoop 里的 SET_CONFIG **逐字节相同**。

**即便如此耳机仍然没有任何声音。** 所以「能力位不够/CIE 不一致」这条假设可以排除了；
结合 §21.5 的空口结论，剩下的差异只在**链路层（QHS）**。

注意这次实验同时改了两个变量（能力位 + 编码器形态），所以"能力位无害"这一点**并不能**
由它否定；要单独验证能力位，需要改插件把"对外通告 R2.2"与"编码器 lossless 模式"解耦
（当前二者绑在同一个 `lossless_enabled()` 上）。

实验已回退：drop-in `zzz-aptx-r22-advert.conf` 已删除，`APTX_ADAPTIVE_LOSSLESS` 回到
`off`，服务已重启，耳机已从本机断开。

### 21.8 强制前置门禁（用户要求：测试前必须确认编解码与输出设备）

**规则（以后每一轮都照此执行）**：任何编码器/空口测试之前先跑 `preflight.sh` 并通过；
播放开始后再跑 `stream_check.py` 并通过。任一步不通过，该次测试结果作废。

1. `preflight.sh [--fix]` —— 检查 hci0 是否 UP、耳机是否已连、card 是否落在
   `a2dp-sink` + `api.bluez5.codec == aptx_adaptive`、以及**默认 sink 是否就是
   MOMENTUM 5**；`--fix` 会断开重连耳机、重选 profile、把耳机设为默认 sink。
   退出码非 0 = 不许测试。
   存在的理由：蓝牙 card 在 A2DP profile 不可选时会**静默退到
   `headset-head-unit`（CVSD）**，此时主机侧所有属性看起来都正常，测试结果无意义
   （本轮实测遇到过两次）。
2. `stream_check.py <btmon capture>` —— 从**本机 HCI** 上量真实空口形态：L2CAP 长度、
   包间隔、吞吐、OTA 头（ttp/period/ptype/channel/version）、帧头，并与 48 kHz R2 的
   期望值（676 B / 25 ms / 27 kB/s / ptype `0x00` / version `0xae`）比对，超差即 exit 1。

两个抓包的标定结果（文件在 `/tmp`，可复现）：

| 抓包 | 结果 |
|---|---|
| `/tmp/verify_fixed.hci`（9/10 正常流） | **PASS** 512 包、676 B、25.00 ms、27.11 kB/s、ptype `0x00`、帧头 `83 00 d0 a1` |
| `/tmp/our-link.hci`（今晚被 `LOSSLESS=auto` 污染） | **FAIL** 6104 包、676 B、49.98 ms、13.53 kB/s、ptype `0xa0` |

即 §21.7 那个"主机侧看不出来、只有量速率才发现"的异常，现在由工具自动拦住。

### 21.9 第十一轮测试矩阵（全部经门禁 + 形态校验，2026-09-11 晚）

> **更正（§21.17）**：本节表格里的 `ptype` 读数受 `stream_check.py` 的解析 bug 影响，
> 真实的普通 R2 形态是 `ptype 1`（656 B 帧）、`period 0x64`（25.00 ms）；以 §21.17 为准。

用户确认：**手机今晚一直播的是 48 kHz / 24-bit**。因此 lossless/R2.2 那条路（编码器只在
44.1 kHz + 16 bit 才进入）**不是手机会用的形态，可以直接排除**。

| # | 配置 | 实测空口形态（`stream_check.py`） | 结果 |
|---|---|---|---|
| T1 | 基线 `0xae` | 676 B / 25.00 ms / 216 kbps / ptype `0x00` / ver `0xae` / 帧头 `83 00 d0 a1` **PASS** | 静音 |
| T2 | OTA 版本 `0xaf` | 676 B / 25.00 ms / 216 kbps / ptype `0x00` / ver `0xaf` / 帧头同上 **PASS** | 静音 |
| T3 | R2.2 形态（44.1k/16bit、`0xaf`） | 780 B / **46.5 ms** / ptype `0xa0` / OTA 周期字段写着 92.8 ms / **55 个包（2.7 s）后彻底断流** **FAIL** | 无传输（用户看到 11 B/s） |
| T4 | 基线复验 | 2475 包、676 B / 25.00 ms / **27.04 kB/s** / ptype `0x00` / ver `0xae` **PASS** | 静音 |

外加 §21.7 那次：CIE 与手机**逐字节相同**（features `0x0f000092`）→ 仍然静音。

**三条结论**

1. **T3 证明 R2.2/Lossless 实时管线不完整**：模块确实产出了 780 B 的 R2.2 帧，但发 55 个包
   后停摆（OTA 周期字段与实际节奏也不自洽：92.8 vs 46.5 ms）。这就是"等边带反馈"那个
   已知缺陷在**实时链路**上的表现（§16.2 的静态探针能出包，实机不能）。用户侧的 11 B/s
   与 HCI 侧的 2.7 秒断流互相印证。
2. **48 kHz / 24-bit 下主机侧变量已穷尽**：`0xae`、`0xaf`、CIE 与手机逐字节一致 —— 全部静音，
   而每一次的线形态都经工具验证正确（676 B / 25 ms / 216 kbps / ptype 0 / 帧头 `83 00 d0 a1`）。
   §13.10 的"回放手机原始 PDU 仍静音"也早已排除码流内容。
3. 因此**剩下的差异只剩链路层**：手机走高通 QHS（§21.5），AX210 只能标准 EDR。

**唯一尚未闭合的主机侧缺口**（低概率但可判定）：手机对外通告 R2.2 能力位（`0x0f000092`）
**同时**发普通 R2 码流；我们从未在这个组合下测过 —— 因为插件把"通告 R2.2"与"编码器
lossless 模式"绑在同一个 `lossless_enabled()` 上：要么清位（我们的常态），要么让模块进
R2.2 状态（T3/§21.7 的坏形态）。要干净地测它，需要给插件加一个"仅对外通告 R2.2、
不改变编码器模式"的开关（约 10 行），重建 PipeWire 后再跑一次门禁测试。
若它仍静音，则"耳机 AD 播放依赖 QHS 链路"就是定论。

### 21.10 补齐最后一个缺口的实现与准备（2026-09-11 夜，用户离开期间）

**代码改动**（`baizhu945/pipewire`，本地检出 `/home/baizhu945/work/pipewire`，尚未提交）：

- 新增插件开关 `APTX_ADAPTIVE_ADVERTISE_R2_2`。置 1 时：对外通告的 CIE **保留** R2.2
  能力位（于是 features 变为 `0x0f000092`，与手机逐字节一致），但**不改变编码器模式**
  （`lossless_mode` 仍为 `off`），因此不会触发 §21.7/T3 那个等边带反馈的停摆。
- 实现落在 `get_advertise_r2_2()` + 两处清位条件（协商路径 + `APTX_ADAPTIVE_FEATURES`
  覆盖路径），并在选配日志里加 `advertise-r22=` / `lossless-mode=` 两个字段。
- **关键细节（自审时发现，必须做）**：插件原本把**同一个 CIE** 既发给对端、又发给编码器，
  而源码注释明确写着"把 R2.2 位呈现给编码器会让 R2.2 wrapper 进入等待状态"。所以
  `initialize_helper()` 里对**编码器那份**单独清位：对端仍看到 `0x0f000092`，编码器仍看到
  清掉 0x80 的旧记录。否则这次实验会和 §21.7 一样被"编码器形态变化"污染。

**新增两个工具**（本轮，已交付到 research 目录）：

- `cie_check.py` —— 从 btmon 抓包里读出 AVDTP 双方的能力/配置元素（用 btsnoop flags
  的方向位区分「我们发的」和「耳机发的」），可断言我们的 features 是否等于手机值
  `0x0f000092`。**这让"通告是否与手机一致"在没有人耳的情况下也能判定。**
- `run_ad_test.sh` —— 门禁 + 播放 + 两项校验串成一条命令，最后提示"现在听有没有声音"。

**顺带发现**：插件源码里早就写着「Lossless 在 48 kHz 下耳机约 1 秒后停止读取，
链路卡在 **~11 B/s**」（`a2dp-codec-aptx-adaptive.c` 关于 44.1 kHz 的注释）。
用户今晚在 T3 观察到的 11 B/s 与这条既有记录完全一致 —— T3 的停摆属于已知形态，
不是新问题。

**构建状态**：`/etc/nixos/pipewire-aptx-adaptive-module.nix` 的 `src` 已**临时**指向本地
检出（文件内注明，测试判定后要改回 pinned rev 或指向新的 fork rev）；
`nixos-rebuild build` 已预构建（**只 build 未 switch**，所以测试前不断连）。
用户要求：不要让蓝牙断开太久，否则耳机会自动关机 —— 因此把 switch 留到用户回来、
可以同时听声音的那一刻，断连窗口只有几秒。

**用户回来后的测试序列**（每一步都有工具兜底）：

1. `nixos-rebuild switch`（store 已就绪，激活很快）；
2. `./run_ad_test.sh`（默认带 `--fix`）→ 先跑「新插件 + 开关关闭」作为回归对照，
   确认线形态仍是 676 B / 25 ms / 27 kB/s / ptype 0 / `0xae`；
3. 写 drop-in `zzz-aptx-advertise-r22.conf`（`APTX_ADAPTIVE_ADVERTISE_R2_2=1`）+
   重启两个服务（几秒），再跑 `./run_ad_test.sh`，并额外跑
   `python3 cie_check.py <capture> --expect-features 0x0f000092` 确认通告与手机一致；
4. **问用户：耳机里有没有声音？**
   - 有 → 根因就是能力位，把开关做成正式特性、提交并更新 pin；
   - 无 → 「耳机 AD 播放依赖 QHS」定论，方向转向高通控制器（如 M.2 QCNCM865，
     仍属本机内置、不是外接 USB），并在 HANDOFF 里写明结论。
5. 无论结果，把 `/etc/nixos/...nix` 的 `src` 从本地路径改回 pinned rev（或新 rev），
   并把 drop-in 删掉，恢复 `LOSSLESS=off` / 通告关闭的基线。

### 21.11 用户离开期间：找到并修正了采样率错误，实验已武装完毕（2026-09-11 深夜）

**重大修正：我们今晚一直跑在错误的采样率上。**

插件的头文件给出了权威位定义：

```
APTX_ADAPTIVE_SAMPLING_FREQ_44100 = 0x40
APTX_ADAPTIVE_SAMPLING_FREQ_48000 = 0x10
APTX_ADAPTIVE_SAMPLING_FREQ_96000 = 0x20
```

而手机 btsnoop 里那份 SET_CONFIG（§13）的采样率字节是 **`0x40` = 44.1 kHz**。T1/T2/T4 全部
跑在 48 kHz（`0x10`）上 —— 用户看到的"48 kHz"是开发者选项里的**偏好设置**，实际码流跟着
内容走（音乐基本都是 44.1 kHz）。**所以"普通 R2 @ 44.1 kHz"这个最接近手机的形态此前从未上过空口。**

| # | 配置 | CIE（采样率/features） | 实测码流 | 结论 |
|---|---|---|---|---|
| T5 | 44.1 kHz + 普通 R2 + 通告关 | `40 02` / `0x0f000012` | 676 B / 24.99 ms / 217 kbps / ptype 0 / `0xae` / 帧头 `83 00 c0 a1` | 形态正确 |
| T6 | 44.1 kHz + 普通 R2 + 通告开 | `40 02` / **`0x0f000092`** | 同上（**编码器仍走普通 R2**） | 与手机逐字节一致 |

T6 的 CIE：`d7 00 00 00 ad 00 40 02 50 64 64 64 ff ff 00 01 92 00 00 0f` —— 与手机
btsnoop 里那 26 个字节**完全相同**；`cie_check.py --expect-features 0x0f000092` **PASS**。

**同时验证了 §21.10 的 CIE 分离改动是对的**：通告位打开后编码器没有进入 R2.2 状态，
码流仍是 676 B / 25 ms / ptype 0 —— 若没有那份"给编码器的另一份 CIE"，这里必然重现
§21.7 的半速/停摆形态。

**顺带修正**：`run_ad_test.sh` / `cie_check.py` / `stream_check.py --report-only` 三个工具
在本轮全部实际使用过；`stream_check.py` 现在把"实测包间隔 vs OTA 周期字段"作为**始终执行**
的自洽检查。

**当前状态（等用户回来听声音）**：drop-in
- `zzz-aptx-force44100.conf`（`APTX_ADAPTIVE_FORCE_RATE=44100`）
- `zzz-aptx-advertise-r22.conf`（`APTX_ADAPTIVE_ADVERTISE_R2_2=1`）

两个都已生效，耳机保持连接（有看门狗防自动关机）。**这就是与手机差异最小的一份配置：
只差链路层（QHS）。** 用户回来后执行：

```bash
cd ~/work/openaptx/research/aptx-adaptive-qemu
./run_ad_test.sh /tmp/tone44k24.wav        # 会自动门禁 + 播放 + 两项校验
```

- **有声音** → 根因是"通告/采样率"，把开关做成正式特性、pin 到新 rev；
- **仍静音** → 「耳机 AD 播放依赖 QHS 链路」定论（此时主机侧唯一剩余差异就是它），
  方向转向高通控制器（M.2 QCNCM865 仍属本机内置）。
- 听过之后记得回退：删掉那两个 drop-in（或至少 `zzz-aptx-force44100.conf`，
  它会把所有内容重采样到 44.1 kHz），并把 `/etc/nixos/...nix` 的 `src` 从本地路径
  改回 pinned rev。

### 21.12 定论：主机侧变量已穷尽，差异只在链路层（2026-09-12 凌晨）

用户听了 T6 那份"与手机逐字节同构"的配置：**没有任何声音**，实测速率 **26.73 kB/s**
（与我在 HCI 上量到的 27.07 kB/s 一致 ⇒ 码流确实以正确形态、正确速率发出去了）。

**至此主机侧所有可控变量都已测过，全部静音：**

| 变量 | 测过的取值 |
|---|---|
| 采样率 | 48 kHz（`0x10`）与 **44.1 kHz（`0x40`，手机同款）** |
| OTA 版本字节 | `0xae`（R2）与 `0xaf`（R2.2） |
| OTA 包类型 | `0x00`（普通 R2 形态） |
| AVDTP CIE | **与手机 SET_CONFIG 逐字节相同**（44.1 kHz / stereo / features `0x0f000092`） |
| 码流内容 | 回放手机原始 PDU（§13.10） |
| TTP 时基 | 墙钟 / 音频钟 / 冻结（§13） |
| 包间隔与吞吐 | 25 ms / 217 kbps，已被 `stream_check.py` 核对 |
| R2.2 / Lossless 形态 | 44.1k+16bit 下实时管线停摆（§21.7、T3），不可用 |

**唯一剩下的差异是链路层**：手机走高通 QHS（专有 6 Mbps 物理层，§21.5），AX210 只能标准 EDR。

**结论：在 Intel AX210 上，主机侧软件再正确也不会让这副耳机出声。** 三个数据点一致 ——
能出声的源（HONOR 90 GT、FiiO BT11/QCC5181）都是高通 + QHS；唯一不出声的源（AX210）没有 QHS。

**下一步（待用户决定）**

1. **换高通控制器**（M.2 内置，如 QCNCM865 / FastConnect 7800，仍不算外接 USB）。
   预期：Qualcomm↔Qualcomm 链路会协商出 QHS，而我们的 AD 码流作为 A2DP 载荷走这条链路，
   耳机侧的解码门槛应当被满足 —— 这是唯一还有希望"原生"的路径。
2. **接受外接方案**：FiiO BT11 已验证可用，但属外接 USB 硬件，不符合项目目标。
3. 暂停 AD，转去完善其他可出声的编码器（aptX HD 在本机链路已验证可用）。

**本轮收尾状态（已核验）**

- 实验 drop-in 全部删除，基线恢复：`LOSSLESS=off`、通告关闭、无 `FORCE_RATE`；
  复验 `PASS`（676 B / 25.00 ms / 27.05 kB/s / ptype `0x00` / `0xae` / 帧头 `83 00 d0 a1`）。
- `/etc/nixos/pipewire-aptx-adaptive-module.nix` 的 `src` 已从本地路径改为
  **pinned rev `8976dcf`**（含新开关，默认关闭）；运行中的插件来自该 pin
  （`1zvdsfvv0n47k3lq8b2i7dkpxinwaan1-pipewire-aptx-adaptive-…`）✓。
- 两个仓库已推送：pipewire fork `8976dcf`、openaptx `808c1e3`（CI 六项全绿）。

**留给后续复测的最小命令**（若换控制器后要重跑）：

```bash
cd ~/work/openaptx/research/aptx-adaptive-qemu
./preflight.sh --fix && ./run_ad_test.sh /tmp/tone48k24.wav
python3 cie_check.py /tmp/ad-test.hci --expect-features 0x0f000012   # 通告关闭时的基线
```

### 21.13 **更正**：QHS 不是门槛 —— BT11 走标准 BR/EDR（2026-09-12 凌晨）

§21.12 的结论"耳机 AD 播放依赖 QHS 链路"**被实验推翻**，特此更正。

**实验**（用户拔掉 BT11 40 秒再插回，survey 连续抓 300 秒，按时间戳对齐）：

```
08064f: 3s(-40)  [拔掉期间完全消失]  65s(-48) 66s(-66) 97s(-63) 102s(-63)
```

拔插前的那次 survey 里 `08064f` 有 **103 次命中、RSSI 最高 −30 dBm**；拔掉即消失、
插回即回归 ⇒ **`08064f` 就是 BT11↔耳机 这条链路**。而 `6add90`（67 次、−5 dBm）在拔掉
之后整段抓包里 0 次命中，属于同一条链路在不同时期的另一个观察值或另一台设备。

**关键推论**：BT11 是已知**能出声**的源（QCC5181，本次推的是 aptX **Lossless**），
它的链路在标准 BR/EDR 上**清晰可见** ⇒ **耳机完全可以在标准 EDR 链路上出声** ⇒
"耳机要求高通 QHS"不成立。QHS 只是高通对高通之间的一种**可选优化**（手机的
FastConnect 支持、BT11 的 QCC5181 大概不支持，所以一个用、一个不用，两者都能出声）。

**所以"我们静音"的原因必须在别处。** 修正后的对照：

| | 链路 | 码流形态 | 结果 |
|---|---|---|---|
| 手机（骁龙） | 疑似 QHS（不可见） | 未直接观测（§13 只有 AVDTP 层） | 出声 |
| FiiO BT11（QCC5181） | **标准 EDR（可见）** | **Lossless / R3 形态**（0xad、ptype 5、760 B 帧、44.1k/16bit） | 出声 |
| 我们 | 标准 EDR（可见） | **普通 R2 形态**（0xae、ptype 0、656 B 帧） | 静音 |

两个能出声的源用的都是 **"Snapdragon Sound" 形态（packet type 5 / 760 B 帧）**，
而我们**从未在空口上持续发出过 ptype 5 的形态**（唯一一次尝试在 §21.7/T3 停摆）。
**新的首要假设：耳机只接受 R2.2/R3 形态的帧，不接受普通 R2（ptype 0）形态。**
这条与 §6 里"768-byte 0xaf/channel 0xa0 静音"那条旧记录冲突，但那次的流是否连续
从未被核实（很可能同样是停摆），所以需要用**不 stalls 的 R2.2 形态**重测。

**下一步优先级（修正后）**

1. **修掉 R2.2/Lossless 实时管线的停摆** —— 这从一个"可选功能"变成了**关键路径**：
   只有能持续发出 ptype 5 形态，才能验证"耳机只吃 Snapdragon Sound 形态"这个假设。
2. 若 BT11 的 App/按键能把编解码器切到**普通 aptX Adaptive**，让它发一次普通 AD：
   若耳机照常出声 ⇒ 形态假设被否；若 BT11 切不了 ⇒ 该实验不可用。
3. 我们自己的 ptype 5 流一旦连续，立刻用 `stream_check.py --expect-ptype 5
   --expect-version 0xaf` 门禁 + 用户听音。

**方法论教训（已记入 §21.2 的坑之外）**：跟随模式命中率极低（我们自己的链路 0.45 包/秒、
BT11 这条 0.08 包/秒），**不能用它做存在性判定**；survey 的高命中数 + 由用户动作触发的
消失/回归差分才是可靠判据。

### 21.14 钉信道计数法的标定：**Ubertooth 测不了吞吐**（2026-09-12）

用户的质疑很关键：**BT11 的 "Lossless" 会不会是假的**？如果它其实只发低码率，
"两个能出声的源都用 ptype 5 形态"这个推论就站不住。为此我尝试用"钉住一个信道数包"
来测吞吐，并**用已知速率的链路做了标定**。

标定方法：让本机 AD 推流（HCI 精确测得 **40 包/秒**：676 B / 25.00 ms），
survey 取它的 AFH 信道图（23 次命中落在 16 个信道上），再钉在最忙的三个信道上计数：

| 钉住信道 | 数到的 `c50142` | 真实速率 | 捕获率 |
|---|---|---|---|
| 47 | 0.62 包/秒 | 40 包/秒 | 1.6% |
| 60 | 0.20 包/秒 | 40 包/秒 | 0.5% |
| 64 | 0.29 包/秒 | 40 包/秒 | 0.7% |

**结论：这个方法只捕到真实包的 1–2%，且强烈依赖 RSSI（我们这条链路饱和 0 dBm，
BT11 那条只有 −30…−72 dBm，探测效率更低）。它无法用来测量任何链路的吞吐。**

**因此 §21.13 里"BT11 链路只有 ~5 包/秒、肯定没在传音频"这句话作废**（那是未标定的
外推）。同理，帧长判据也不可用：Ubertooth 对 EDR 给出不了包头/长度
（我们自己的对照抓包同样是 `packet_header=0x00000000`、`frame.len` 是原始缓冲大小）。

**"BT11 的 Lossless 是真是假"目前无法用 Ubertooth 判定。** 可行的判据只剩：

1. **耳机自己报的编解码器**（Sennheiser Smart Control App 经 BLE 读取）—— 最干净，
   手机侧的 App 能直接显示耳机当前在解码什么；
2. FiiO 上位机/App 显示的是**协商结果**，未必等于实际传输，参考价值较低；
3. 若能把 BT11 切到普通 aptX Adaptive，则"是否仍出声"本身就是一个判据。

**同时被证伪的一条支线**：我曾猜"两条能出声的链路其实都是 LE Audio"。用旧日志直接排除了
（手机侧）：三份 btsnoop 里 **HCI ISO 包数 = 0**、LE Meta 只有扩展广播报告（0x0d）、
没有任何 LE Connection Complete / CIS 事件，而高通 offload 命令齐全
（`0xFC0A` ×3、`0xFD57` ×221/44、`0xFD59` ×9）。**手机确实是在走 A2DP/BR-EDR（offload），
不是 LE Audio。** 所以"手机 AD 能出声、空口却不可见"这个矛盾依然成立，
链路层（QHS）假设对**手机**仍然是最合理的解释；BT11 那条可见链路是否承载它的音频，
目前仍未判定。

### 21.15 定论：普通 AD 走专有链路，Lossless 走标准 EDR（2026-09-12 凌晨）

用户提出关键猜想：**"你抓到的那条可见链路也许只是看门狗/保活，音频在另一条通路上"** ——
并且提供了两个决定性条件：BT11 切到**普通 aptX Adaptive** 后**有声音**，以及可以再做拔插对照。
下面是闭合的证据链。

**关键推理**：piconet 的 LAP **只能**是主设备的地址 —— 要么是 BT11 的 `08064f`，
要么是耳机当主设备时的 `b7163b`。两者都不可能出现第三种身份。

| 观测 | 数值 |
|---|---|
| Lossless 模式，第一次拔插对照 | `08064f` 存在（103 次/分钟）→ 拔掉 40 s 完全消失 → 插回即回归 |
| AD 模式，300 s survey（约 5 个检测周期） | `08064f` **0 次**、`b7163b` **0 次**，而音乐照常在响 |
| AD 模式第二次拔插对照（300 s） | 同上，`08064f`/`b7163b` 均 0 次 |

**⇒ 普通 aptX Adaptive 模式下，BT11↔耳机 这条 piconet 在标准 BR/EDR 上完全不可见**
（连 ACL 信令都看不到 ⇒ 整条 piconet 都在专有 PHY 上，而不是"媒体走子链路、控制走标准链路"）。
**⇒ 而 aptX Lossless 模式下它是一条可见的标准 EDR 链路，且能出声。**

汇总（全部为本机实测）：

| 源 | 模式 | 链路 | 出声 |
|---|---|---|---|
| FiiO BT11（QCC5181） | aptX Lossless | **标准 EDR（可见，`08064f`）** | 是 |
| FiiO BT11 | 普通 aptX Adaptive | **不可见（专有 PHY）** | 是 |
| HONOR 90 GT | 普通 aptX Adaptive | 不可见（§21.4） | 是 |
| 本机 AX210 | 普通 aptX Adaptive | 标准 EDR（可见） | **否** |
| 本机 AX210 | aptX HD | 标准 EDR（可见） | 是 |

**结论与方向**

1. **普通 aptX Adaptive（R2）在这副耳机上似乎只在专有链路上被解码** —— 两个能出声的
   AD 源都是"不可见链路"，唯一"可见链路的 AD"就是我们，而它静音。
2. **但 aptX Lossless/R3 形态在标准 EDR 上是被解码的**（BT11 就是证明）。
   这对本项目是**好消息**：它说明耳机并不要求高通控制器才能出声，
   而要求的是**它认识的码流形态**。
3. 因此**关键路径从"普通 AD"改为"让 R2.2/R3 形态的实时管线不再停摆"**（§21.7/T3）。
   只要那条路能连续输出 ptype 5 的记录，就有希望在 Intel 控制器上出声。
4. 用户原先"先不要碰 lossless"的约束与硬件事实冲突：**在这副耳机 + AX210 的组合下，
   能走通的形态是 R3/R2.2（`0xad`/`0xaf`），普通 R2（`0xae`）看起来无解。**

**方法论**：这次能定论靠的是"用户动作锚定的拔插对照 + LAP 只能等于主设备地址"这两点，
而不是速率 —— 速率法已被 §21.14 证明不可用。survey 的 ~60 s 检测周期会让**低流量**
的 LAP 出现假空档（`6a8fcc`、`ee02f8`、`2ab332` 在本轮都有 25–78 s 的假空档），
但一个 103 次/分钟的 LAP 在整个 300 秒里归零不可能是相位假象。

### 21.16 尝试修 R2.2/R3：找到三个真问题，但最后一道墙没能推倒（2026-09-12）

> **更正（§21.17）**：本节中 `period 1424`、`ptype 0xa0`、"周期字段写着 92.8 ms"
> 等读数都是工具解析错位造成的；真实读数是 `ptype 5`、`channel 0xa0`、
> `period 0x90`（36.00 ms）。结论方向不变，但依据更硬（见 §21.17）。

用户批准的路线：把 R2.2/R3 实时管线修到能连续出流，因为**两个能出声的源都用这一族形态**
（BT11 的 Lossless 就是 R3，手机的 offload 配置里也带着 R2.2 能力位）。

**发现一：不需要改代码的开关（已用上）**

| 开关 | 作用 |
|---|---|
| `APTX_ADAPTIVE_CONFIG_STREAM_OVERRIDE_HEX` | 直接覆盖发给编码器的 11 字节 R2 stream（`[0]`=ext_ver、`[1..4]`=features、`[5..8]`=setup_pref、`[9..10]`=eoc） |
| `APTX_ADAPTIVE_CODEC_FRAMES` | PCM 块尺寸 |
| `APTX_ADAPTIVE_ABR` | 是否发送质量等级 |
| `APTX_OTA_VERSION` / `APTX_FORCE_BITS` | 版本字节 / 源字长 |

**发现二：`cap_ext_ver_num = 0`（文档 §4.6 记载的"16 ms 周期"）会让 helper 当场死亡**

实测：OTA `period` 字段确实从 356 变成 320、帧头从 `83 00 d0 a1` 变成 `81 00 2d a1`，
**但流只发了 24 个包（0.6 s）就断**，日志刷满 `error 断开的管道` —— 模块拒绝该配置。
⇒ §4.6 记的"唯一替代周期"是**致命配置**，不是"能用但耳机不认"。

**发现三（正面）：`ABR=0` 让 R2.2 管线连续运行**

`LOSSLESS=force + ALLOW_UNSTABLE=1 + QHS=1 + FORCE_BITS=16 + FORCE_RATE=44100 +
OTA_VERSION=0xaf + ABR=0` → **1237 个包 / 61.8 s 连续不断**（用户实测 15.40 kB/s，
HCI 实测 15.62 kB/s），形态：**780 B / ptype `0xa0` / version `0xaf`** —— 与能出声的源同族 ✓。
**但用户听音：仍然没有声音。** 且节奏是 **46.48 ms**（≈125 kbps），比能出声的源（~10 ms）慢 4.6 倍。

**发现四：帧长推不动**

模块每包消耗 **2204 样本**（44.1 kHz 域 = 50.0 ms；48 kHz 域 = 45.9 ms，与实测 46.48 ms 吻合），
以下全部**无效**：encoder profile（1/2/3/4/5/6/7/0x1000/0 共 9 个）、码率等级（0–8）、
MTU（700–3000，仅 <768 时退回 664 B 普通形态）、PCM 块尺寸。

**发现五：一个真 BUG（已修，但没改变结果）**

`configure_bitrate_map()` 把码率表按 **bit/s** 发出（`279000…420000`），而模块按 **kbit/s**
阈值 `262/275/290/307/327/348/373/402/436` 吸附 ⇒ **五项全部超过最高阈值、五个等级塌成同一档**。
项目文档 §4.3 早已记录并验证过这个故障模式，但**修复从未落进仓库源码**。
→ 已改成 `279/320/352/384/420`（依次吸附到 275/307/348/373/402 五档），重新编译并部署
（sha256 `4c18bc2e65…`）。**但可观测行为完全没变**：R2.2 仍是 46.4 ms/包、ABR=1 时仍在
第 55 个包停摆 ⇒ **lossless 形态不使用这张表**。

**附带修复：构建环境**

交叉工具链的宿主依赖（`libcxx-22.1.8`、`libunwind-22.1.8`、zlib、zstd）曾被 Nix GC 删除，
导致 helper 无法重编译。已用 nix 取回**精确版本**并用 `LD_LIBRARY_PATH` 拼好，构建恢复：
`SRC_COMPAT=…/compat.c LD_LIBRARY_PATH=<libcxx>:<libunwind>:<zlib>:<zstd> bash build-any.sh …`

`helper_probe.py` 也已扩展出 `--mtu/--bits/--lossless/--qhs`，可在**没有耳机**的情况下
离线复现并扫描这些组合。

**结论（诚实）**

- 我们的模块在 R2.2/lossless 形态下**被限制在 ~50 ms 帧**（比能出声的源慢 4.6 倍），
  且 ABR 一旦开启就在第 55 个包停摆；
- 因此**"修好 R2.2/R3 就能出声"这条路，在当前这个 SPF/Hexagon 模块构建上走不通**；
- 主机侧目前仍只有"25 ms 普通 R2（静音）"与"46 ms R2.2（静音）"两种可连续输出的形态。

### 21.17 **重大更正：stream_check 的 OTA 解析错位一字节**（2026-09-12 凌晨）

**起因**：向 PR 汇报前逐条复核结论，用原始字节手工解析抓包里的 OTA 头，与工具输出对不上。

**根因**：`stream_check.py` 用 `struct.unpack('<HHBBBB', ota)` 解析 8 字节 OTA 头，把
`period` 当成 **16 位**。实际布局是 `[TTP:2][period:1][ptype:1][channel:1][pad:2][ver:1]`
（STATUS §4.1 一直是对的）。于是工具把 ptype 字节并进了 period，**后面每个字段都错位一字节**：
它打印的 `ptype` 其实是 `channel_mode`，打印的 `channel` 其实是 `pad`。

**原始字节证据**（本次直接 dump，不经过工具）：

| 抓包 | OTA 8 字节 | 正确读法 | 工具（错位）读法 |
|---|---|---|---|
| `/tmp/final-baseline.hci` | `3c 0f 64 01 00 00 00 ae` | period `0x64`=100 (25.00 ms)、ptype 1、chan 0、ver `0xae` | period 356、ptype `0x00` |
| `/tmp/base3.hci` | `3c 0f 64 01 00 00 00 ae` | 同上 | 同上 |
| `/tmp/t5-44100.hci` | 同上 | 同上（44.1 kHz 普通 R2） | 同上 |
| `/tmp/t3-r22.hci` | `… 70 05 a0 00 00 af` | period `0x70`=112 (28.00 ms)、**ptype 5**、chan `0xa0`、ver `0xaf` | period 1392、ptype `0xa0` |
| `/tmp/r22b.hci` | `… 90 05 a0 00 00 af` | period `0x90`=144 (36.00 ms)、**ptype 5**、chan `0xa0`、ver `0xaf` | period 1424、ptype `0xa0` |
| 手机（参考） | `a8 61 64 01 00 00 00 ae` | period 100 (25 ms)、ptype 1、ver `0xae` | — |

**交叉验证（两条独立证据，不依赖任何单位假设）**：
ptype 1 对应 656 B 帧、ptype 5 对应 760 B 帧（模块自己的载荷表，STATUS §4.2）。
三段普通 R2 抓包的 L2CAP 都是 676 B = 12+8+656 ⇒ **ptype 必须是 1**；
两段 R2.2 抓包都是 780 B = 12+8+760 ⇒ **ptype 必须是 5**。错位读法给出的
`0x00`/`0xa0` 都不是合法索引（`0xa0`=160 越界）。

**因此以下此前写下的结论作废，特此撤回**：

1. ~~"R2.2 形态的 OTA 周期字段自相矛盾：写着 94.93 ms，实测 46.48 ms"~~ —— 那是错位读数。
   真实是 period `0x90` = **36.00 ms** 对实测 46.48 ms。**不一致仍然存在，但数值不同**，
   而且 T3 那次是 28.00 ms 对 46.52 ms ⇒ 模块在 R2.2 形态下声明的周期随配置变化，
   实际节奏却始终 ~46.5 ms（= 每帧 2204 样本）。
2. ~~"helper 有单位换算 bug：`packet[2] = module_memory[0x248] << 2` 应为 ×3.75，
   导致普通流声明 23.73 ms 而实测 25.00 ms"~~ —— **完全作废**。模块该字段是**毫秒**
   （25 ms），×4 恰好得到 0.25 ms 单位的 100 = `0x64`，**与手机逐字节相同**。
   所谓 23.73 ms 是错位读数的产物。普通 R2 流的声明周期与实测**完全一致**（25.00 ms）。
3. ~~"我们从未在空口上持续发出过 ptype 5 形态"~~ —— T3 就已经是 ptype 5（只是 55 包后停摆），
   ABR=0 的 `/tmp/r22b.hci` 更是**连续 938 包 / 46.8 s 的 ptype 5 / 760 B / chan `0xa0` / ver `0xaf`**。

**结论的变化（这才是关键）**：既然 **Snapdragon Sound 形态确实连续上过空口**（`0xaf`、
ptype 5、760 B、chan `0xa0`、46.5 ms、15.63 kB/s）**而耳机仍然静音**，
那么"只差码流形态"这一读法被削弱，§21.15 那张五源对照表里剩下的差异是：

1. **版本字节**：能出声的 Lossless 是 **R3（`0xad`）**，本模块出的是 **R2.2（`0xaf`）**；
2. **节奏**：能出声的源 ~10 ms/包，本模块被钉在 2204 样本/帧（≈46 ms）；
3. **链路**：所有能出声的普通 AD 源都在标准接收机看不见的链路上，唯一"可见链路的 AD"
   （本机）静音。

**工具修复**：解析改为 `'<HBBBHB'`；`period` 按 0.25 ms 单位解释（`OTA_PERIOD_UNITS_PER_MS`）；
默认 `--expect-ptype` 从 `0x00` 改为 **`0x01`**；并新增一条**自检**：帧长必须等于
ptype 在载荷表里对应的长度，否则直接 FAIL —— 正是这条能防住本次这类错位。
修复后复验：普通 R2（48 kHz、44.1 kHz）**声明 25.00 ms / 实测 25.00 ms，完全自洽**；
R2.2 报出真实的 36.00 ms 声明与"帧长 760 B ↔ ptype 5"自洽，仅节奏不一致一条 NOTE。

**教训**：解析二进制头时，"字段错位一字节"往往仍能产出一组看起来合理的数字
（356、1424 都不是明显的非法值），所以**必须有一条独立于解析器的物理约束做交叉验证**
（这里是"载荷长度必须等于 ptype 表项"）。这与 §21.14 的"速率法不可用、要靠拔插对照"
是同一条方法论。

### 21.18 复核："BT11 在普通 Adaptive 下不可见"—— 结论成立但**理由与范围都要改**（2026-09-12 下午）

**任务**：用户重新插上 Ubertooth 与 BT11，切到普通 aptX Adaptive，要求复核 §21.15 的结论。

**仪器与前提**：Ubertooth One（固件 2020-12-R1 / API 1.07）；工具不在 PATH，实际路径
`/nix/store/n7nknq4bf481l2c0zrpl5dn3dq0925fl-ubertooth-2020-12-R1/bin`。
宿主侧用 PipeWire 确认 BT11 的 UAC 声卡 `node.state = running`（音频确实在流向 dongle），
`lsusb` 计数 `0a12:` 设备来确认拔插状态（拔掉时 = 0）。

**第一次尝试（失败，记录教训）**：我一边启动 245 s survey 一边发问，用户**立即**回复"已插回"
⇒ 拔插动作很可能落在抓包窗口之外。结果是 `68167a` 全程存在（339 次，最大空档仅 6 秒），
它**不可能**是被拔插的设备。**教训：动作锚定必须由"我说开始、用户做完再确认"的顺序保证，
不能靠并发提问。**

**严格分步的四段对照**（每次 130 s，动作后由用户确认再抓）：

| BT11 状态 | `6a8fcc` | `08064f` | `b7163b` | 噪声底样本 |
|---|---|---|---|---|
| 拔掉 ① | **0** | 0 | 0 | `1c8cb9` 119.5/min、`73bb13` 21.2/min |
| 拔掉 ② | **0** | 0 | 0 | `1c8cb9` 120.4/min、`73bb13` **0** |
| 插回+播放 ① | **74.3/min（0 dBm）** | 0 | 0 | `73bb13` 13.8/min |
| 插回+播放 ② | **59.1/min（0 dBm）** | 0 | 0 | `73bb13` **0** |

**噪声底**：同一状态（拔掉）的两次抓包里，`73bb13` 从 21.2/min 掉到 0，而 `1c8cb9` 稳定
120/min ⇒ **单次对照不足以定论**；`6a8fcc` 是"两次为 0 / 两次 59–74"的唯一 LAP。

**电源循环抓包（197 s，一次抓包里同时看到消失与出现）**：

```
t(s)      0 ...... 47 48 49 ...................... 79 ....... 197
6a8fcc    0          0  0  9 93 92 65 | 259 hits / 31 s = 8.4/s, 45 信道, 最高 0 dBm
                             然后 0 次持续 118 s（音乐仍在播）
b7163b    0           4 次（0 dBm）  0 ...
```

`b7163b` 是**耳机自己的地址**，它只在 47–48 s 出现 2 秒，紧接着 `6a8fcc` 爆发 ——
这正是 **paging 阶段**（接入码属于被呼叫方），连接建立后 piconet 的接入码属于主设备
⇒ **`6a8fcc` 就是 FiiO BT11 的地址（它做 master）**。

**稳态复核**：`present-3`（180 s，用户确认耳机正在出声）= `6a8fcc` **0 次**；
camp `ch 73` 190 s 也 0 次（注：camp 信道是拿 burst 之前的数据选的，burst 之后 AFH 图已变，
所以这条**不构成**对 `6a8fcc` 的独立证据，只记录方法局限）。

**结论（对 §21.15 的修正）**：

1. **稳态不可见：成立** —— 普通 AD 播放中（用户确认出声），该 piconet 在标准 BR/EDR 上
   0 次 / 118 s、180 s、190 s。旧结论的方向没错。
2. **理由错了**：§21.15 的零结果是在 `08064f`/`b7163b` 上测的，而这两者**今天一次都没出现**；
   BT11 的 AD piconet 是 `6a8fcc`。旧结论"碰巧对了"，但证据指向了错误的 LAP。
3. **范围过强**：§21.15 说"整条 piconet 都在专有 PHY 上，连 ACL 信令都看不到"——**不成立**。
   (重)连接建立阶段清晰可见（259 次 / 31 s、8.4 次/秒、45 个信道、最高 0 dBm），
   之后才转为不可见。
4. 手机在 §21.4 表现出的正是**同一模式**（只在重连瞬间可见，稳态 0）⇒ 这是**普通 AD 的
   行为特征**（连接建立走标准 BR/EDR，音频稳态走专有 PHY），不是 BT11 特有、也不是耳机特有。
5. 顺带更正：§21.15 把 `6a8fcc` 列为"survey 相位假象的例子"——它不是假象，
   它就是 BT11 的 AD piconet。

**遗留未解**：旧记录里 Lossless 模式的锚点是 `08064f`（103 次/分钟、拔插锚定），
今天 AD 模式的锚点是 `6a8fcc`。同一个 dongle 的 BD_ADDR 只应有一个，
所以两者必有一处是误判 —— 需要用同样的电源循环方法在 **Lossless 模式**下复核一次。

**方法论沉淀（三条，都可复用）**：
1. 锚定动作必须**由协调者发令、操作者完成后再确认**，并发提问会让动作落在窗口之外；
2. 任何"某 LAP 不存在"的结论，先要**同状态重复两次**估计噪声底（本次 `73bb13` 21→0）；
3. 用**被叫方地址出现在 paging 阶段**这一特征可以反推 master 身份，从而把 LAP 与设备对上。

### 21.19 Lossless 模式复核：与 AD 在空口上**完全无法区分**（2026-09-12 下午）

用户把 BT11 切到 **Lossless**，要求用 §21.18 的同一套方案复核 `08064f` / `6a8fcc` 的矛盾。

**宿主侧前提**：BT11 的 UAC 声卡 `node.state = running`；`/proc/asound/card1/pcm0p/sub0/hw_params`
显示实际流是 **44100 Hz / S24_3LE（24 bit）**——注意是 24 bit，而 Lossless 是否需要 16 bit 输入
尚未确认（见"遗留"）。

**结果一（稳态 129 s，音乐在播）**：`08064f` **0 次**、`6a8fcc` **0 次**、`b7163b` **0 次**。

**结果二（电源循环 199 s，一次抓包内）**：

```
t(s)      0 ....... 49 50 51 .................... 81 ....... 199
6a8fcc    0          0  0  0  78 76 80 12 | 246 次/31 s = 7.7/s，39 信道，0 dBm
                              之后 118 s 全 0（音乐仍在播）
b7163b    0          14 次（0 dBm，11 信道）  0 ...
08064f    全程 0 次
```

**与 AD 那次逐项对照**（两次都是同一套动作、同一位操作者）：

| 模式 | master LAP | 爆发 | 稳态 | `08064f` | paging（`b7163b`） |
|---|---|---|---|---|---|
| 普通 AD | `6a8fcc` | 259 次/31 s（8.4/s、45 信道） | 0 次 | 全程 0 | 47–48 s，4 次 |
| Lossless | `6a8fcc` | 246 次/31 s（7.7/s、39 信道） | 0 次 | 全程 0 | 49–50 s，14 次 |

**⇒ 在 Ubertooth 能观测的范围内，两种模式没有任何差别**：同一个 `6a8fcc`、
同样 ~31 秒的"建立即消失"、同样在稳态归零、`08064f` 一次都不出现。

**推论与保留**：

1. 旧记录里 Lossless 的锚点 `08064f`（103 次/分钟、稳态可见）**无法复现**，
   很可能当年是误判（例如把某个邻居设备当成了 BT11）——但见第 2 条。
2. **空口无法判定 dongle 当前到底跑的是哪种编码**。因此"今天的 Lossless 跑其实就是 AD"
   这一可能无法排除，两种解释都成立：
   (a) 旧锚点错了，`6a8fcc` 才是 BT11，Lossless 同样是"建立可见、稳态不可见"；
   (b) dongle 没真正进入 Lossless（例如本机送的是 **24 bit**，而 Lossless 可能要 16 bit 输入），
       今天测的仍是 AD。
3. **对结论的影响（重要）**：旧推理链里"BT11 的 Lossless 在标准 EDR 上可听 ⇒ 耳机不要求高通控制器"
   这一环，**建立在一个无法复现的观测上**。若第 2 条取 (a)，则今天测到的三个可听源
   （手机 AD、BT11 AD、BT11 Lossless）**稳态链路对标准接收机全部不可见** ⇒
   "需要专有 PHY 才能解码"这一读法反而被**加强**，"只差码流形态"被削弱。

**遗留与下一步（按价值排序）**：

1. **用外部指示确认 dongle 真实编码**：FiiO 的指示灯/上位机，或手机侧 Sennheiser Smart Control
   读耳机当前解码的编码——这是唯一能判定 (a)/(b) 的办法；
2. 若确认已是 Lossless，则把宿主侧改成 **44.1 kHz/16 bit** 再测一次（排除位深导致的回退）；
3. 若两者都确认，则 `08064f` 应正式判定为误判，并从文档里移除其"Lossless 锚点"地位。

**方法论**：这次能快速定位，靠的是 §21.18 建立的三件事——动作顺序受控的锚定、
同状态重复两遍估噪声底、以及 paging 阶段用被叫方地址反推 master 身份。
