**htsim** 是一个**高性能网络模拟器**（High-Throughput Simulator），用于模拟数据中心网络中的各种传输协议和拓扑结构。它是 Uno 项目的核心仿真工具。
## 主要特点

1. **多协议支持**：支持多种传输协议的模拟
    
    - TCP/DCTCP
    - NDP (Near-optimal Datacenter Protocol)
    - RoCE (RDMA over Converged Ethernet)
    - Swift
    - HPCC
    - BBR
    - UEC (Ultra-low latency Ethernet)
    - LCP (Uno 项目的核心协议)
2. **多种可执行文件**：针对不同协议有不同的编译版本
    
    - [htsim_uec_entry_modern](vscode-webview://15at561hd3jsksql3ihfm6uie39oh906d1lrqph9e01vvgcdruun/Uno_SC25/sim/datacenter/htsim_uec_entry_modern)
    - [htsim_lcp_entry_modern](vscode-webview://15at561hd3jsksql3ihfm6uie39oh906d1lrqph9e01vvgcdruun/Uno_SC25/sim/datacenter/htsim_lcp_entry_modern)
    - [htsim_ndp_entry_modern](vscode-webview://15at561hd3jsksql3ihfm6uie39oh906d1lrqph9e01vvgcdruun/Uno_SC25/sim/datacenter/htsim_ndp_entry_modern)
    - [htsim_swift_entry](vscode-webview://15at561hd3jsksql3ihfm6uie39oh906d1lrqph9e01vvgcdruun/Uno_SC25/sim/datacenter/htsim_swift_entry)

## 如何使用 htsim

### 1. 编译
```bash
cd /home/ustc/Uno_SC25/sim
make clean && cd datacenter/ && make clean && cd .. && make -j 8 && cd datacenter/ && make -j 8 && cd ..
```
### 2. 运行示例

htsim 通过命令行参数配置仿真，典型的调用方式：
```bash
./sim/datacenter/htsim_lcp_entry_modern \
  -o uec_entry \
  -seed 215 \
  -queue_type composite \
  -hop_latency 1000 \
  -nodes 128 \
  -topology interdc \
  -linkspeed 100000 \
  -topo lcp/configs/topos/fat_tree_100Gbps.topo \
  -tm <traffic_matrix_file> \
  -end_time 2860 \
  -logging-folder <output_folder>

```
### 3. 关键参数说明
- `-o`: 输出模式
- `-seed`: 随机种子
- `-nodes`: 节点数量
- `-topology`: 拓扑类型（如 interdc 表示跨数据中心）
- `-linkspeed`: 链路速度（Mbps）
- `-topo`: 拓扑配置文件
- `-tm`: 流量矩阵文件
- `-end_time`: 仿真结束时间（微秒）
- `-logging-folder`: 日志输出目录


