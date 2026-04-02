改成 `tmux` / `nohup` 这种方式，下次就能关编辑器了
## 下一步最正确的动作

如果你是要“开始跑实验看 APPQ 效果”，现在正确顺序不是直接跑图，而是：

1. **先把 `htsim_lcp_entry_modern` 接上 [-use_appq](vscode-file://vscode-app/c:/Users/86188/AppData/Local/Programs/Microsoft%20VS%20Code/072586267e/resources/app/out/vs/code/electron-browser/workbench/workbench.html)**
2. 重新编译
3. 先跑一个单场景对比，比如 `4_4_1000MB`
4. 确认 `Uno` 和 `APPQ` 的 FCT 真有差异
5. 再去改 `fig9` / `fig10` / `fig11` / `fig12` 脚本

我做了什么
我在 main_lcp_entry_modern.cpp:614-622 增加了 -use_appq 参数解析，逻辑和 uec 入口一致：

读取 -use_appq <0|1>
打印 UseAPPQ: ...
调用 CompositeQueue::set_use_appq(...)
然后我重新编译了 htsim_lcp_entry_modern，编译成功。

我验证过了
我又跑了一次带 -use_appq 1 的短时 smoke test，已经不再报 Unknown option -use_appq，而且仿真正常进入事件循环并退出，结果在：

appq_lcp_smoke_after_patch.tmp
这说明：

htsim_lcp_entry_modern 现在已经支持 -use_appq
APPQ 已经能走进 lcp 主实验入口了
现在意味着什么
现在你这套主实验脚本终于具备了“接入 APPQ”的前提。
也就是说，后面像 fig3、fig8、fig9、fig10、fig11、fig12、fig13 这类依赖 lcp 的实验，已经可以改成真正跑 APPQ。

但还要注意两点：

脚本本身还没自动加 -use_appq 1
有些绘图脚本还在用静态数据
所以现在是“入口通了”，不是“整套图已经自动切换完”。

下一步建议
我建议我继续直接帮你做下面两件事中的一个：

方案 A： 先改一个最关键的对比实验，比如 fig9，加上 APPQ vs Uno
方案 B： 先跑一个 4_4_1000MB 的 Uno / APPQ 小实验，直接看数值有没有变化
如果你要最快看到效果，我建议先做 方案 B。

### 实验B：跑一个 4_4_1000MB 的 Uno / APPQ 小实验，直接看数值有没有变化

试了，`方案 B` 已经跑完了。

##### 我做了什么

我先把 `htsim_lcp_entry_modern` 接上了 [-use_appq](vscode-file://vscode-app/c:/Users/86188/AppData/Local/Programs/Microsoft%20VS%20Code/072586267e/resources/app/out/vs/code/electron-browser/workbench/workbench.html)，然后在同一个 `4_4_1000MB` 场景下各跑了一次：

- `Uno` 基线
- `APPQ` 对照

输出文件在：

- [uno_4_4_b.txt](vscode-file://vscode-app/c:/Users/86188/AppData/Local/Programs/Microsoft%20VS%20Code/072586267e/resources/app/out/vs/code/electron-browser/workbench/workbench.html)
- [appq_4_4_b.txt](vscode-file://vscode-app/c:/Users/86188/AppData/Local/Programs/Microsoft%20VS%20Code/072586267e/resources/app/out/vs/code/electron-browser/workbench/workbench.html)

并且我确认 `APPQ` 这一轮确实启用了开关，见 [appq_4_4_b.txt:24-30](vscode-file://vscode-app/c:/Users/86188/AppData/Local/Programs/Microsoft%20VS%20Code/072586267e/resources/app/out/vs/code/electron-browser/workbench/workbench.html)。

#### 直接结果

##### Uno

8 条流的完成时间在这里：

- [uno_4_4_b.txt:875-882](vscode-file://vscode-app/c:/Users/86188/AppData/Local/Programs/Microsoft%20VS%20Code/072586267e/resources/app/out/vs/code/electron-browser/workbench/workbench.html)

##### APPQ

8 条流的完成时间在这里：

- [appq_4_4_b.txt:876-883](vscode-file://vscode-app/c:/Users/86188/AppData/Local/Programs/Microsoft%20VS%20Code/072586267e/resources/app/out/vs/code/electron-browser/workbench/workbench.html)
- **继续跑更敏感的场景**
    
    - 比如 `8_0_1000MB`、`0_8_1000MB`
    - 或不同 `os_border`
    - 或 [collect_data 1](vscode-file://vscode-app/c:/Users/86188/AppData/Local/Programs/Microsoft%20VS%20Code/072586267e/resources/app/out/vs/code/electron-browser/workbench/workbench.html) 看 phantom queue 是否真的动态变化
- **直接改一个图脚本**
    
    - 我建议先改 `fig9`
    - 把 `APPQ` 加成新的 variant
    - 这样最直观看到图是否变化

如果你愿意，我下一步直接帮你：  
**把 [fig9.py](vscode-file://vscode-app/c:/Users/86188/AppData/Local/Programs/Microsoft%20VS%20Code/072586267e/resources/app/out/vs/code/electron-browser/workbench/workbench.html) 改成同时画 `Uno` 和 `APPQ`**

##### 我算出来的对比

基于这 8 条流的完成时间：

- `All Avg FCT`
    
    - Uno: `565887.5 us`
    - APPQ: `565546.0 us`
    - 变化：`-0.06%`
- `P99 / 最大值近似`
    
    - Uno: `687670 us`
    - APPQ: `684725 us`
    - 变化：`-0.43%`
- `Inter-DC Avg FCT`
    
    - Uno: `449446.75 us`
    - APPQ: `448200.5 us`
    - 变化：`-0.28%`
- `Intra-DC Avg FCT`
    
    - Uno: `682328.25 us`
    - APPQ: `682891.5 us`
    - 变化：`+0.08%`

## 结论

结论很明确：

1. **图的数值会变**  
    因为同一场景下，`Uno` 和 `APPQ` 的完成时间已经不一样了。
    
2. **但这次变化不大**  
    这一组 `4_4_1000MB` 实验里，APPQ 只带来了非常小的改善，属于：
    
    - 整体平均值略好
    - 尾延迟略好
    - inter-flow 略好
    - intra-flow 基本持平甚至略差
3. **所以“图会不会明显变化”要分开说**
    
    - **数值上：会变**
    - **视觉上：未必很明显**
    - 如果柱状图精度不高，可能肉眼几乎看不出来
    - 如果画差值图或把 APPQ 单独加成一条新柱子/新曲线，就能体现出来

额外观察
这次运行时间也差不多：

Uno: 约 4:06
APPQ: 约 4:12
说明现在流程已经能正常跑对照实验了。

我建议下一步
现在最值得做的是二选一：

继续跑更敏感的场景

比如 8_0_1000MB、0_8_1000MB
或不同 os_border
或 collect_data 1 看 phantom queue 是否真的动态变化
直接改一个图脚本

我建议先改 fig9
把 APPQ 加成新的 variant
这样最直观看到图是否变化