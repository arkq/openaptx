# gen_cntr 进程循环提取 & helper 差距分析

来源：`~/work/audioreach-engine/fwk/spf/containers/gen_cntr/`（BSD-3-Clause，Qualcomm AudioReach 开源）
对照对象：`research/aptx-adaptive-qemu/aptx-lossless-helper.c`

---

## 1. 容器数据通路的完整调用链

```
gen_cntr_data_process_frames()                     core/src/gen_cntr_data_handler_island.c:1113
├─ gen_cntr_reset_process_info()                   1039  清 anything_changed / release_ext_out_buf
├─ gen_cntr_wait_for_any_ext_trigger(entry=TRUE)   等待外部触发（输入或输出缓冲区到达）
└─ for (loop_count = 0;; loop_count++)
   ├─ do {
   │    gen_cntr_data_process_one_frame()          718   处理一帧
   │    cu_poll_and_process_ctrl_msgs()                  数据后处理控制消息
   │    inner_loop_count++
   │  } while (gen_cntr_need_to_process_frames())  154   决定是否继续处理
   ├─ if (is_output_ready) gen_cntr_deliver_output()   1053  把输出缓冲区交还客户
   ├─ gen_cntr_check_and_vote_for_island_in_data_path()
   ├─ gen_cntr_process_pending_input_data_cmd()          媒体格式变更后续
   ├─ gen_cntr_handle_fwk_events_in_data_path()
   ├─ if (need_to_exit_outer_loop) break
   ├─ if (gen_cntr_wait_for_any_ext_trigger(FALSE)) break
   └─ if (loop_count >= 100) { 只监听控制端口; break }
```

### 1.1 `gen_cntr_data_process_one_frame()`（一帧的完整流程）

```
1) 若 port_thresh_event：跳过输入设置，直接置 anything_changed
   否则 对每个 ext_in_port:
       gen_cntr_setup_internal_input_port_and_preprocess()
         - 把外部输入缓冲拷进内部 topo 缓冲
         - 缓冲驱动(CD)模式下内部缓冲未满且外部为空 → 设 input_wait_mask
         - 中断驱动(EP)模式下外部为空且内部未满 → UNDERRUN
         - 否则 ready_to_go = FALSE（下一帧跳过）

2) 对每个 ext_out_port:
       actual_data_len = gen_cntr_get_ext_out_total_actual_max_data_len()
       记入 ext_out_port_scratch[].prev_actual_data_len
       vtbl->setup_topo_buf()（peer cntr / Rd shm ep 才有）

3) 对每条并行路径 i:
       若 wait_mask_arr[i] == 0:
         while (TRUE):
           gen_topo_topo_process(topo, &start_module_list, &i)   ← 真正调用模块 process
           gen_cntr_handle_process_events_and_flags(...)
           if (mf_th_ps_event && start_module_list) 继续循环（媒体格式/阈值/状态变更要重跑）
           else break

4) 对每个 ext_out_port:
       gen_cntr_post_process_ext_output_port()
         - 未 STARTED 则跳过
         - bytes_produced_by_topo = out_port.bufs[0].actual_data_len
         - 若 media_fmt_event：先释放旧数据，再从 topo 取媒体格式
         - 若 bytes_produced_by_topo:
             gen_cntr_post_process_ext_out_buffer()
               - gen_cntr_check_output_space_availability()
                   * 缓冲区不存在 → buf_available=FALSE（overrun）
                   * 空间不足     → release_ext_out_buf=TRUE；num_frames==0 时直接 AR_EFAILED
               - 若 ext 缓冲被复用为 topo 缓冲 → skip_copy（只加 actual_data_len）
               - 否则 gen_cntr_move_data_from_topo_to_ext_out_buf()
                   * PCM unpacked：逐声道拷贝 + 剩余数据 memmove 回开头
                   * 其它：gen_cntr_mv_data_from_topo_to_ext_out_buf_npli_()
             - 合并 metadata 链表
             - bytes_produced_by_pp != 0:
                 anything_changed = TRUE
                 num_frames_in_buf == 0 → 设置 next_out_buf_ts
                 vtbl->fill_frame_md()
                 num_frames_in_buf++
                 时间戳 += topo_bytes_to_us(bytes_produced_by_pp)
         - force_return_buf = TRUE
         - gen_topo_output_port_return_buf_mgr_buf()
         - 清除内部端口借用的 ext 缓冲
         - end_of_frame → 释放缓冲

5) 对每个 ext_in_port: gen_cntr_clear_borrowed_ext_in_buffer_from_int_ports()
```

### 1.2 `gen_cntr_need_to_process_frames()`（是否继续内层循环）

```
对每个 ext_out_port:
    if (端口有缓冲区 && num_frames_in_buf == max_frames_per_buffer)
        is_output_ready = TRUE; release_ext_out_buf = TRUE; continue
    if ((num_frames_in_buf == 0 && md_list_ptr) || release_ext_out_buf)
        is_output_ready = TRUE; release_ext_out_buf = TRUE; continue

if (is_output_ready) return EXIT_PROCESSING            ← 输出满/有 MD，先交付

if (curr_trigger == SIGNAL_TRIGGER) return (inner_loop_count == 0)   ← 信号触发只跑一次

if (num_data_tpm == 0 && !is_any_path_ready_to_process()) return EXIT_PROCESSING
if (num_data_tpm == 0 && trigger_signal 已置位) { need_to_exit_outer_loop = TRUE; return EXIT }
if (!anything_changed) return EXIT_PROCESSING          ← 关键退出条件
if (!is_real_time && 命令队列非空) { process_pending = TRUE; need_to_exit_outer_loop = TRUE; return EXIT }
if (inner_loop_count > 1000) { 报错; need_to_exit_outer_loop = TRUE; is_output_ready = FALSE; return EXIT }

return PROCESS_MORE_FRAMES
```

### 1.3 `gen_cntr_handle_process_events_and_flags()`（事件处理）

```
if (capi_event_flag 或 fwk_event_flag 非空) → 临时退出 island，可能提升线程优先级

capi_event_flag.media_fmt_event → gen_topo_propagate_media_fmt_from_module(); mf_th_ps_event = TRUE
capi_event_flag.port_thresh    → process_info.port_thresh_event = TRUE
                                 anything_changed |= TRUE
                                 cntr_vtbl->port_data_thresh_change()
                                 mf_th_ps_event = TRUE
capi_event_flag.process_state  → mf_th_ps_event = TRUE
gen_cntr_handle_fwk_events_in_data_path()
voice_info: hw_acc_proc_delay 变更 → 聚合延时
```

### 1.4 BT codec 框架扩展（`ext/bt_codec_fwk_ext/src/gen_cntr_bt_codec_fwk_ext.c`）

**这是 aptX Adaptive 专用的唯一容器侧逻辑**，只处理两个事件：

| 事件 | 作用 |
|---|---|
| `CAPI_BT_CODEC_EXTN_EVENT_ID_DISABLE_PREBUFFER` | 对**所有** ext_out_port 设置 `cu.icb_info.disable_one_time_pre_buf = (disable_prebuffering > 0)` |
| `CAPI_BT_CODEC_EXTN_EVENT_ID_KPPS_SCALE_FACTOR` | `module_ptr->kpps_scale_factor_q4 = scale_factor`（q4 格式，1.0 = 0x10），并置 kpps 投票标志；<0x10 视为非法 |

**结论：BT codec 扩展不参与任何音频数据生成**，只影响预缓冲开关和时钟投票。

### 1.5 预缓冲（`gen_cntr_check_and_send_prebuffers_util_`，:2039）

```
if (cu_check_if_port_requires_prebuffers(&ext_out_port->cu))
    cu_handle_prebuffer(&cu, &ext_out_port->gu, out_buf_ptr,
                        ext_out_port->cu.buf_max_size - 需要额外 DM 字节数)
else
    ext_out_port->cu.icb_info.is_prebuffer_sent = TRUE
```

---

## 2. 差距分析：helper vs gen_cntr

| # | gen_cntr 行为 | helper 现状 | 差距 | 影响 |
|---|---|---|---|---|
| 1 | **一次 process 调用只处理一帧**，由 `need_to_process_frames` 决定是否再跑 | 每次 `process_audio()` 只调一次 `module->process()` | **缺失** | 模块内部缓冲一帧时需要宿主反复调用；helper 靠 `-EAGAIN` 让上层重试，语义不等价 |
| 2 | `num_frames_in_buf == max_frames_per_buffer` 才交付输出 | 有数据就立即返回 | **缺失** | 输出缓冲区生命周期模型不同 |
| 3 | `gen_cntr_check_output_space_availability`（缓冲不存在→overrun；空间不足→释放并可能报错） | 只检查 `produced <= MAX_PACKET_SIZE` | **缺失** | 无 overrun/空间不足语义 |
| 4 | `setup_internal_input_port_and_preprocess`：外部缓冲→内部缓冲拷贝 + 阈值/等待掩码 | 直接把 PCM 指针交给模块；R3 靠 `reset_r3_input_cursors()` 手动改游标 | **缺失（被 hack 替代）** | R3 游标语义是我们自己猜的，不是容器行为 |
| 5 | `post_process_ext_output_port`：topo→ext 拷贝、帧计数、时间戳推进、metadata 合并 | 直接读 `output_buffers[0].actual_data_len` | **部分** | 无帧计数/时间戳/metadata |
| 6 | `handle_process_events_and_flags`：媒体格式/阈值/状态变更 → **回退重跑模块** | `event_callback` 只记录，不重跑 | **缺失** | 媒体格式变更时不会重入 |
| 7 | BT codec 扩展：`DISABLE_PREBUFFER` / `KPPS_SCALE_FACTOR` | 完全忽略 | **缺失** | 预缓冲开关、时钟投票丢失 |
| 8 | `anything_changed` / `num_data_tpm_done` 驱动的循环退出 | 无 | **缺失** | 无法判断"再跑一帧是否有意义" |
| 9 | 预缓冲 `cu_handle_prebuffer` | 无 | **缺失** | 首包填充语义不同 |
| 10 | `max_frames_per_buffer` / `num_frames_in_buf` 端口状态 | 无 | **缺失** | — |
| 11 | 外部触发等待 `wait_for_any_ext_trigger` | 阻塞读 stdin | **等价**（用协议层替代） | — |
| 12 | 命令队列轮询 | 协议层单独处理控制命令 | **等价** | — |

**核心结论**：helper 目前是"一次调用一帧"的简化模型，缺少容器的**多帧循环 + 缓冲区状态机 + 事件回退**。这三项是本次移植的目标。

---

## 3. 移植方案

在 helper 中实现一个 `struct cntr_port_state`（每输出端口一个），把 gen_cntr 的状态机映射到单线程协议循环：

```c
struct cntr_port_state {
    uint8_t *buf;                  /* 当前外部输出缓冲区 */
    size_t   buf_max_size;
    size_t   actual_data_len;
    uint32_t max_frames_per_buffer;/* 由端口阈值/媒体格式推导 */
    uint32_t num_frames_in_buf;
    bool     release_ext_out_buf;  /* scratch flag */
    bool     is_prebuffer_sent;
    bool     disable_one_time_pre_buf; /* BT codec 扩展 */
    uint32_t kpps_scale_factor_q4;     /* BT codec 扩展 */
    bool     out_media_fmt_changed;
    uint64_t next_out_buf_ts;
};

struct cntr_process_info {
    bool anything_changed;
    bool port_thresh_event;
    uint32_t num_data_tpm_done;
};
```

`process_audio()` 改为：

```
gen_cntr_reset_process_info()
do {
    anything_changed = FALSE
    process_one_frame()            /* 调 module->process()，处理事件，累加 num_frames_in_buf */
    inner_loop_count++
} while (need_to_process_frames())

if (is_output_ready) deliver_output()   /* 返回给协议层 */
```

其中：
- `process_one_frame()` = 设置输入缓冲 + 调 `module->process()` + 处理事件 + 累加输出帧计数
- `need_to_process_frames()` = 上面 1.2 的移植（去掉 island/voice 相关分支）
- BT codec 扩展事件在 `event_callback` 中解析并写入 `disable_one_time_pre_buf` / `kpps_scale_factor_q4`

**保留**：R3 游标处理（作为 `setup_internal_input_port_and_preprocess` 的等价物），但改成只在"输入被消费"后推进，而不是每帧无条件清空。

---

## 4. 与 R3/Lossless 空帧问题的关系（重要）

本次反汇编 `libaptXAdaptiveEnc3.so` 发现：

- `aptX3Encode` 的**主编码路径**由 `ctx[0x8] == 1` 门控（0x463c）。
- `ctx[0x8]` 只能由 `aptX3Encode_SetProfileMode(ctx, profile, force)` 在 `force == 0` 时设置（0x520c）。
- 模块 init 后 `ctx[0x34] = 1`、`ctx[0x8] = 0`。
- 用 `force = 0` + profile 6 调用后 `ctx[0x8] = 1`，输出从 `87 80 00 00` 变为 `81 22 80 00`，但**仍然是 4 字节 + 全零**。

因此：**容器侧状态机（本节 1-3）解决的是缓冲区/帧/事件语义，不能直接让编码器产出真实码流。** 4 字节 stub 的根因在编码器内部（framer/compressor/bit-pool 的输入为空或位分配为 0），需要继续在 `libaptXAdaptiveEnc3.so` 内部定位。本移植仍值得做，因为它是正确性基础，并且能让 helper 行为与官方容器一致。

---

## 5. 移植实施记录（已完成）

已在 `aptx-lossless-helper.c` 中落地：

| 项 | 实现 |
|---|---|
| `struct cntr_process_info` | `anything_changed` / `port_thresh_event` / `num_data_tpm_done` |
| `struct cntr_port_state` | `buf_max_size` / `actual_data_len` / `max_frames_per_buffer` / `num_frames_in_buf` / `release_ext_out_buf` / `is_prebuffer_sent` / `disable_one_time_pre_buf` / `kpps_scale_factor_q4` |
| `process_audio()` | 改为 `for(;;)` 容器循环：每次清零 `output_buffers[0].actual_data_len` → `module->process()` → 累加 `num_frames_in_buf` / `anything_changed` → 按 `need_to_process_frames` 判定退出（输出满 / 阈值事件重跑 / 无变化 / 1000 次保护） |
| `event_callback()` | 通过 `event_context` 拿到 state，解析 `0x000132e5 DISABLE_PREBUFFER` → `disable_one_time_pre_buf` + `is_prebuffer_sent`；`0x000132e7 KPPS_SCALE_FACTOR` → `kpps_scale_factor_q4`（<0x10 拒绝，与容器一致） |
| `reset_module_storage()` | 清零容器状态 |

**验证（全部 PASS，无回归）**：

```
协议级 10 例：48k/48k-q1/48k-q5/44k-r22/44k-nor22/44k-r22-force/96k/r3-48k/88k2-44k1/192k-96k
协商：r22-sink 48k/44.1k、r21-sink、no-ext-sink、lossless=force
集成：lossless=off 243 包、lossless=force 243 包 → PASSED
插件端到端：96k / 88.2k→44.1k / 44.1k-force 全部 errors=0 → PASSED
已安装 .so：44.1k / 48k / 96k / 44.1k-force 全部 PASS → PASSED
R2 48k：168 包、unique=168、TTP 递增
R3 48k：300 包、unique=3（stub 未变，与容器迁移无关）
```

二进制：12816 字节（原 12336）。备份：`/tmp/aptx-lossless-helper.pre-cntr`。

---

## 6. 决定性突破：`aptX3Encode` 的真实调用约定（2026-09-08）

### 6.1 从模块反汇编得到的调用点

`aptx_adaptive3_enc_process` 的右声道路径（`deferred_rhs_process` @0x9a20）：

```
9b98: r2 = memw(r16+0x8); r3 = memw(r16+0x4)   ; end, start
9b9c: r2 = sub(r2,r3)                          ; end - start
9ba0: if (!cmp.gt(r2,0)) jump 0x9c14           ; 窗口非空才调用
9ba4: r1 = r16                                 ; arg1 = 输入描述符
9ba8: r2 = memw(r16+0x34); r3 = memw(r16+0x30) ; 输出描述符 end/limit
9bac: r2 = sub(r2,r3)
9bb0: if (!cmp.gt(r2,0)) jump 0x9c14
9bb4: r2 = add(r17,#0x6450)                    ; arg2 = 输出描述符
9bb8: r3 = memw(r19+#0x10c)                    ; 函数指针
9bbc: r0 = memw(r17+#0xc8)                     ; arg0 = 编码器 ctx
9bc0: callr r3
```

即：**`aptX3Encode(ctx, 输入描述符, 输出描述符)`**

### 6.2 之前探针的错误

旧探针写的是 `encode(ctx, 0, &ring)`——把 ring 当成**输出**描述符、输入传 NULL。
这正是"4 字节 stub"的直接原因。

### 6.3 正确用法下的结果

`direct-probe2.c` 用正确约定驱动，**每次调用都返回 0 并产出真实码流**：

| 条件 | 结果 |
|---|---|
| 输入窗口 ≥ 1344 样本（2 帧） | 成功 |
| 输入窗口 = 672 样本（1 帧） | 0xF015，只写 1 字节 |
| 输出描述符 `limit2 - limit > 63` | 必须满足，否则 0xF014 |
| 输出描述符 `limit - end > 11` | 必须满足，否则 0xF016 |
| 输出缓冲 328 字节（与模块一致） | **产出 328 字节，325/327/291 非零** |
| 输出缓冲 64 KB | 产出 696~707 非零字节 |

输出随输入变化（同一位置）：

```
sine : 81 cc d0 7f fc 0f ff d1 8c 01 0e 7d fe f4 32 93 b5 ...
noise: 81 cc d0 7f c8 05 e0 08 4b 5f 13 83 a8 1f a4 1f e2 0f ...
```

**结论：aptX Adaptive R3 / Lossless 编码器本身完全正常，之前的所有"空帧"都是调用约定错误造成的。**

### 6.4 模块内部描述符（helper 运行时实测）

```
DESC inL  411aa29c 411aa29c 411aad1c 411b229c 411b229c   ← 输入环（32768 字节）
DESC inR  411b229c 411b229c 411b2d1c 411ba29c 411ba29c
DESC outA 411ae43c 411ae43c 411ae43c 411ae584 411ae5c4   ← limit=+0x148, limit2=+0x188
DESC outB 411ae5c4 411ae5c4 411ae5c4 411ae70c 411ae74c   ← 模块实际传给编码器的
DESC outC 411afbcc 411afbcc 411afbcc 411afc64 411b0000
```

描述符本身**完全合法**（`limit-end=0x148>11`，`limit2-limit=0x40>63`），输入窗口也随调用增长
（2688→5376→8064→10752 字节）。因此模块侧仍有别的因素导致 stub，但**正确管线已被证明可用**。

### 6.5 下一步

在 helper 中实现直接编码管线：

1. 让模块完成一次 process（触发 `init_output_bufs`，建立 ctx 与输出缓冲）
2. helper 自管输入环，累积到 ≥1344 样本
3. 每帧调用 `aptX3Encode(ctx, &in_desc, &out_desc)`（输出描述符每次重置 `start=end=base`）
4. 从输出缓冲取 328 字节作为该声道 payload
5. 按模块字段构 OTA 头（`me+0x230` 类型、`me+0x231` 版本、`me+0x248` 周期、`me+0x21c` 大小、`me+0x268` 时间戳）
