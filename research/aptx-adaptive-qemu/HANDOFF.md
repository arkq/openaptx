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
