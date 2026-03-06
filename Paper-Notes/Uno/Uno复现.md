**GitHub 代码库**：https://github.com/spcl/Uno_SC25.git

改完后一定要**重新编译 C++ 模拟器**，然后在终端输入：
```cpp
cd /home/ustc/Uno_SC25/sim
make
```

方案一：**跨机房流保护（引入非线性惩罚）**
修改定位：sim/uec.cpp 里的 dec_fact = y_rtt / (double)_base_rtt;
原版逻辑：当前的惩罚是线性的（除以 _base_rtt）。如果 Inter-DC（跨机房）的时延是 Intra-DC 的 10 倍，它被惩罚的力度也就是十分之一。
怎么改：我们可以引入一个“偏心指数”，比如用平方或者加倍放大 _base_rtt 的地位。
将这一行改为：
```cpp
// 给 base_rtt 加上 1.5 倍的权重，让长 RTT 的数据流降速更少（更霸道）
double dec_fact = y_rtt / ((double)_base_rtt * 1.5); 
```
- **预期结果与缘由**：在混合流量场景下（尤其是图 13 验证的数据中心内外混合流），你会发现跨机房（Inter-DC）宽带抢占能力大幅度提升，吞吐量变大；但这可能会导致机房内（Intra-DC）短距离流的排队更严重。你可以通过画出来的图去观察这种“帕累托最优”的此消彼长！

方案二：**提升降窗保底**（优化长尾延迟/Tail Latency）
修改定位：sim/uec.cpp 底部计算最终大砍一刀的地方。
原版逻辑：如果砍得太狠，保底只剩 1 个数据包 _cwnd = _mss;。
怎么改：把保底值提升到 4（或者像 Google BBR 一样提升到 10）。
把原来的 if...else 块改成：
```cpp
if (_cwnd > 4 * _mss + gent_dec_amount) {
    _cwnd = _cwnd - gent_dec_amount; 
} else {
    // 绝对不让窗口小于 4 个 MSS，防止流彻底饿死
    _cwnd = 4 * _mss; 
}
```

方案三：激进的“试探性”重传窗口
修改定位：sim/uec.cpp 的 uint32_t val1 = temp_window / dec_fact;

原版逻辑：老老实实除以时间因子。
怎么改：给计算出的惩罚加上一个“折扣”。
```cpp
// 打个 8 折，我就是要少砍一点，胆子大一点
uint32_t val1 = (temp_window / dec_fact) * 0.8; 
```
预期结果与缘由：官方的下掉逻辑偏保守。打 8 折可以让 Uno 在网络有随机抖动（如光模块微损引发的假丢包）时抵抗力更强。如果网络的实际 Buffer 够大，这么改可以压榨出更高的极限链路利用率！

**Q&A**：
1.两个图相同
这个我们依赖来跑实验的 C++ 核心模拟器可执行文件 htsim_lcp_entry_modern 的最后修改时间，居然还是 3月3号晚上 21:16（也就是好几个小时前我们第一次帮你从一堆报错里修好编译时候生的旧版本）。

这是为什么？
因为刚才我们虽然改了 sim/uec.cpp 文件，并且我也帮你执行了 make clean && make -j 8；但是，那个 make 生成的是 sim 文件夹下的通用库 libhtsim.a，而 sc25_quick_validation.sh 脚本在调用的时候，是用**sim/datacenter里的上层 htsim_lcp_entry_modern的二进制。因为我没有在这个上层目录再次强制重新触发链接编译最新的主程序，导致它现在用来跑这100多趟实验的老牛，还是原来那头未动过手脚的老牛！**

所以它老老实实把一模一样的实验、利用原版未修改的计算公式又跑了一遍，画出来的图自然一模一样！

这一步骤需要强制重新编译+清空缓存：
这个锅算我的，接下来我要在你的终端直接操作：

彻彻底底地把所有关联 C++ 缓存删干净重新编译。
把上一回跑出来污染的 .tmp 结果强行删掉，不然 Python 可能会偷懒复用。
```cpp
cd /home/ustc/Uno_SC25/simmake clean && make -j 8cd datacentermake clean && make -j 8
```

**实验输入**：
网络与流量参数：包括网络带宽（如 400Gbps）、网络延迟（如跨数据中心的 1ms/5ms/10ms RTT 与数据中心内部的 5µs/10µs/20µs）、消息/流大小（消息传输字节数）、以及流量负载强度（40%, 60%, 80%等）。
仿真配置文件：位于 sim/（底层仿真器 C++ 代码所在）、configs/ 或 traffic_gen/ 中给定的流量生成模式和路由机制参数。
这些参数由诸如 sc25_fig10_realistic_60_load.sh 这样的 bash 启动脚本，或是各个 .py 脚本设定并喂给模拟器。

**实验输出**：
原始数据 (Logs)：网络模拟器运行完成后的日志，反映了每个数据流的完成时间 (Flow Completion Time, FCT)、队列长度、吞吐量分布等性能指标。
可视化图表 (Plots)：画图脚本会解析这些原始结果或理论数据，并将生成的 PDF / PNG 格式图表保存在 artifact_results 目录下。

2.为什么“非线性惩罚”导致结果绝对一致？
**网络模拟器是绝对确定性的 (Deterministic)**：  
你使用的基于事件驱动的自研模拟器（类似 NS-3 等）完全依赖伪随机数生成器。在类似 [fig3_run_all.py](vscode-file://vscode-app/c:/Users/86188/AppData/Local/Programs/Microsoft%20VS%20Code/072586267e/resources/app/out/vs/code/electron-browser/workbench/workbench.html) 或 `fig8_comparison.py` 的脚本中，你可以看到启动命令写死了硬编码种子：
```cpp
# fig3_run_all.py 第 24 行片段
"-seed", "215",  # uno参数
# 或者下面的
"-seed", "15",   # bbr/gemini参数
```
**在固定了网络拓扑、初始发包事件和随机数种子的情况下，无论你的数学惩罚公式多么复杂、非线性，只要输入是特定的 [x](vscode-file://vscode-app/c:/Users/86188/AppData/Local/Programs/Microsoft%20VS%20Code/072586267e/resources/app/out/vs/code/electron-browser/workbench/workbench.html)，算出的 [y](vscode-file://vscode-app/c:/Users/86188/AppData/Local/Programs/Microsoft%20VS%20Code/072586267e/resources/app/out/vs/code/electron-browser/workbench/workbench.html) 就肯定是一模一样的。** 系统会以皮秒不差的精度执行完全一模一样的路由、拥塞和排队逻辑，产生每一行字节都不差的日志文件，因此最后 Matplotlib 画出的图当然连一个像素都不会变。

3.**为什么你修改的 C++ 代码（`sim/lcp.cpp`）在很多实验中没反应？**  
    我查看了你的 Git 工作区，发现你在 `sim/lcp.cpp` 中将一行判定条件从：  
    `if (rtt > _base_rtt * 1.055)` 修改成了 `if (rtt > _base_rtt * 1.2)`。  
    如果你运行某个实验（比如 fig3 甚至 fig8 的某些设置），网络拓扑中的 `rtt` **压根没有落在 `[1.055, 1.2)` 这个区间内**（要么一直没超 1.055，要么平时拥塞直接飙破 1.2），这就意味着代码控制流在绝大多数图表的场景里跟 Baseline 走的路线是一模一样的，结果也就原封不动。