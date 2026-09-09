# aptX Adaptive 诊断工具（第三轮新增）

这些脚本都假设：
- helper 在 `/home/baizhu945/Documents/aptx-adaptive-runtime/helper/`
- QEMU 在 `/nix/store/gx222l0zm41h4zqrgpb07nc0brwpv3nk-qemu-11.1.0/bin/qemu-hexagon`
- tshark 在 `/nix/store/fy4pvbxfj8nj9f9pfq31i67vdp7n9csd-wireshark-cli-4.6.8/bin/tshark`

## 1. 端到端配置实验
`sudo python3 aptx_test.py <case>` — 写 systemd drop-in、重启 PipeWire、选
aptX Adaptive、回放测试音、抓 btmon、输出 SET_CONFIG 与流统计。
注意：drop-in 的 `UnsetEnvironment` 在 `Environment` 之后生效，脚本每次会先删掉
所有旧的 `zz-aptx*.conf`。

## 2. 抓包统计
`python3 rtp_analyse.py /tmp/cap_*.hci` — RTP 时钟率、TTP 速率、帧长、
间隔直方图、帧头分布。用来和手机参考流比对。

## 3. 手机 btsnoop
`python3 acl.py <btsnoop_hci_*.log>` — 按 PB 标志正确重组 HCI ACL。
注意手机 btsnoop 里 handle 0xedc 的一批记录不是 L2CAP（Qualcomm BTM 日志）。

## 4. 直接驱动 helper（不经 PipeWire）
`python3 helper_probe.py --rate 48000 --blocks 30`
`python3 helper_sweep.py` — 扫 rate/profile/mtu/abr/quality/cie
`python3 raw_probe.py probe|scan` — 通过 raw set_param 探测模块 param ID

这些脚本直接 spawn `qemu-hexagon <helper>`，用 helper 的
`{u32 size, payload}` 协议喂 PCM、读 664 字节 OTA 记录。

## 5. 带模块日志的 helper
`compat-log.c` 是 `compat.c` 去掉 `APTX_HAP_LOG` 门控的版本。构建：
```bash
SRC_COMPAT=compat-log.c \
  LD_LIBRARY_PATH=/nix/store/66lzffbmnizxfp8h1k03wbl8yhbsazgg-libcxx-22.1.8/lib:/nix/store/imladhnprigm0ffyr8ka5ymxbh5r3lxa-libunwind-22.1.8/lib \
  bash /home/baizhu945/work/openaptx/build-helper/build-any.sh <helper.c> /tmp/helper-log
```
然后把 `build-helper/libgcc.so` 复制到 helper 同目录（rpath 是 `$ORIGIN`）。
stderr 上会打印模块的 `HAPv2` 日志；`arg2` 起才是格式串实参（`%f` 占两个字）。

## 6. helper 源码的可调开关（本轮加的，未提交到主源码）
在 `aptx-lossless-helper.c` 上做的小补丁，用于实验：
- `APTX_R2_PROFILE`（已有）— CAPI init 的 profile
- `APTX_RATE_SELECTOR` — 覆盖 `capi_rate_selector()`
- `APTX_CAPI_CHANNEL_MODE` — CAPI init 的 channel_mode
- `APTX_MIN_BUF`/`APTX_MAX_BUF` — min/max_sink_buffer
- `APTX_MAP_LEVELS` — bitrate map 的等级值（**kbps**，逗号分隔）
- `APTX_MAP_PARAM_ID`/`APTX_MAP_FIRST` — map 的 param ID 与发送顺序
- `APTX_IMCL_LEVEL`/`APTX_IMCL_PORT` — 每个音频请求前注入 IMCL 码率等级反馈
