# aptX Adaptive on a non-Qualcomm Linux host — 项目交接报告

**最后更新**：2026-09-09 深夜（第二轮）
**当前状态**：AVDTP 配置与 RTP 头已与手机**逐字节一致**；耳机仍静音。
根因进一步收窄到 **码率/帧长**：我们的 48 kHz 流只有 212 kbps，低于 aptX Adaptive
最低的 279 kbps（用户实测正常应为 ~50 kB/s = 420 kbps）。

---

## 0. 一句话总结

我们这轮做到了三件以前做不到的事：

1. **证明编码器输出正确**——用 Qualcomm 官方参考解码器（openaptx PR #9 里的
   `test-decoder.exe`，Wine 运行）解码我们经蓝牙实际发出的码流，FFT 主频正好
   `440.0 Hz`（即播放的测试正弦），48 kHz 立体声。
2. **证明传输层正确**——把手机抓下来的**原始帧**原样通过我们的 PipeWire/BlueZ
   链路回放，HCI 抓包逐帧比对完全一致，RTP 头与可用的 aptX HD 完全同构。
3. **找到并修掉真正的根因**——插件把 aptX Adaptive 的采样率位掩码写错了：
   `44100` 写成 `0x40`（实际是 192 kHz），`96000` 写成 `0xa0`（未定义值）。
   所以“44.1 kHz 会话”实际向耳机声明 192 kHz，却喂 44.1 kHz 帧 → 耳机静音。

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
#define APTX_ADAPTIVE_SAMPLING_FREQ_44100  0x40   // 错：这是 192 kHz
#define APTX_ADAPTIVE_SAMPLING_FREQ_48000  0x10   // 对
#define APTX_ADAPTIVE_SAMPLING_FREQ_96000  0xa0   // 错：未定义
```

后果：`APTX_ADAPTIVE_FORCE_RATE=44100` 时实际协商出 `0x40`（192 kHz），
编码器却按 44.1 kHz 出帧（帧头字节 2 = `0xc0`），耳机按 192 kHz 解码 → 静音。
48 kHz（`0x10`）本来就是对的，但耳机仍静音，说明还有第二层原因（见 §6）。

修复后（`bf15869`）：

```c
#define APTX_ADAPTIVE_SAMPLING_FREQ_44100  0x08
#define APTX_ADAPTIVE_SAMPLING_FREQ_48000  0x10
#define APTX_ADAPTIVE_SAMPLING_FREQ_88200  0x20
#define APTX_ADAPTIVE_SAMPLING_FREQ_96000  0x40
#define APTX_ADAPTIVE_SAMPLING_FREQ_192000 0x40
```

广告能力从 `0xf0` 变为 `0x58`（44.1/48/96），与 MOMENTUM 5 的能力 `0x70`
交集为 `0x50`（48 kHz、96 kHz）。

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

`0x40` 按 Qualcomm 表 = 192 kHz，但手机内容是 48k/96k，**待确认**它到底是
96 kHz 还是 192 kHz（见 §6 下一步）。`0x92` 比我们的多一个 R2.2 位。

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

| # | 协商配置（byte6/7/features） | 帧来源 | 结果 |
|---|---|---|---|
| 1 | `0x40 02 12`（修复前，误当 44.1k） | 我们的 44.1k 帧 | 静音 |
| 2 | `0x10 02 12`（48k） | 我们的 48k 帧 | 静音 |
| 3 | `0x10 02 12`（48k） | **手机原始 48k 帧**（回放） | 静音 |
| 4 | `0x40 02 92`（修复前，误当 44.1k+R2.2） | 手机原始帧 | 静音 |
| 5 | `0x10 02 92`（修复后，48k+R2.2） | 我们的 48k 帧 | 静音 |
| 6 | `0x40 02 12`（修复后，96k） | 我们的 96k 帧 | 静音 |
| — | aptX HD（同一链路、同一 RTP 头） | — | **正常出声** |

补充事实：

- R2 wrapper 的真实帧长实测就是 **1200 样本/帧**（喂 600/672/1200 样本块，
  输出都稳定在 1200 样本一帧），即 48 kHz 下 25 ms。所以帧率不是问题。
- **helper 的 96 kHz 模式有 bug**：配置 `encoder_rate=96000`（selector 0）时，
  输出帧头仍是 `8300d0a1`，用参考解码器解出来是 **48000 Hz**（输入 1000 Hz →
  解出 504 Hz）。手机真正的 96 kHz 段帧头是 `8300b0a1`。这说明
  `capi_rate_selector()` 把 96000 映射到 selector 0 并不产生真正的 96 kHz 码流，
  需要重新确认 selector 与采样率的对应关系（Qualcomm 官方把 88000/192000 都映射
  到 0）。
- 耳机能力 `0x71` 按 Qualcomm 表 = 采样率位 `0x70` = {0x10, 0x20, 0x40}
  = {48k, 88.2k, 192k}（没有 44.1k、没有 96k）。手机 SET_CONFIG 选 `0x40`
  → 很可能是 **192 kHz**。我们的编码器目前只能出 48k/44.1k 的码流。

---

## 7. 下一步（按优先级）

### A. 打通真正的 192 kHz 编码路径 ⭐ 最高优先
证据链指向：手机用 `0x40`（192 kHz）与耳机通信，耳机能力里也只有
48k/88.2k/192k。要做的是：

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

### C. 确认 headphone 能力的真实语义
如果能找到 MOMENTUM 5 的固件或 aptX Adaptive 解码器字符串，确认 `0x71`
的采样率位到底是 {48k,88.2k,192k} 还是 {48k,96k,192k}。

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

**所以最可能的静音根因：我们向耳机声明了 R2.2，却发 R2 帧。**
两种修法：
1. 把等级顶到 6/7，让 wrapper 自己发 0xaf（需要搞清楚 br_level 的钳位：
   日志显示我们送 level 7，模块内部只认 `br_level = 3`）；
2. 先做**最小验证**：把 helper 输出记录的 `packet[7]` 改成 0xaf（或改用
   lossless=AUTO 的 768 B 包再改版本字节）回放，听耳机是否出声。

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
