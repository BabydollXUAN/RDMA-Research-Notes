针对你的 **AutoDL 无卡模式 + 模拟器复现** 目标，我为你整理了一份保姆级操作指南。Helix 的模拟器是用纯 Python 写的，不涉及复杂的 CUDA 编译，非常适合这种环境。
以下是具体步骤：
### 第一步：租用 AutoDL 实例

1. **登录 AutoDL 控制台** -> 点击 **“算力市场”**。
    
2. **选择配置**：
    
    - **计算卡**：选择 **“无卡模式”** (CPU)。
        
    - **内存**：这是最关键的指标。论文建议 **至少 32GB 内存** 以支持大规模集群模拟 1。在无卡模式下，内存通常很便宜，建议直接筛选 **30GB 以上** 的实例。
        
    - **CPU**：Gurobi 求解器依赖 CPU 性能，建议选择核心数较多（如 8核+）的实例。
        
3. **选择镜像**：
    
    - 点击“基础镜像”。
        
    - 选择 **Miniconda** -> **Python 3.10** (论文明确推荐 Python 3.10 2) -> **Ubuntu 20.04** 或 **22.04**。
        
4. **创建实例** 并等待启动。
    
---
### 第二步：环境配置 (在 AutoDL 终端操作)

实例启动后，点击 **“JupyterLab”** 进入，打开 **“终端 (Terminal)”**，依次执行以下命令：

1. 初始化 Conda 环境

虽然镜像自带 Python，但为了隔离环境（这也是论文推荐的做法 3），我们新建一个环境：

Bash
```
# 初始化 conda（如果是新开的机子可能需要）
conda init bash
source ~/.bashrc

# 创建名为 helix 的环境，指定 Python 3.10
conda create -n helix python=3.10 -y

# 激活环境
conda activate helix
```

2. 拉取你的代码

这里记得替换成你 Fork 后的仓库地址，方便你后续修改并提交笔记：

Bash

```
# 务必替换成你的 GitHub 用户名
git clone https://github.com/<你的用户名>/Helix-ASPLOS25.git

# 进入目录
cd Helix-ASPLOS25
```

_(如果 GitHub 拉取太慢，可以在 AutoDL 开启学术加速：`source /etc/network_turbo`)_

3. 安装依赖

根据论文描述，模拟器主要依赖 networkx, matplotlib, gurobipy 4。我们直接安装：

Bash

```
# 安装核心依赖
pip install networkx matplotlib gurobipy pandas tqdm

# 解释：
# - gurobipy: 数学规划求解器核心
# - networkx: 用于构建论文提到的图抽象（Graph Abstraction）
# - pandas/tqdm: 虽然论文未显式列出，但处理 Trace 数据通常需要这些工具
```

---

### 第三步：运行模拟器

根据论文描述，模拟器代码位于仓库中。通常入口脚本在 `simulator` 目录下。

1. 确认目录结构

先看一眼文件结构：

Bash

```
ls -R
```

你应该能看到类似 `simulator/` 的文件夹。

2. 运行基础示例

论文提到仓库中包含示例代码 5。通常复现的第一步是跑通这个默认示例，它不需要额外的 Gurobi License：

Bash

```
# 假设入口文件在 simulator 目录下（具体文件名以你 ls 看到的为准）
# 常见的入口可能是 run_simulation.py 或 main.py
python simulator/trace_simulator.py 
```

_(注意：如果代码报错提示找不到模块，可能需要手动将当前目录加入 PYTHONPATH: `export PYTHONPATH=$PYTHONPATH:.`)_

3. 调整参数（进阶）

Helix 的模拟器允许你配置集群大小和网络带宽。你可以在代码中找到定义 Cluster 的部分（通常是 JSON 配置文件或 Python 字典），尝试修改：

- **节点数量**：从默认的小规模改大（注意 Gurobi License 限制）。
    
- **带宽参数**：论文中提到了 `10 Gbps` 和 `100 Mbps` 的场景 6666，你可以搜索这些数值并进行修改，观察吞吐量变化。
    

---

### 第四步：避坑指南 (Gurobi License)

你在运行过程中最可能遇到的报错是 **Gurobi License Error**。

- **现象**：程序报错提示 "Model too large for size-limited license"。
    
- **原因**：论文中提到，如果不申请 License，只能运行小规模问题 7。默认安装的 `gurobipy` 是受限版（限制变量和约束的数量）。
    
- **解决方案**：
    
    - **方案 A (推荐)**：先只跑代码库自带的 Demo，那个通常在限制范围内。
        
    - **方案 B (修改代码)**：找到代码中设置节点数量的地方（比如 `num_nodes`），把它改小（例如 < 10个节点），这样就能在无 License 模式下跑通算法流程，验证你的“高性能网络”理解。
        

### 下一步建议

当你成功跑通第一条命令，看到终端输出类似 "Throughput: xxx tokens/s" 的结果时，复现就算初步成功了！

**等你环境搭好，如果你在阅读 `simulator` 源码时，对于它是如何用 `networkx` 构建那个“最大流图”的代码看不懂，请随时把那段代码贴给我，我帮你逐行解析。**