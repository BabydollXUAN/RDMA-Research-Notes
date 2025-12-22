### Uno 系统处理 Pipeline

这个Pipeline展示了一个消息（Message）从产生到完成传输的全过程。

---

#### 第一阶段：发送侧处理与编码 (Sender Side - UnoRC)

这一阶段主要由 **UnoRC (Reliable Connectivity)** 组件负责，目标是解决Inter-DC链路的高丢包和故障问题。

1. **流量分类与分块 (Segmentation & Coding)**
    
    - **输入：** 应用层产生的消息。
        
    - **判断：** 如果是Inter-DC流量（跨数据中心），启用纠删码（Erasure Coding）。
        
    - **操作：** 将消息分割为多个Block。每个Block包含 $n$ 个包，其中包括 $x$ 个数据包和 $y$ 个校验包（Parity Packets）。
        
    - _逻辑：_ 只要接收端收到任意 $x$ 个包即可还原数据，无需重传。
        
2. **子流负载均衡 (UnoLB - Subflow Assignment)**
    
    - **分配：** 将同一个Block内的 $n$ 个数据包分散分配到多个子流（Subflows）上 。
        
    - **路由：** 每个子流通过改变包头（如源端口或IPv6 Flow Label）来利用ECMP哈希到不同的物理路径上 。
        
    - **故障规避：** 如果收到NACK或发生超时（Timeout），UnoRC会立即将该子流“重路由”（Re-route），从已知健康的子流池中随机选择一个新的路径，避开故障链路 。
        

---

#### 第二阶段：网内传输与拥塞标记 (Network Side - Switch)

这一阶段涉及交换机内部的处理，核心是 **Phantom Queue（幽灵队列）** 机制。

3. **双队列维护 (Queue Management)**
    
    - **入队：** 数据包到达交换机，进入**物理队列**等待转发。
        
    - **幽灵计数：** 同时，交换机在逻辑上维护一个虚拟的**幽灵队列（Phantom Queue）**计数器。物理队列每进一个包，幽灵队列计数器也增加 。
        
4. **差异化排空 (Differential Draining)**
	    
    - **物理队列：** 以链路的全线速（Line Rate, e.g., 100Gbps）排空。
        
    - **幽灵队列：** 以略低于线速的速率（例如 90% Line Rate）进行逻辑排空。
        
    - _结果：_ 幽灵队列总是比物理队列更早“填满”，从而在物理队列由于拥塞导致延迟增加之前，就提前感知到带宽压力。
        
5. **ECN 标记 (Marking Logic)**
    
    - **判断：** 检查物理队列 **或** 幽灵队列的占用率是否超过设定的阈值（Min/Max Threshold）。
        
    - **操作：** 如果任意一个队列超过阈值，则在数据包的IP头中标记 ECN（Congestion Experienced）位 。
        

---

#### 第三阶段：接收与反馈 (Receiver Side)

6. **解码与重组 (Reconstruction)**
    
    - **计时：** 收到Block的第一个包时启动计时器。
        
    - **还原：** 如果在计时器内收到 $\ge x$ 个包，直接使用MDS算法还原原始数据块 。
        
    - **异常处理：** 如果超时或包数量不足，发送 **NACK** 给发送端请求重传 。
        
7. **反馈生成 (Feedback)**
    
    - **ACK发送：** 向发送端发送ACK确认包，ACK中携带了收到的ECN标记信息（ECN-Echo）。
        

---

#### 第四阶段：拥塞控制与速率调整 (Sender Side - UnoCC)

发送端收到ACK后，**UnoCC (Congestion Control)** 组件根据网络状态调整发送窗口（$cwnd$）。这是Uno保证公平性的核心。

8. 状态判断与窗口调整 (State Machine)
    
    UnoCC根据反馈将网络判定为三种状态之一，并执行相应操作：
    
    - **状态 A：无拥塞 (Uncongested)**
        
        - _信号：_ ACK中无ECN标记。
            
        - _操作 (AI)：_ 执行加法增（Additive Increase）。
            
        - _公式：_ $cwnd = cwnd + \alpha$ 。
            
    - **状态 B：普通拥塞 (Congested)**
        
        - _信号：_ ACK中有ECN标记。
            
        - _操作 (MD)：_ 每个Epoch（周期）执行一次乘性减（Multiplicative Decrease）。
            
        - _区分真假拥塞：_ UnoCC检查数据包的往返延迟（Delay）：
            
            - **情形1（幽灵拥塞）：** 有ECN标记，但延迟极低（接近基准RTT）。说明是幽灵队列满了，物理队列还是空的。
                
                - _动作：_ **温和减速**。将减小因子乘以0.3（$MD_{scale} \times 0.3$）。
                    
            - **情形2（物理拥塞）：** 有ECN标记，且延迟较高。
                
                - _动作：_ **全额减速**。执行标准的MD操作 。
                    
    - **状态 C：极端拥塞 (Extremely Congested)**
        
        - _信号：_ 确认的字节数（Bytes Acked）突然断崖式下跌（Incast场景）。
            
        - _检测：_ 每RTT检查一次，如果 `bytes_acked < cwnd * beta`。
            
        - _操作 (Quick Adapt - QA)：_ **急刹车**。不等待MD，直接将 $cwnd$ 重置为当前实际的 `bytes_acked` 值 。
            

---

### Pipeline 逻辑图示

1. **[UnoRC] 发送端：** 切片 -> 加纠删码 -> 散列到多路径
    
2. **[Switch] 交换机：** 物理入队 + 幽灵计数 -> (幽灵满 ? 标记ECN) -> 物理转发
    
3. **[Receiver] 接收端：** 凑齐碎片 -> 还原数据 -> 回复ACK (带ECN)
    
4. **[UnoCC] 发送端 (收ACK)：**
    
    - 无标记 -> **加窗 (+)**
        
    - 有标记 + 低延迟 -> **微量减窗 (-)** (幽灵队列起作用)
        
    - 有标记 + 高延迟 -> **大幅减窗 (--)** (物理队列堵了)
        
    - 吞吐骤降 -> **重置窗口 (Reset)** (Quick Adapt触发)