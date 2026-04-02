# APPQ 实验 Q&A 文档

## Q1: APPQ 和 Uno 的区别是什么？

**A:** Uno 使用固定的幽灵队列参数（容量、排空速率、ECN 阈值）。
APPQ 在 Uno 基础上增加了三个自适应机制：

| 参数            | Uno                    | APPQ                                     |
| ------------- | ---------------------- | ---------------------------------------- |
| 幽灵队列上限        | 固定 `phantom_size`      | `phantom_size × phantom_size_factor`（动态） |
| ECN kmin/kmax | 固定 `phantom_kmin/kmax` | `adaptive_kmin/kmax`（随拥塞趋势调整）            |
| 排空时间          | 固定 `draining_time`     | `draining_time × drain_time_factor`（动态）  |

启用 APPQ 只需在 Uno 命令行参数基础上加 `-use_appq 1`。

---

## Q2: 如何运行最基础的 APPQ 功能验证？

**A:** 直接运行仿真器，观察 `UseAPPQ: 1` 是否打印：

```bash

cd /home/ustc/APPQ_SC25

sim/datacenter/htsim_uec_entry_modern -use_phantom 1 -use_appq 1 -phantom_size 100000 -end_time 100

    
预期输出包含：

UsePhantomQueue: 1
UseAPPQ: 1
PhantomQueueSize: 100000
```

---

## Q3: 如何运行 APPQ vs Uno 的 FCT 对比实验（基于 fig9 场景）？

**A:** fig9 使用 `htsim_lcp_entry_modern`，流量矩阵在 `.cm.tmp` 文件中。

**Step 1：运行 Uno 基线**
```bash
cd /home/ustc/APPQ_SC25
sim/datacenter/htsim_lcp_entry_modern \
    -o uec_entry \
    -seed 215 \
    -queue_type composite \
    -hop_latency 1000 \
    -switch_latency 0 \
    -nodes 128 \
    -collect_data 0 \
    -topology interdc \
    -os_border 16 \
    -strat rand \
    -linkspeed 100000 \
    -topo lcp/configs/topos/fat_tree_100Gbps.topo \
    -tm uno_0_8_1000MB.tmp \
    -noRto \
    -queueSizeRatio 1 \
    -IntraFiT 100 \
    -InterFiT 2500 \
    -interKmax 60 \
    -interKmin 20 \
    -ecnAlpha 0.65 \
    -usePacing 1 \
    -end_time 2860 \
    -lcpK 6 \
    -interEcn \
    -mdRTT 0.0003515625 \
    -interdcDelay 886500 \
    -kmin 10 \
    -kmax 80 \
    -lcpAlgo aimd_phantom \
    -use_phantom 1 \
    -phantom_size 22400515 \
    -phantom_slowdown 5 \
    -phantom_kmin 2 \
    -phantom_kmax 60 \
    -forceQueueSize 1000000 \
    -noFi \
    > results/uno_baseline.tmp
```

**Step 2：运行 APPQ（在 Uno 命令基础上加 `-use_appq 1`）**
```bash
sim/datacenter/htsim_lcp_entry_modern \
    -o uec_entry \
    -seed 215 \
    -queue_type composite \
    -hop_latency 1000 \
    -switch_latency 0 \
    -nodes 128 \
    -collect_data 0 \
    -topology interdc \
    -os_border 16 \
    -strat rand \
    -linkspeed 100000 \
    -topo lcp/configs/topos/fat_tree_100Gbps.topo \
    -tm uno_0_8_1000MB.tmp \
    -noRto \
    -queueSizeRatio 1 \
    -IntraFiT 100 \
    -InterFiT 2500 \
    -interKmax 60 \
    -interKmin 20 \
    -ecnAlpha 0.65 \
    -usePacing 1 \
    -end_time 2860 \
    -lcpK 6 \
    -interEcn \
    -mdRTT 0.0003515625 \
    -interdcDelay 886500 \
    -kmin 10 \
    -kmax 80 \
    -lcpAlgo aimd_phantom \
    -use_phantom 1 \
    -use_appq 1 \
    -phantom_size 22400515 \
    -phantom_slowdown 5 \
    -phantom_kmin 2 \
    -phantom_kmax 60 \
    -forceQueueSize 1000000 \
    -noFi \
    > results/appq.tmp
```

---

## Q4: 有哪些现成的流量矩阵文件可以直接用？

**A:** 根目录下已有以下 `.tmp` 文件：

```
uno_0_8_1000MB.tmp     # 0% intra + 8 inter-DC 流，1000MB
uno_4_4_1000MB.tmp     # 4 intra + 4 inter-DC 流，1000MB
uno_8_0_1000MB.tmp     # 8 intra + 0 inter-DC 流，1000MB
uno_custom_256_5mb.tmp # 自定义 256 流，5MB
```

这三个文件覆盖了三种流量场景：
- `0_8`：纯 inter-DC（大 RTT 流为主）
- `4_4`：混合流量（你的核心实验场景）
- `8_0`：纯 intra-DC（小 RTT 流为主）

用 `-tm` 参数指定，例如 `-tm uno_4_4_1000MB.tmp`。

---

## Q5: 如何运行三种流量场景的批量对比实验？

**A:** 用以下脚本一次跑完 Uno vs APPQ 在三种流量下的对比：

```bash
cd /home/ustc/APPQ_SC25
mkdir -p results

for TM in uno_0_8_1000MB.tmp uno_4_4_1000MB.tmp uno_8_0_1000MB.tmp; do
    BASE=$(basename $TM .tmp)

    # 公共参数
    COMMON="sim/datacenter/htsim_lcp_entry_modern \
        -o uec_entry -seed 215 -queue_type composite \
        -hop_latency 1000 -switch_latency 0 -nodes 128 \
        -collect_data 0 -topology interdc -os_border 16 \
        -strat rand -linkspeed 100000 \
        -topo lcp/configs/topos/fat_tree_100Gbps.topo \
        -tm $TM -noRto -queueSizeRatio 1 \
        -IntraFiT 100 -InterFiT 2500 \
        -interKmax 60 -interKmin 20 -ecnAlpha 0.65 \
        -usePacing 1 -end_time 2860 -lcpK 6 -interEcn \
        -mdRTT 0.0003515625 -interdcDelay 886500 \
        -kmin 10 -kmax 80 -lcpAlgo aimd_phantom \
        -use_phantom 1 -phantom_size 22400515 \
        -phantom_slowdown 5 -phantom_kmin 2 -phantom_kmax 60 \
        -forceQueueSize 1000000 -noFi"

    echo "Running Uno: $BASE"
    $COMMON > results/uno_${BASE}.tmp

    echo "Running APPQ: $BASE"
    $COMMON -use_appq 1 > results/appq_${BASE}.tmp
done

echo "Done! Results in results/"
```

---

## Q6: 如何用已有的 fig9 Python 脚本跑实验？

**A:**

```bash
cd /home/ustc/APPQ_SC25
python3 artifact_scripts/fig9.py
```

这会运行 Uno、Gemini、BBR 的对比。要加入 APPQ，需修改 `artifact_scripts/fig9.py`，在 `run_benchmark()` 函数里添加 `appq` variant（在 `uno` variant 基础上加 `-use_appq 1`），然后在绘图部分加入 APPQ 曲线。

---

## Q7: 实验结果输出格式是什么？如何解析 FCT？

**A:** 输出文件（`.tmp`）每行格式为：

```
Flow <flow_id> <size_bytes> <fct_us> <src> <dst> ...
```

用以下命令快速查看 P50/P99 FCT：

```bash
# 提取所有流的 FCT（第3列），计算分位数
awk '{print $3}' results/appq_uno_4_4_1000MB.tmp | sort -n | \
    awk 'BEGIN{lines=0} {data[lines]=$1; lines++} \
    END{print "P50:", data[int(lines*0.50)], \
        "P99:", data[int(lines*0.99)]}'
```

或者使用已有的 `parse_output` 工具：
```bash
sim/parse_output results/appq_uno_4_4_1000MB.tmp
```

---

## Q8: APPQ 的关键参数如何调整？

**A:** APPQ 的核心参数在 `compositequeue.cpp` 的 `APPQState` 结构体中：

| 参数 | 默认值 | 含义 | 调大效果 |
|------|--------|------|----------|
| `ALPHA` | 0.2 | EWMA 平滑系数 | 响应更快但更抖动 |
| `VEL_THRESH_HIGH` | 1000.0 | 速度高阈值（bytes/ps） | 更难触发激进保护 |
| `VEL_THRESH_LOW` | -500.0 | 速度低阈值 | 更难触发缩减 |
| `ACC_THRESH_HIGH` | 50.0 | 加速度高阈值 | 更难触发最激进保护 |

趋势等级与参数调整对应关系：

| 趋势等级 | 触发条件 | phantom_size_factor | kmin 调整 | kmax 调整 |
|----------|----------|--------------------|-----------|-----------|
| 4（加速恶化） | acc > 50 | 1.5× | -5 | -10 |
| 3（稳定上升） | vel > 1000 | 1.25× | -3 | -5 |
| 2（轻微上升） | vel > 0 | 1.0× | 0 | 0 |
| 1（稳定/下降） | vel > -500 | 0.9× | +2 | +3 |
| 0（快速下降） | vel ≤ -500 | 0.85× | +5 | +5 |

---

## Q9: 如何验证 APPQ 的自适应机制确实在工作？

**A:** 开启 `collect_data 1`，查看幽灵队列日志：

```bash
sim/datacenter/htsim_lcp_entry_modern \
    ... \
    -collect_data 1 \
    -use_phantom 1 \
    -use_appq 1 \
    ...
```

日志文件在 `output/queue_phantom/` 目录，格式为：
```
时间(us), 幽灵队列当前大小, ecn_minthresh, ecn_maxthresh, 幽灵队列上限
```

用 `paper_script/plot_queues.py` 绘图，观察幽灵队列上限是否随拥塞程度动态变化。

---

## Q10: 推荐的第一个实验是什么？

**A:** 推荐从最简单的单场景对比开始：

```bash
cd /home/ustc/APPQ_SC25
mkdir -p results

# 1. Uno 基线（混合流量场景）
sim/datacenter/htsim_lcp_entry_modern \
    -o uec_entry -seed 215 -queue_type composite \
    -hop_latency 1000 -switch_latency 0 -nodes 128 \
    -collect_data 0 -topology interdc -os_border 16 \
    -strat rand -linkspeed 100000 \
    -topo lcp/configs/topos/fat_tree_100Gbps.topo \
    -tm uno_4_4_1000MB.tmp -noRto -queueSizeRatio 1 \
    -IntraFiT 100 -InterFiT 2500 \
    -interKmax 60 -interKmin 20 -ecnAlpha 0.65 \
    -usePacing 1 -end_time 2860 -lcpK 6 -interEcn \
    -mdRTT 0.0003515625 -interdcDelay 886500 \
    -kmin 10 -kmax 80 -lcpAlgo aimd_phantom \
    -use_phantom 1 -phantom_size 22400515 \
    -phantom_slowdown 5 -phantom_kmin 2 -phantom_kmax 60 \
    -forceQueueSize 1000000 -noFi \
    > results/uno_4_4.tmp

# 2. APPQ（加一个参数）
sim/datacenter/htsim_lcp_entry_modern \
    -o uec_entry -seed 215 -queue_type composite \
    -hop_latency 1000 -switch_latency 0 -nodes 128 \
    -collect_data 0 -topology interdc -os_border 16 \
    -strat rand -linkspeed 100000 \
    -topo lcp/configs/topos/fat_tree_100Gbps.topo \
    -tm uno_4_4_1000MB.tmp -noRto -queueSizeRatio 1 \
    -IntraFiT 100 -InterFiT 2500 \
    -interKmax 60 -interKmin 20 -ecnAlpha 0.65 \
    -usePacing 1 -end_time 2860 -lcpK 6 -interEcn \
    -mdRTT 0.0003515625 -interdcDelay 886500 \
    -kmin 10 -kmax 80 -lcpAlgo aimd_phantom \
    -use_phantom 1 -use_appq 1 -phantom_size 22400515 \
    -phantom_slowdown 5 -phantom_kmin 2 -phantom_kmax 60 \
    -forceQueueSize 1000000 -noFi \
    > results/appq_4_4.tmp

# 3. 对比结果
echo "=== Uno ==="
sim/parse_output results/uno_4_4.tmp 2>/dev/null || head -5 results/uno_4_4.tmp
echo "=== APPQ ==="
sim/parse_output results/appq_4_4.tmp 2>/dev/null || head -5 results/appq_4_4.tmp
```

这个实验约运行 1-5 分钟，完成后可以直接比较 FCT 数值。

cd /home/ustc/APPQ_SC25 && mkdir -p results

# Uno 基线 放后台
nohup sim/datacenter/htsim_lcp_entry_modern \
    -o uec_entry -seed 215 -queue_type composite \
    -hop_latency 1000 -switch_latency 0 -nodes 128 \
    -collect_data 0 -topology interdc -os_border 16 \
    -strat rand -linkspeed 100000 \
    -topo lcp/configs/topos/fat_tree_100Gbps.topo \
    -tm uno_4_4_1000MB.tmp -noRto -queueSizeRatio 1 \
    -IntraFiT 100 -InterFiT 2500 \
    -interKmax 60 -interKmin 20 -ecnAlpha 0.65 \
    -usePacing 1 -end_time 2860 -lcpK 6 -interEcn \
    -mdRTT 0.0003515625 -interdcDelay 886500 \
    -kmin 10 -kmax 80 -lcpAlgo aimd_phantom \
    -use_phantom 1 -phantom_size 22400515 \
    -phantom_slowdown 5 -phantom_kmin 2 -phantom_kmax 60 \
    -forceQueueSize 1000000 -noFi \
    > results/uno_4_4.tmp 2>&1 &
echo "Uno PID: $!"

nohup sim/datacenter/htsim_lcp_entry_modern -o uec_entry -seed 215 -queue_type composite -hop_latency 1000 -switch_latency 0 -nodes 128 -collect_data 0 -topology interdc -os_border 16 -strat rand -linkspeed 100000 -topo lcp/configs/topos/fat_tree_100Gbps.topo -tm uno_4_4_1000MB.tmp -noRto -queueSizeRatio 1 -IntraFiT 100 -InterFiT 2500 -interKmax 60 -interKmin 20 -ecnAlpha 0.65 -usePacing 1 -end_time 2860 -lcpK 6 -interEcn -mdRTT 0.0003515625 -interdcDelay 886500 -kmin 10 -kmax 80 -lcpAlgo aimd_phantom -use_phantom 1 -phantom_size 22400515 -phantom_slowdown 5 -phantom_kmin 2 -phantom_kmax 60 -forceQueueSize 1000000 -noFi > results/uno_4_4.tmp 2>&1 & echo "Uno PID: $!"
# APPQ 放后台
nohup sim/datacenter/htsim_lcp_entry_modern \
    -o uec_entry -seed 215 -queue_type composite \
    -hop_latency 1000 -switch_latency 0 -nodes 128 \
    -collect_data 0 -topology interdc -os_border 16 \
    -strat rand -linkspeed 100000 \
    -topo lcp/configs/topos/fat_tree_100Gbps.topo \
    -tm uno_4_4_1000MB.tmp -noRto -queueSizeRatio 1 \
    -IntraFiT 100 -InterFiT 2500 \
    -interKmax 60 -interKmin 20 -ecnAlpha 0.65 \
    -usePacing 1 -end_time 2860 -lcpK 6 -interEcn \
    -mdRTT 0.0003515625 -interdcDelay 886500 \
    -kmin 10 -kmax 80 -lcpAlgo aimd_phantom \
    -use_phantom 1 -use_appq 1 -phantom_size 22400515 \
    -phantom_slowdown 5 -phantom_kmin 2 -phantom_kmax 60 \
    -forceQueueSize 1000000 -noFi \
    > results/appq_4_4.tmp 2>&1 &
echo "APPQ PID: $!"
