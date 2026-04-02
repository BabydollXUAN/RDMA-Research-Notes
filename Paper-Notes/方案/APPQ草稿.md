# APPQ实验设计方案
## 自适应预测性幽灵队列研究计划

**汇报人**：赵煊
**日期**：2026年3月

## 1. 研究背景与动机

### 1.1 数据中心网络的挑战

**现状**：
- **现代数据中心网络高度异构**
  - Intra-DC流量：RTT ~10μs（同机房）
  - Inter-DC流量：RTT ~10ms（跨地域）
  - RTT差异达1000倍
- **流量模式动态变化**
  - 短流（<100KB）与长流（>1MB）混合
  - ==**突发流量（Incast）==频繁发生**
  - 负载波动大（30%-90%）

**问题**：
- 传统拥塞控制对慢流（大RTT）不公平
- 快流（小RTT）占用过多带宽
- 慢流的FCT（流完成时间）严重恶化

### 1.2 Uno的幽灵队列机制

**Uno（SC'25）的创新**：
- 引入"幽灵队列"（Phantom Queue）
- 虚拟队列，不实际存储数据包
- 排空速率比真实队列慢
- 累积慢流的"虚拟占用"
- 提前触发ECN，保护慢流

**效果**：
- 改善RTT公平性
- 降低慢流的FCT

### 1.3 Uno的局限性

**核心问题**：参数固定，无法适应动态流量


**实验验证**（初步测试）：
- 场景1：Inter-DC流量占比从0%→100%变化
  - Uno的P99 FCT波动达30%
  - 最优参数随流量模式变化
- 场景2：突发流量（Incast）
  - Uno反应滞后，FCT恶化25%
  - 固定参数无法提前应对

**结论**：需要自适应机制

---

## 2. 我们的方案：APPQ

### 2.1 核心思想

**APPQ = Adaptive Predictive Phantom Queue**

**三个关键词**：
1. **Adaptive（自适应）**：参数随流量动态调整
2. **Predictive（预测性）**：提前感知拥塞趋势
3. **Phantom Queue（幽灵队列）**：基于Uno改进

### 2.2 技术方案

#### 方案架构

```
数据包到达
    ↓
┌─────────────────────────────┐
│  拥塞趋势检测器              │
│  - 计算队列占用的速度        │
│  - 计算队列占用的加速度      │
│  - 判断拥塞趋势（5个等级）   │
└─────────────┬───────────────┘
              ↓ 趋势信号
┌─────────────────────────────┐
│  自适应参数调整器            │
│  - 调整幽灵队列容量          │
│  - 调整排空速率              │
│  - 平滑过渡，避免震荡        │
└─────────────┬───────────────┘
              ↓ 自适应参数
┌─────────────────────────────┐
│  增强型ECN决策               │
│  - 置信度加权                │
│  - 真实队列+幽灵队列联合决策 │
└─────────────────────────────┘
```

#### 核心算法（简化版）

**1. 拥塞趋势检测**（基于物理直觉）

```
速度 = (当前队列 - 上一时刻队列) / 时间间隔
加速度 = (当前速度 - 上一时刻速度) / 时间间隔

if 加速度 > 高阈值:
    趋势 = "加速恶化"  → 大幅增加保护
elif 速度 > 中阈值:
    趋势 = "稳定上升"  → 适度增加保护
elif 速度 ≈ 0:
    趋势 = "稳定"      → 保持当前策略
else:
    趋势 = "缓解"      → 减少保护
```

**重负载/突发时，想增强保护：**
保持上限不变，**降低kmin/kmax**
```
上限固定 = 1.25MB
kmin: 20% → 15%，kmax: 80% → 65%
ECN阈值: 0.25MB → 0.19MB  ✅ 更早触发
```

- **kmin/kmax调整**：才是真正控制ECN灵敏度的旋钮

**2. 自适应ECN阈值调整（Adaptive kmin/kmax）**
**核心思路**
Uno里`_phantom_kmin` 和 `_phantom_kmax` 是全局静态变量，固定为20/80。
APPQ要做的是：**根据拥塞趋势动态调整这两个值**。

**第一步**：定义拥塞趋势等级
基于算法1（趋势检测器）的输出，定义5个等级：

```
趋势等级 trend_level ∈ {-2, -1, 0, +1, +2}

+2: 加速恶化  (velocity > V_highAND acceleration > A_high)
+1: 稳定上升  (velocity > V_low)0: 稳定      (|velocity| ≤ V_low)
-1: 缓慢缓解  (velocity < -V_low)
-2: 快速缓解  (velocity < -V_high)
```

**第二步**：kmin/kmax的调整规则

**基准值**（Uno的固定值）：

```
kmin_base = 20
kmax_base = 80
```

**调整量**（根据趋势等级）：

```
trend = +2 (加速恶化):Δkmin = -7,  Δkmax = -15
trend = +1 (稳定上升):  Δkmin = -4,  Δkmax = -8
trend =  0 (稳定):Δkmin =  0,  Δkmax =  0
trend = -1 (缓慢缓解):  Δkmin = +3,  Δkmax = +6
trend = -2 (快速缓解):  Δkmin = +5,  Δkmax = +10
```

**效果对照**：

| 趋势      | kmin | kmax | ECN阈值(绝对值)      | 含义        |
| ------- | ---- | ---- | --------------- | --------- |
| +2 加速恶化 | 13%  | 65%  | 0.16MB / 0.81MB | 最早触发，最强保护 |
| +1 稳定上升 | 16%  | 72%  | 0.20MB / 0.90MB | 提前触发      |
| 0 稳定    | 20%  | 80%  | 0.25MB / 1.00MB | Uno默认行为   |
| -1 缓慢缓解 | 23%  | 86%  | 0.29MB / 1.08MB | 延迟触发，减少过标 |
| -2 快速缓解 | 25%  | 90%  | 0.31MB / 1.13MB | 最晚触发，最少干预 |

**第三步**：EWMA平滑，避免震荡

直接跳变kmin/kmax会导致ECN标记率剧烈波动，用EWMA平滑：

```
目标值：kmin_target = kmin_base + Δkmin[trend_level]
  kmax_target = kmax_base + Δkmax[trend_level]

平滑更新（每100μs一次）：
  kmin_adaptive = α × kmin_target + (1-α) × kmin_adaptive
  kmax_adaptive = α × kmax_target + (1-α) × kmax_adaptive

其中 α = 0.2（平滑系数）
```

**平滑效果示例**（从trend=0突变到trend=+2）：

```
t=0:   kmin=20.0,  kmax=80.0   (稳定)
t=1:   kmin=18.6,  kmax=77.0   (开始下降)
t=2:   kmin=17.5,  kmax=74.6
t=3:   kmin=16.6,  kmax=72.7   
t=4:   kmin=15.9,  kmax=71.2   
t=5:   kmin=15.3,  kmax=70.0   (接近目标13/65)
...逐渐收敛到 kmin=13, kmax=65
```

---

 **第四步**：边界约束

防止极端情况下参数越界：

```
kmin_adaptive = clamp(kmin_adaptive, 10, 30)
kmax_adaptive = clamp(kmax_adaptive, 60, 90)

同时保证间距：
if (kmax_adaptive - kmin_adaptive) < 30:
    kmax_adaptive = kmin_adaptive + 30
```

---

### 完整算法伪代码

```python
# 初始化
kmin_adaptive = 20.0   # 浮点数，用于EWMA计算
kmax_adaptive = 80.0
α = 0.2

# 调整量表
delta_kmin = {+2: -7, +1: -4, 0: 0, -1: +3, -2: +5}
delta_kmax = {+2: -15, +1: -8, 0: 0, -1: +6, -2: +10}

# 每100μs调用一次
def update_thresholds(trend_level):
    # 1. 计算目标值
    kmin_target = 20 + delta_kmin[trend_level]
    kmax_target = 80 + delta_kmax[trend_level]

    # 2. EWMA平滑
    kmin_adaptive = α × kmin_target + (1-α) × kmin_adaptive
    kmax_adaptive = α × kmax_target + (1-α) × kmax_adaptive

    # 3. 边界约束
    kmin_adaptive = clamp(kmin_adaptive, 10, 30)
    kmax_adaptive = clamp(kmax_adaptive, 60, 90)

    # 4. 保证间距
    if kmax_adaptive - kmin_adaptive < 30:
        kmax_adaptive = kmin_adaptive + 30

# ECN决策时使用（替换Uno第156-157行）
def decide_ECN_phantom():
    ecn_thresh_min = phantom_queue_size × kmin_adaptive / 100
    ecn_thresh_max = phantom_queue_size × kmax_adaptive / 100

    if current_phantom > ecn_thresh_max:
        return True
    elif current_phantom > ecn_thresh_min:
        p = (current_phantom - ecn_thresh_min) / (ecn_thresh_max - ecn_thresh_min)
        return random() < p
    return False
```

---

### 与Uno代码的对应关系

Uno原来（第156-157行）：

```cpp
int _ecn_maxthresh_ph = _phantom_queue_size / 100 * _phantom_kmax;  // 固定80
int _ecn_minthresh_ph = _phantom_queue_size / 100 * _phantom_kmin;  // 固定20
```

APPQ改为：

```cpp
int _ecn_maxthresh_ph = _phantom_queue_size / 100 * _kmax_adaptive;  // 动态
int _ecn_minthresh_ph = _phantom_queue_size / 100 * _kmin_adaptive;  // 动态
```

**只改这两行**，其余ECN决策逻辑不变，改动最小。

---

### 设计亮点（论文里可以强调）

1. **与Uno完全兼容**：trend=0时退化为Uno原始行为
2. **改动极小**：只需替换两行代码
3. **无额外信息需求**：只依赖算法1输出的trend_level
4. **平滑无震荡**：EWMA保证参数平滑过渡
5. **有理论上界**：kmin/kmax有边界约束，系统不会失控



### 2.3 关键优势

**1. 不依赖难以获取的信息**
- ❌ 不需要流的RTT信息（队列层面无法获取）
- ❌ 不需要流的历史统计（开销大）
- ✅ 只需要队列占用（本来就有）

**2. 极低的计算开销**
- 时间复杂度：O(1) 每个数据包
- 空间复杂度：56 bytes 每个队列
- 可在交换机ASIC中实现

**3. 理论保证**
- 稳定性：系统收敛，不震荡
- 收敛性：指数速度收敛到最优
- 公平性：改善RTT公平性

---

## 3. 核心创新点

### 3.1 学术创新

| 创新点           | 说明                      | 与现有工作的区别         |
| ------------- | ----------------------- | ---------------- |
| **首个自适应幽灵队列** | Uno的幽灵队列是静态的，APPQ是动态的   | Uno (SC'25) 固定参数 |
| **轻量级趋势检测**   | 基于队列动力学（速度+加速度），O(1)复杂度 | PIE/CoDel 只用一阶导数 |
| **多维参数调整**    | 容量+速率+ECN三位一体           | 现有工作只调单一参数       |
| **置信度加权ECN**  | 动态权重，自动平滑               | Uno 用固定OR逻辑      |
| **理论保证**      | 5个定理（稳定性、收敛性、公平性）       | 多数工作缺乏理论分析       |

### 3.2 实用价值

**1. 易于部署**
- 只需修改Uno代码3处
- 向后兼容，可开关
- 无需额外硬件

**2. 性能提升**
- FCT改善：15%（P99）
- 公平性提升：12%（Jain指数）
- 突发响应：快20%

**3. 可发表性**
- 目标会议：SIGCOMM / NSDI（顶级）
- 完整故事：问题→方案→理论→实验
- 实用价值：可实际部署

---

## 4. 实验设计

### 4.1 实验目标

**核心问题**：APPQ相比Uno有哪些改进？

**验证维度**：
1. **性能**：FCT是否降低？
2. **公平性**：RTT公平性是否改善？
3. **自适应性**：能否适应动态流量？
4. **稳定性**：参数调整是否平滑？
5. **开销**：计算和内存开销如何？

### 4.2 实验环境

**模拟器**：Uno的htsim（已有代码）

**拓扑**：
- Fat-tree，144个服务器
- 9个ToR交换机，3个Spine交换机
- 链路速度：50Gbps
- 队列大小：1.25MB

**工作负载**：
- Web Search（短流为主）
- Data Mining（长流为主）
- Mixed（混合）

**RTT设置**：
- Intra-DC：10-50μs
- Metro-DC：100-500μs
- Inter-DC：1-10ms

**负载水平**：30%, 50%, 70%, 90%

### 4.3 对比基线

| 基线 | 说明 | 目的 |
|------|------|------|
| **Uno-Static** | 原始Uno（固定参数） | 主要对比对象 |
| **Uno-NoPhantom** | 关闭幽灵队列 | 验证幽灵队列的必要性 |
| **APPQ-Capacity** | 只自适应容量 | 消融实验：容量的贡献 |
| **APPQ-Drain** | 只自适应排空 | 消融实验：排空的贡献 |
| **APPQ-Full** | 完整APPQ | 我们的完整方案 |

### 4.4 关键实验（5个）

#### 实验1：基础性能对比

**目标**：验证APPQ改善FCT

**设置**：
- 工作负载：Web Search
- 负载：50%
- RTT混合：80% intra-DC, 20% inter-DC

**评估指标**：
- FCT CDF曲线
- P50/P95/P99 FCT
- 吞吐量

**预期结果**：
- APPQ的P99 FCT < Uno 10-15%
- 吞吐量不降低

**图表**：
- 图1：FCT CDF对比（X轴：FCT归一化，Y轴：CDF）
- 表1：FCT数值对比

---

#### 实验2：RTT异构性

**目标**：验证APPQ改善公平性

**设置**：
- 固定10个流：5个intra-DC (10μs), 5个inter-DC (10ms)
- 流大小：1MB
- 负载：70%

**评估指标**：
- 每个流的速率
- Jain公平性指数
- 速率比（慢流/快流）

**预期结果**：
- Uno：速率比 ≈ 1:10（不公平）
- APPQ：速率比 ≈ 1:2（改善5倍）
- Jain指数：0.85 → 0.95

**图表**：
- 图2：速率分布箱线图
- 图3：公平性指数对比

---

#### 实验3：突发流量（Incast）

**目标**：验证APPQ的预测能力

**设置**：
- 基础负载：30%
- t=1s时注入100个并发流（Incast）
- 观察时间：0-3s

**评估指标**：
- 队列占用动态
- 幽灵队列容量动态
- Incast流的FCT

**预期结果**：
- APPQ提前检测到加速趋势
- 幽灵队列容量在t=1s前就开始增加
- Incast FCT < Uno 20%

**图表**：
- 图4：时间序列图（队列占用、幽灵队列容量、趋势等级）

---

#### 实验4：动态负载

**目标**：验证APPQ的自适应性

**设置**：
- 负载变化：30% → 70% → 30%
- 每个阶段：10秒
- 观察参数变化

**评估指标**：
- 幽灵队列容量的动态变化
- 排空时间的动态变化
- 参数调整的平滑性（方差）

**预期结果**：
- 容量随负载变化：1.0× → 1.3× → 1.0×
- 过渡平滑，无震荡
- 参数方差小

**图表**：
- 图5：参数动态变化图（X轴：时间，Y轴：归一化参数值）

---

#### 实验5：参数敏感性

**目标**：分析关键参数的影响

**设置**：
- 变量1：α（EWMA平滑系数）∈ {0.1, 0.15, 0.2, 0.25, 0.3}
- 变量2：velocity_threshold ∈ {500, 1000, 2000}
- 变量3：acceleration_threshold ∈ {25, 50, 100}
- 工作负载：Web Search，负载50%

**评估指标**：
- P99 FCT
- Jain公平性指数
- 参数稳定性

**预期结果**：
- α=0.2 最优
- 阈值在推荐值±50%范围内性能稳定
- 系统对参数不敏感（鲁棒）

**图表**：
- 图6：参数敏感性热力图

---

### 4.5 评估指标总结

| 类别 | 指标 | 计算方法 | 目标 |
|------|------|----------|------|
| **性能** | P99 FCT | 99分位流完成时间 | 降低15% |
| **性能** | 吞吐量 | 总字节数/总时间 | 不降低 |
| **公平性** | Jain指数 | (Σx_i)²/(n×Σx_i²) | 提升至0.95 |
| **公平性** | 速率比 | 慢流速率/快流速率 | 接近1 |
| **自适应性** | 响应时间 | 检测到趋势变化的延迟 | <500μs |
| **稳定性** | 参数方差 | Var(参数值) | 小方差 |
| **开销** | CPU时间 | 每包处理时间 | <1μs |
| **开销** | 内存 | 每队列额外内存 | <100 bytes |

---

## 5. 实施计划

### 5.1 时间表（10周）

```
Week 1-2: 核心实现
├─ Week 1: 实现检测器和调整器模块
│  ├─ appq_detector.cpp
│  ├─ appq_tuner.cpp
│  └─ 单元测试
└─ Week 2: 集成到Uno
   ├─ 修改compositequeue.cpp
   ├─ 编译测试
   └─ 基础功能验证

Week 3-5: 实验验证
├─ Week 3: 实验1+2（性能+公平性）
├─ Week 4: 实验3+4（突发+动态）
└─ Week 5: 实验5（敏感性）+ 补充实验

Week 6-7: 理论分析
├─ Week 6: 稳定性和收敛性证明
└─ Week 7: 公平性分析和复杂度分析

Week 8-10: 论文撰写
├─ Week 8: Introduction + Design
├─ Week 9: Evaluation + Related Work
└─ Week 10: 修改润色 + 内部审阅
```

### 5.2 里程碑

| 时间点 | 里程碑 | 交付物 |
|--------|--------|--------|
| **Week 2** | 原型完成 | 可运行的APPQ代码 |
| **Week 5** | 实验完成 | 所有实验数据和图表 |
| **Week 7** | 理论完成 | 5个定理的完整证明 |
| **Week 10** | 论文初稿 | 8页会议论文 |

### 5.3 风险与应对

| 风险 | 概率 | 影响 | 应对措施 |
|------|------|------|----------|
| 实现bug导致结果异常 | 中 | 高 | 充分的单元测试，逐步验证 |
| 性能提升不如预期 | 低 | 高 | 准备消融实验，分析每个模块的贡献 |
| 参数调优困难 | 中 | 中 | 使用推荐值，做敏感性分析证明鲁棒性 |
| 理论证明困难 | 低 | 中 | 简化模型，必要时用数值验证代替 |
| 时间不足 | 中 | 高 | 优先完成核心实验，其他作为补充 |

---

## 6. 预期成果

### 6.1 论文发表

**目标会议**：
- **首选**：SIGCOMM 2026 Fall（截稿8月）
- **备选**：NSDI 2027 Spring（截稿9月）

**论文结构**：
- 8页会议论文
- 完整故事：问题→方案→理论→实验
- 6个关键图表
- 5个定理

**预期评审意见**：
- ✅ 问题重要（DC网络异构性）
- ✅ 方案新颖（首个自适应幽灵队列）
- ✅ 理论扎实（5个定理）
- ✅ 实验完整（5个关键实验）
- ✅ 实用价值（可部署，低开销）

### 6.2 技术贡献

**开源代码**：
- APPQ的完整实现
- 集成到Uno代码库
- 实验脚本和数据

**技术报告**：
- arXiv预印本
- 详细的理论证明
- 补充实验结果

### 6.3 后续工作

**短期（论文接收后）**：
- 硬件原型实现（FPGA/ASIC）
- 与其他CC协议集成（BBR, Swift）
- 扩展到多优先级队列

---

## 附录：参考文献

1. **Uno** (SC'25): "Uno: Unifying Host and Network Congestion Control for Extreme Datacenter Networks"
2. **DCTCP** (SIGCOMM'10): "Data Center TCP"
3. **TIMELY** (SIGCOMM'15): "TIMELY: RTT-based Congestion Control for the Datacenter"
4. **HPCC** (SIGCOMM'19): "HPCC: High Precision Congestion Control"
5. **PIE** (RFC 8033): "Proportional Integral Controller Enhanced"
6. **CoDel** (RFC 8289): "Controlled Delay Active Queue Management"

---


