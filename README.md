# RK3506 通信控制程序说明文档

## 一、项目概述

本项目运行于瑞芯微 RK3506 嵌入式 Linux 开发板，实现板端与 PTU 上位机之间的
串口通信协议栈。核心功能包括：

- **状态上报**：周期性向 PTU 发送扩展坞状态报文（500ms）和环境感知报文（1000ms）
- **指令接收**：解析 PTU 下发的背光开关、背光调档、蜂鸣器开关、校时指令
- **触摸坐标**：读取 TSC2046 电阻屏，自动校准后实时上报
- **系统监控**：从 /proc 读取 CPU、内存、磁盘使用率

### 硬件环境

| 项目 | 规格 |
|------|------|
| CPU | RK3506 ARM Cortex-A7 |
| 操作系统 | Buildroot Linux 6.1 |
| 串口 | /dev/ttyS3，115200bps，8N1 |
| 触摸屏 | TSC2046 SPI 电阻屏，节点 /dev/input/event0 |
| 背光 | PWM 驱动，sysfs 节点 /sys/class/backlight/backlight |
| 蜂鸣器 | GPIO24，高电平触发 |

### 软件依赖

- 内核 input 子系统（CONFIG_INPUT=y）
- backlight class 驱动（CONFIG_BACKLIGHT=y）
- sysfs GPIO 接口（CONFIG_GPIO_SYSFS=y）

---

## 二、目录结构

```
test_rk/
├── main.c           # 主循环 + 环形缓冲区 + 调度
├── uart_comm.c/h    # UART 串口封装
├── crc_util.c/h     # CRC8 / CRC16 算法
├── protocol_pack.c/h    # 上报帧打包
├── protocol_parse.c/h   # 下行帧解析
├── business.c/h     # 业务状态 + 系统监控 + 校时
├── hw_driver.c/h    # 背光 / 蜂鸣器驱动
├── hw_touch.c/h     # TSC2046 触摸驱动 + 自动校准
├── log_util.h       # LOG_I / LOG_E 宏
└── Makefile         # ARM 交叉编译
```

---

## 三、通信协议

### 3.1 上位机 → 板端（下行指令）

帧头 `0x55 0xAA`，CRC8 覆盖 0~6 字节。

| 功能码 | 名称 | 数据区 | 帧长 |
|--------|------|--------|------|
| 0x01 | 背光开关 | `[3]` 0/1 | 8 |
| 0x02 | 背光档位 | `[3]` 1~5 | 8 |
| 0x03 | 蜂鸣器 | `[3]` 0/1 | 8 |
| 0x04 | 校时 | `[3..4]`年大端 `[5]`月 `[6]`日 `[7]`时 `[8]`分 `[9]`秒 | 11 |

**应答帧**：固定 8 字节 `0xAA 0xAA func result 0 0 0 CRC8`，CRC8 覆盖 0~6。

### 3.2 板端 → 上位机（上报报文）

均 48 字节，大端编码，CRC16-Modbus 覆盖 0~45 字节，高字节在前发送。

**扩展坞状态报文（500ms 周期）**

| 偏移 | 大小 | 含义 |
|------|------|------|
| 0~1 | 2 | 帧头 `0xAA 0xBB` |
| 2 | 1 | ID `0x01` |
| 3 | 1 | 长度 `0x30` |
| 4~7 | 4 | 序列号（自增） |
| 8~9 | 2 | 年 |
| 10 | 1 | 月 |
| 11 | 1 | 日 |
| 12 | 1 | 时 |
| 13 | 1 | 分 |
| 14 | 1 | 秒 |
| 15 | 1 | 内存使用率 % |
| 16 | 1 | CPU 使用率 % |
| 17 | 1 | 磁盘使用率 % |
| 18 | 1 | 心跳间隔（固定 2） |
| 19~45 | 27 | 预留（填 0） |
| 46~47 | 2 | CRC16 |

**环境感知状态报文（1000ms 周期 + 触摸即时发送）**

| 偏移 | 大小 | 含义 |
|------|------|------|
| 0~7 | 8 | 同 dock 帧 |
| 8~9 | 2 | 温度 × 10 |
| 10~11 | 2 | 湿度 |
| 12~13 | 2 | 大气压 |
| 14~15 | 2 | 海拔 |
| 16~17 | 2 | 氧气 |
| 18~19 | 2 | 一氧化碳 |
| 20~21 | 2 | 硫化氢 |
| 22~23 | 2 | 甲烷 |
| 24~25 | 2 | 罗盘航向角 |
| 26~27 | 2 | 触摸 X（0~2048） |
| 28~29 | 2 | 触摸 Y（0~2048） |
| 30~45 | 16 | 预留 |
| 46~47 | 2 | CRC16 |

---

## 四、CRC 校验

项目使用两种不同的 CRC 算法，来源各异：

### 4.1 CRC8（下行指令帧尾）

| 参数 | 值 |
|------|----|
| 多项式 | 0x07 |
| 初值 | 0x00 |
| 反射输入 | 否 |
| 反射输出 | 否 |
| 输出异或 | 0x00 |

**反推依据**：项目初期代码用异或校验（`calc_crc8_xor`），导致所有 PTU 下行指令校验失败。
通过抓包得到 6 组 (帧数据, CRC) 对，用脚本穷举所有标准 CRC8 参数，
**6 组全部命中 `poly=0x07, init=0x00`**，故确定为 PTU 协议要求。

### 4.2 CRC16-Modbus（上报报文帧尾）

| 参数 | 值 |
|------|----|
| 多项式 | 0xA001（反转 0x8005） |
| 初值 | 0xFFFF |
| 反射输入 | 是 |
| 反射输出 | 是 |

按协议文档实现，与 PTU 上位机解析一致。

---

## 五、触摸坐标映射

### 5.1 为什么不用 EVIOCGABS

常规做法是初始化时通过 `EVIOCGABS` ioctl 查询驱动声明的 ABS_X/ABS_Y 范围，
然后用这个范围做归一化映射。但 TSC2046 驱动声明的是 ADC 全量程 0~4095，
而屏幕实际可触摸区域只覆盖其中一部分（如 raw X=800~3700），
用 0~4095 映射会导致：**原点偏移、满屏顶不到 2048**。

### 5.2 自动校准方案

彻底废掉 EVIOCGABS，改为**运行时动态学习**：

```
启动时   abs_x_min=4095, abs_x_max=0   ← 极端值，等待触摸学习
         abs_y_min=4095, abs_y_max=0

触摸时   每摸一个点: abs_x_min = min(abs_x_min, raw_x)
                   abs_x_max = max(abs_x_max, raw_x)

校准完成  X 范围>500 且 Y 范围>500 → 打 CALIBRATED 日志
         后续映射用学习到的真实物理范围
```

用户摸几个角后自然收敛到屏幕真实触摸区域，无需硬编码任何参数。

### 5.3 映射公式

```c
// Y 轴直通：上=0 下=2048
out = (raw - abs_y_min) * 2048 / (abs_y_max - abs_y_min);

// X 轴反转：TSC2046 默认左大右小，协议要求左=0 右=2048
out = 2048 - ((raw - abs_x_min) * 2048 / (abs_x_max - abs_x_min));
```

### 5.4 校准流程日志示例

```
[I]touch dev=/dev/input/event0 (auto-calibration mode)
[I]touch init: please touch all 4 corners to calibrate
[I]touch calib X[3800..2600] Y[3100..200]        ← 摸左上+右上角
[I]touch calib X[3800..800]  Y[3700..200]        ← 摸左下角
[I]touch CALIBRATED  X[3800..800] Y[3700..150]    ← 范围足够，校准完成
[I]touch raw=(2450,180) mapped=(410,95) [calibrated]
```

---

## 六、系统监控实现

### 6.1 CPU 利用率（/proc/stat）

两次采样计算增量：

```
total = user + nice + system + idle + iowait + irq + softirq + steal
idle  = idle
usage = (delta_total - delta_idle) / delta_total * 100
```

首次调用只有一个样本，保持旧值；第二次起有效。

### 6.2 内存使用率（/proc/meminfo）

```
usage = (MemTotal - MemAvailable) / MemTotal * 100
```

使用 `MemAvailable` 而非 `MemFree`，更接近真实可用量（含可回收缓存）。

### 6.3 磁盘使用率（statvfs）

```
usage = (total_blocks - available_blocks) / total_blocks * 100
```

统计根分区 `/`。

---

## 七、环形缓冲区与粘包处理

### 7.1 为什么需要环形缓冲区

UART 接收是流式的，一次 `read()` 可能读到：
- 一帧完整数据
- 半帧（半包）
- 多帧（粘包）
- 一帧半（粘包 + 半包）

解析器不能假设一次 read 就能拼好一帧，必须把所有字节先存入缓冲区，
然后逐帧扫描。

### 7.2 设计要点

- 缓冲区大小 **512 字节**（`RING_BUF_SIZE`）
- 写指针 `ring_wr`，读指针 `ring_rd`，`wr == rd` 为空
- `ring_push` 写入时满则丢弃新字节（保护旧数据）
- `protocol_parse_ptu_cmd` 扫描帧头 `0x55 0xAA`，取功能码后按帧长取数据

### 7.3 半包活锁保护（`partial_stall`）

**问题**：当缓冲区尾部只有 0x55（半个帧头），parser 回退 rd 等待下一轮，
但如果后续 UART 一直没数据（对端断开、噪声），parser 每次都从同一位置失败，
`ring_rd` 永远不动，`ring_push` 看到缓冲满 → **后续所有指令永久丢弃**。

**解决**：`partial_stall` 计数器，连续 5 次（约 500ms）半包等待后，
跳过那个残留 0x55（`ring_advance(..., 1)`），重新扫描。

---

## 八、主循环调度

```
┌──────────────────────────────────────────────────────┐
│  while(g_running) {                                  │
│                                                       │
│  ① time 刷新          —— 每轮都调（time() 开销极小）   │
│  ② sysinfo 刷新       —— tick_extend>=500ms          │
│  ③ dock 帧发送        —— tick_extend>=500ms          │
│  ④ sensor 帧发送      —— tick_env>=1000ms            │
│  ⑤ 触摸即时帧发送     —— 触摸事件 + 节流 100ms       │
│  ⑥ UART 接收 + 解析   —— VTIME=1, VMIN=0             │
│                                                       │
│  usleep(100ms);                                      │
│  }                                                   │
└──────────────────────────────────────────────────────┘
```

**为什么 time 不跟 tick 耦合？**

之前 time 刷新绑在 `tick_env>=1000` 分支里，但触摸发送会 `tick_env=0`，
导致持续触摸时 time 永远不更新。现在每轮循环开头都调 `business_update_time()`，
完全独立于其他周期任务。

---

## 九、编译与部署

### 9.1 编译

```sh
# Makefile 已硬编码交叉编译器路径
make clean && make

# 产物：rk3506_comm (ARM ELF 32-bit)
```

### 9.2 部署

```sh
# 板端挂载 /mnt 指向主机工程目录
cp rk3506_comm /mnt/
cd /mnt
./rk3506_comm
```

### 9.3 开机自启

```sh
# 在 /etc/init.d/ 下创建启动脚本，或 systemd service
chmod +x /mnt/rk3506_comm
echo "/mnt/rk3506_comm &" >> /etc/profile
```

### 9.4 权限要求

- 校时指令需要 **root** 权限（`clock_settime(CLOCK_REALTIME)`）
- 其他功能普通用户即可

---

## 十、调试命令

```sh
# 查看触摸设备范围（驱动声明的，不一定是实际可触摸范围）
cat /sys/class/input/input0/abs/ABS_X/minimum
cat /sys/class/input/input0/abs/ABS_X/maximum

# 实时抓取触摸事件（看看 raw 值到底分布在哪个区间）
timeout 3 cat /dev/input/event0 | xxd

# 查看 UART 收发
stty -F /dev/ttyS3 115200 raw
cat /dev/ttyS3 & echo -n "test" > /dev/ttyS3

# 查看系统负载
cat /proc/stat
cat /proc/meminfo
df /
```

---

## 十一、版本历史

| 版本 | 日期 | 改动 |
|------|------|------|
| v1.0 | 2026-09-22 | 初始骨架：CRC8 用异或，sensor 帧预留坐标位 |
| v1.1 | 2026-09-23 | 修正 CRC8（poly=0x07, init=0x00），6 组抓包反推验证 |
| v1.2 | 2026-09-23 | 加入触摸驱动（ADS7846），坐标归一化 0~2048 |
| v1.3 | 2026-09-23 | 修正 sensor 报文字段顺序：罗盘 out[24]、X out[26]、Y out[28] |
| v1.4 | 2026-09-23 | 加入 CPU/内存/磁盘实时监控，修复 time 刷新与 tick 耦合 |
| v1.5 | 2026-09-23 | 修复环形缓冲区半包活锁，发送完整写入，SIGINT 优雅退出 |
| v1.6 | 2026-09-23 | 触摸改用 TSC2046，坐标映射从 EVIOCGABS 改为运行时自动校准 |
