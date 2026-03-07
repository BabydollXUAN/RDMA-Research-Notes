Uno 最大的特色是**精细的反馈状态机+多状态切换**：

1.**处理ACK包时，先判断ACK的性质**
在处理新收到的ACK时，算法会先判断本次事件是正常的成功反馈，还是网络丢包发出的“报警”信号。
```cpp
void UecSrc::adjust_window(simtime_picosec ts, bool ecn, simtime_picosec rtt) {
	// 模拟器当前的绝对时间减去上次进行窗口大小调整的时间，相当于这段窗口内“实际经历的时间周期”
    bool time_enough = (eventlist().now() - t_last_decrease) > _base_rtt;

  
    // 先进入一个大 Switch 看当前的流处于什么状态（状态机）

    switch(current_state) {

        // ... (其他状态)
```
在状态机里，当检测到==**丢包、乱序、ECN标记、RTT指数级升高**==时，Uno从正常状态切入到降低cwnd或者拥塞探测相关状态。

2.**丢包时核心反应**
一旦判定遭遇不好的反馈，对丢包及其敏感。
```cpp
    case UEC_STATE_ERROR: 
        if (eventlist().now() > update_next_window) { 
            // 发生拥塞的核心惩罚机制
            
            t_last_decrease = eventlist().now(); 
            
            // 直接基于目标速率计算拥塞窗口减小基数
            // y_rtt 这段时间窗口内“实际经历的时间周期”
            // double y_rtt = eventlist().now() - update_next_window;
            // 惩罚平滑因子（创新参数），用来平衡机房内和跨机房的流量
            double dec_fact = y_rtt / (double)_base_rtt;
            // temp_window 保存了出发拥塞前你的正常飞行量
            // unint32_t gent_dec_amount=val1; 这个是预估退回值
            uint32_t val1 = temp_window / dec_fact;
            
            // 下方这段通过比较降窗，让窗口迅速收缩来给当前网络腾路
            // 避免雪崩导致跨数据中心 (Inter-DC) 的包排队丢死
             if (_cwnd > _mss + gent_dec_amount) {
                  _cwnd = _cwnd - gent_dec_amount; 
             } else {
                  _cwnd = _mss; 
             }
        }
```
在跨数据中心（Inter-DC）中，距离遥远，`_base_rtt` 很大；如果在同一个机房（Intra-DC），`_base_rtt` 很小。如果只用传统的乘性减小（比如直接把窗口除以 2），长距离流恢复起来比短距离流慢几百倍，有失公平性。
**作用**：通过把当前观测的时间比例除以你的“无拥塞耗时”，它会把对降窗的“扣减额度”，均平到每一个数据流自身的时延基准上。RTT越大的流（距离越远），这个因子作用后扣减的就相对保守，保证不会一夜回到解放前；RTT短的流立刻就能减掉速度，避免拥塞。

**不应该像传统算法那样“无脑减半”，而是按照你应有的带宽时间做比例精准剥夺系数精准削比例分摊降速。**

我已经帮您在后台开好了一个名为 batch_experiments 的 tmux 会话，并把所有的验证脚本（sc25_quick_validation.sh 涵盖的 图1、3、4、8、9 以及另外手动加上的图13）全部挂上去跑了。
tmux attach -t batch_experiments

我之前虽然帮您修改了 sim/lcp.cpp 中的 1.010 代码，但在增量编译时系统的 Makefile 机制没有把核心模块重新链接（它说 Nothing to be done）。我已经帮您执行了 make clean 并使用了最新代码进行重新全量编译。
比较文件时，后台的 C++ 仿真引擎其实仍在运行生成数据中（一个实验需要大约 2-3 分钟），Python 还没有执行画图操作去覆盖旧图片。因此当时看到的是两张相同的“旧图”。