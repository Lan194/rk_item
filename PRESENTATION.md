# RK3506 通信控制程序 —— 答辩文档

> 面向答辩场景的精炼版本，突出**问题-方案-结果**主线和**技术亮点**。

---

## 一、课题背景

### 1.1 场景

项目运行于 **RK3506 嵌入式 Linux 开发板**，通过串口与 PTU 上位机通信，
实现一个"嵌入式设备 ↔ PC 上位机"的完整通信控制链路。

### 1.2 核心需求

| 方向 | 功能 |
|------|------|
| **板端 → PC** | 周期上报扩展坞状态、环境感知数据（含触摸坐标） |
| **PC → 板端** | 下发背光开关/调档、蜂鸣器开关、校时指令 |
| **硬件交互** | TSC2046 电阻屏坐标读取、PWM 背光控制、GPIO 蜂鸣器 |

### 1.3 技术栈

```
硬件  RK3506 (ARM Cortex-A7) + TSC2046 电阻屏 + PWM 背光 + GPIO 蜂鸣器
OS    Buildroot Linux 6.1
协议  私有串口协议，帧头+CRC 校验
语言  C (ARM 交叉编译)
```

---

## 二、系统架构

### 2.1 模块划分

```
┌─────────────────────────────────────────────────────────────┐
│                        main.c                               │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐    │
│  │ uart_comm │  │  crc_util │  │protocol  │  │protocol  │    │
│  │ (串口收发) │  │ (CRC8/16) │  │_pack     │  │_parse    │    │
│  └──────────┘  └──────────┘  │ (帧打包)  │  │ (帧解析)  │    │
│       ▲        ▲              └────┬─────┘  └────┬─────┘    │
│       │        │                   │              │           │
│  ┌────┴────────┴────┐     ┌───────┴──────────────┴──────┐  │
│  │   hw_touch       │     │       business_state_t        │  │
│  │   hw_driver      │◄────┤  时间/监控/硬件状态/触摸坐标  │  │
│  └──────────────────┘     └──────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 数据流

```
PTU 上位机 ──UART──► uart_recv_data ──► ring_push
                                          │
                                          ▼
                              protocol_parse_ptu_cmd
                               │         │         │
                               ▼         ▼         ▼
                          hw_driver  business   send_ptu_ack
                          (背光/蜂鸣)  (校时)
                                      (ACK 回 PTU)

hw_touch_poll ──► g_dev_state.touch_x/y
                         │
                         ▼
              protocol_pack ──► uart_send_data ──► PTU 上位机
              (sensor/dock)       (CRC16)
```

### 2.3 主循环时序

```
时间轴 (每 100ms 一轮)
│ ① time 刷新  ② sysinfo  ③ dock帧  ④ sensor帧  ⑤ 触摸读取  ⑥ UART收发  │
├─────────────────────────────────────────────────────────────────────┤
│ tick_extend >= 500?    tick_env >= 1000?   touch_throttle >= 100?  │
```

---

## 三、关键技术难点与解决方案

### 难点 1：CRC8 校验算法未知

**问题**：协议文档只写"下行指令带 CRC8 校验"，没给参数。
初始用异或校验（`calc_crc8_xor`），所有指令校验失败被丢弃。
板子现象：`[E]frame func=0x03 CRC8 err recv=0x46 calc=0xFD` 满屏刷。

**解决方案**：反推法

1. 抓包得到 6 组 (帧数据, CRC) 对：
   ```
   func=0x03 beep ON     → recv CRC=0x46
   func=0x03 beep OFF    → recv CRC=0x50
   func=0x02 level 2     → recv CRC=0x1E
   func=0x02 level 4     → recv CRC=0x6A
   func=0x01 bl ON       → recv CRC=0x82
   func=0x01 bl OFF      → recv CRC=0x94
   ```
2. 用脚本穷举所有标准 CRC8 参数组合（poly × init × refin × refout 约 6000 种）
3. **6 组全部命中 `poly=0x07, init=0x00, refin=false, refout=false`**

**结果**：替换异或为标准 CRC8，指令解析立即恢复，背光/蜂鸣器/校时全部可用。

---

### 难点 2：环形缓冲区半包活锁

**问题**：UART 丢字节或噪声导致半包时，parser 回退 rd 等待后续数据。
但如果后续一直没数据（对端断开、噪声），rd 永远卡住 → `ring_push` 看到缓冲满 →
**所有新指令永久丢弃**，程序不崩溃但永久失联。

这是典型的**静默故障**——没有任何错误日志，但功能全废。

**解决方案**：活锁检测器

```c
static int partial_stall = 0;
#define PARTIAL_STALL_LIMIT 5  // 5 次 = 约 500ms

// 半包时：
partial_stall++;
if(partial_stall >= PARTIAL_STALL_LIMIT) {
    ring_advance(ring_size, p_rd, 1);  // 跳过这个残留帧头
    partial_stall = 0;
    continue;
}
*p_rd = frame_start;
return;
```

| 指标 | 修复前 | 修复后 |
|------|--------|--------|
| 半包处理 | 永久等待 | 500ms 超时后跳过 |
| 长时运行 | 可能永久失联 | 自愈能力 |

---

### 难点 3：TSC2046 触摸坐标映射

**问题链条**（三次迭代才搞定）：

```
第 1 次  直接透传 raw 值
         → Y 半屏就顶到 2048
         根因：TSC2046 是 12-bit ADC (0~4095)，硬钳位到 2048 等于截半

第 2 次  用 EVIOCGABS 查驱动声明的 0~4095 做归一化
         → X 起始就是 1300，Y 终止只有 1500
         根因：驱动声明的是 ADC 全量程，但屏幕实际可触摸区域只覆盖中间一段
         （比如 raw X 实际范围是 800~3700，用 0~4095 映射等于把原点向左推了一大截）

第 3 次  放弃 EVIOCGABS，改为运行时自动校准
         → 用户摸几个角，程序自己学习 min/max，映射自动正确
```

**最终方案**：自动校准

```c
// 启动时设极端值，等待触摸学习
abs_x_min = 4095; abs_x_max = 0;
abs_y_min = 4095; abs_y_max = 0;

// 每次触摸更新范围
abs_x_min = min(abs_x_min, raw_x);
abs_x_max = max(abs_x_max, raw_x);
abs_y_min = min(abs_y_min, raw_y);
abs_y_max = max(abs_y_max, raw_y);

// 范围足够后校准完成
if(abs_x_max - abs_x_min > 500 && abs_y_max - abs_y_min > 500)
    calibrated = 1;
```

**为什么这是更好的方案**：
- 自适应任何触摸芯片（TSC2046、ADS7846、FT5406...）
- 自适应屏幕安装偏移（物理屏幕和触摸层可能没对齐）
- 不需要硬编码任何校准参数

---

### 难点 4：sensor 报文字段偏移

**问题**：上位机收到的 X 和 Y 显示在错误位置。

**解决**：根据上位机显示现象反推字段顺序

```
上位机期望:  out[24]=罗盘, out[26]=X, out[28]=Y   ✓
最初写的:    out[24]=X,   out[26]=Y, out[28]=罗盘 ✗
中间版本:    out[24]=X,   out[26]=罗盘, out[28]=Y ✗
最终确定:    out[24]=罗盘, out[26]=X, out[28]=Y   ✓
```

---

### 难点 5：系统时间不更新

**问题**：`business_update_time()` 绑在 `tick_env>=1000` 分支里，
但触摸发送会 `tick_env=0` 重置 → **持续触摸时时间冻住**。

**解决**：解耦——每轮循环开头都调 time 刷新

```c
while(g_running) {
    business_update_time();           // 每轮都调，独立于 tick
    if(tick_extend >= 500) { ... }
    if(tick_env >= 1000) { ... }
    ...
}
```

---

## 四、可靠性设计

### 4.1 发送完整性

```c
// 循环 write，处理 EINTR 和 short write
while(sent < len) {
    ssize_t n = write(fd, buf + sent, len - sent);
    if(n < 0) { if(errno == EINTR) continue; return -1; }
    if(n == 0) return -1;
    sent += n;
}
```

内核发送缓冲满时 `write()` 可能只写一部分，循环写保证完整。

### 4.2 非阻塞 I/O

- UART 读：`VMIN=0, VTIME=1`（最多等 10ms）
- 触摸：`O_NONBLOCK`，无事件立即返回
- 主循环：`usleep(100ms)`，响应性好

### 4.3 优雅退出

```c
signal(SIGINT,  on_signal);
signal(SIGTERM, on_signal);
// while(g_running) 循环，收到信号置 g_running=0
// 退出前 hw_touch_close() + uart_close()
```

### 4.4 安全兜底

| 场景 | 处理 |
|------|------|
| 触摸设备不存在 | `hw_touch_init()` 返回 -1，后续 poll 返回 -2，主循环跳过 |
| /proc 读取失败 | 保持上次值（`g_dev_state.cpu_usage` 等） |
| 校时参数非法 | 拒绝并返回 -1，不更新任何字段 |
| 校时 clock_settime 失败 | 仍更新 g_dev_state（上位机以 PTU 校时值为准） |
| CRC 校验失败 | 丢弃该帧，不阻塞后续帧 |
| 触摸抬起 | 坐标立即归零，不上报最后按下位置 |

---

## 五、性能指标

| 指标 | 数值 | 说明 |
|------|------|------|
| 主循环周期 | 100ms | 精确（usleep） |
| dock 帧发送周期 | 500ms | 协议要求 |
| sensor 帧发送周期 | 1000ms | 协议要求 |
| 触摸即时帧延迟 | <200ms | 100ms 节流 + UART 发送 |
| CPU 采样间隔 | 500ms | 两次采样差值计算 |
| 环形缓冲溢出丢字节 | 极少 | 512B 足够粘包场景 |
| 启动到首次上报 | <1s | 初始化轻量 |

---

## 六、工程实践

### 6.1 编译警告零容忍

```makefile
CFLAGS = -Wall -Wextra
```

每轮 make 确认无警告通过。

### 6.2 模块化设计

- 每个 `.c/.h` 对应一类职责
- 只有 main.c 知道所有模块，其他模块只知道自己需要的头文件
- `g_dev_state` 是唯一跨模块共享的数据，集中管理

### 6.3 注释规范

- 文件头 @file/@brief 说明模块职责
- 结构体字段注释 + 对应报文字段偏移
- 关键函数 @brief/@param/@return
- 魔数用 #define 命名（`TOUCH_OUT_MAX`, `PARTIAL_STALL_LIMIT`, `CALIB_RANGE_THRESH`）

---

## 七、代码规模

| 文件 | 行数 | 职责 |
|------|------|------|
| main.c | ~125 | 主循环 + 环形缓冲 + 调度 |
| uart_comm.c/h | ~80 | 串口封装 |
| crc_util.c/h | ~70 | CRC8 + CRC16 |
| protocol_pack.c/h | ~85 | 帧打包 |
| protocol_parse.c/h | ~180 | 帧解析 + 活锁保护 |
| business.c/h | ~180 | 业务状态 + 系统监控 + 校时 |
| hw_driver.c/h | ~175 | 背光 + 蜂鸣器 |
| hw_touch.c/h | ~240 | TSC2046 驱动 + 自动校准 |
| **合计** | **~1135** | |

---

## 八、总结与展望

### 已完成

- ✅ 完整通信协议栈（CRC 反推、粘包处理、指令解析、状态打包）
- ✅ TSC2046 电阻屏驱动 + 运行时自动校准（无需硬编码参数）
- ✅ CPU/内存/磁盘实时监控 + 系统时间动态刷新
- ✅ 半包活锁防护、发送完整写入、优雅退出等可靠性加固
- ✅ 零警告编译、模块化设计、完整中文注释

### 可扩展方向

1. **罗盘驱动**：目前 compass 硬置 0，接入磁力计后填入真实航向角
2. **触摸多点**：当前只处理 TSC2046 单点，可扩展 FT5406 等多点芯片
3. **校准持久化**：当前校准值重启后丢失，可写入 /etc 或 EEPROM 加速下次启动
4. **守护进程化**：加入 daemon 化 + 自动重启脚本
5. **日志分级**：LOG_D/LOG_I/LOG_W/LOG_E 宏 + syslog 输出

---

## 九、Q&A 准备

**Q: 为什么 CRC8 不用现成库，要自己算？**
A: 协议文档没给参数，反推后才确定 poly=0x07。如果用库还得翻源码找对应参数，反而更慢。而且 CRC8 实现也就十几行代码。

**Q: 环形缓冲区为什么不用 mutex？**
A: main 循环单线程，read→push→parse 是线性流程，不存在并发访问。mutex 只会增加开销和死锁风险。

**Q: 触摸坐标为什么不用 EVIOCGABS？**
A: TSC2046 驱动声明 ABS_X=0~4095（ADC 全量程），但屏幕实际可触摸区域只覆盖中间一段。用 0~4095 映射等于从一开始就错了。我们改成运行时自动校准，摸几个角就收敛到真实范围，还能自适应不同芯片。

**Q: 半包活锁概率大吗？**
A: 正常通信几乎不会。UART 丢字节、对端突然断开、串口噪声时会触发。概率不大但一旦触发就是"静默故障"——不崩溃但功能全废，必须修。

**Q: CPU 利用率怎么两次采样？**
A: 第一次调只有一个样本，保持旧值；第二次开始有两个样本，算 delta 差值。500ms 调一次足够稳定。

**Q: 自动校准的阈值 500 怎么来的？**
A: TSC2046 分辨率 12-bit（0~4095），半屏范围就有 ~2000。阈值 500 意味着只要摸到屏幕 1/8 以上区域就算校准完成，足够判定"不是误触"同时又不会让用户摸太多角。
