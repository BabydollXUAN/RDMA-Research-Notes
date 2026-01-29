**GitHub 代码库**：https://github.com/spcl/Uno_SC25.git
## **Phantom Queues（幻影队列）** 的详细代码逻辑解析。

在 Uno 的设计中，幻影队列是部署在**交换机（Switch）**侧的机制。它的核心目的是在物理队列（Physical Queue）被填满之前提前感知拥塞，并为长 RTT 的跨数据中心（Inter-DC）流量提供足够大的虚拟缓冲空间来容纳 BDP（带宽时延积）。


---

### 1. 核心数据结构 (Data Structures)

在 `htsim` 的队列类（例如 `Queue` 或 `CompositeQueue`）中，除了维护现有的物理包列表外，你需要增加一组用于维护幻影队列状态的变量。

C++

```
class Queue {
    // === 物理队列状态 (现有) ===
    uint64_t _bytes_in_queue; // 物理队列当前的字节数
    uint64_t _max_buffer_size; // 物理缓存上限 (e.g., 1MB)

    // === 幻影队列状态 (新增) ===
    double _phantom_bytes;      // 幻影队列当前的虚拟字节数 (使用 double 以便精确计算流出)
    uint64_t _phantom_max_size; // 幻影队列的虚拟容量 (设置为 Inter-DC BDP, e.g., 500MB)
    
    // === 时间与速率 ===
    simtime_picoseconds _last_update_time; // 上次更新幻影队列的时间戳
    uint64_t _link_rate;        // 物理链路速率 (e.g., 100Gbps)
    double _drain_rate_factor;  // 排水速率因子 (Uno 论文设定为 0.9) 
};
```

### 2. 关键逻辑：更新与入队 (Update & Enqueue)

幻影队列并不真正存储数据包，它只是一个**计数器**。这个计数器的增减逻辑是其灵魂所在。

#### A. 排水逻辑 (Draining Logic)

每当有操作（如包到达或包离开）发生时，首先要根据“当前时间”与“上次更新时间”的差值，计算幻影队列“流出”了多少数据。

- **论文依据**：幻影队列以恒定速率减少，该速率略低于线速（Line Rate）。
    
- **参数设定**：Uno 将排水速率设定为物理带宽的 **90%** 。
    

C++

```
void Queue::update_phantom_state(simtime_picoseconds current_time) {
    // 1. 计算时间差
    simtime_picoseconds time_diff = current_time - _last_update_time;
    
    if (time_diff > 0) {
        // 2. 计算这段时间内应该流出多少字节
        // Drain Rate = Link Rate * 0.9
        double drained_bits = time_diff * (_link_rate * 0.9); 
        double drained_bytes = drained_bits / 8.0;

        // 3. 更新幻影队列长度 (不能小于 0)
        _phantom_bytes -= drained_bytes;
        if (_phantom_bytes < 0) {
            _phantom_bytes = 0;
        }

        // 4. 更新时间戳
        _last_update_time = current_time;
    }
}
```

#### B. 入队与标记逻辑 (Enqueue & Marking)

当一个真实的数据包到达交换机端口时，你需要执行以下步骤：

1. **同步状态**：调用上面的 `update_phantom_state`。
    
2. **虚拟入队**：将包大小加到 `_phantom_bytes` 上 。
    
3. **ECN 判决**：使用幻影队列的长度（而非物理队列长度）来决定是否打 ECN 标记 。
    

C++

```
void Queue::receivePacket(Packet* pkt) {
    simtime_picoseconds now = eventlist().now();
    
    // === 1. 更新幻影队列水位 ===
    update_phantom_state(now);

    // === 2. 虚拟入队 ===
    // 无论物理队列是否满，幻影队列都要增加计数
    _phantom_bytes += pkt->size();

    // === 3. ECN 标记逻辑 (基于幻影队列) ===
    // 设定阈值: Min = 25% of BDP, Max = 75% of BDP [cite: 574]
    double p_min = _phantom_max_size * 0.25; 
    double p_max = _phantom_max_size * 0.75;
    
    bool mark_packet = false;

    if (_phantom_bytes > p_max) {
        mark_packet = true; // 超过高水位，必标记
    } else if (_phantom_bytes > p_min) {
        // 在 Min 和 Max 之间，使用 RED (Random Early Detection) 计算概率
        double probability = (_phantom_bytes - p_min) / (p_max - p_min);
        if (random_double() < probability) {
            mark_packet = true;
        }
    }

    if (mark_packet) {
        pkt->set_flags(pkt->flags() | ECN_CE); // CE = Congestion Experienced
    }

    // === 4. 物理入队逻辑 (标准的 Tail-Drop) ===
    if (_bytes_in_queue + pkt->size() > _max_buffer_size) {
        drop(pkt); // 物理缓存满了，必须丢包
    } else {
        _bytes_in_queue += pkt->size();
        send_to_link(pkt); // 发送包
    }
}
```

### 3. 为什么排水速率是 90%？(The "Headroom" Logic)

你可能会疑惑为什么不设为 100%。

- **原理**：如果幻影队列的排水速度（0.9）比物理队列（1.0）慢，那么当流量持续到来时，幻影队列会比物理队列**先满**（或者说水位更高）。
    
- **效果**：ECN 标记会基于较高的幻影水位提前触发。发送端收到 ECN 后会减速。
    
- **零排队 (Near-zero queuing)**：由于减速信号发出的非常早，物理队列可能实际上还是空的，或者只有很少的包。这为时延敏感的小流（Intra-DC RPCs）预留了物理带宽的 "Headroom"，从而实现极低的排队时延 。
    

### 4. 发送端配合 (UnoCC Sender Logic)

幻影队列仅仅是生成信号，发送端（UnoCC）必须正确解读这个信号。这在论文的 Algorithm 1 中有描述。

发送端需要区分**两种拥塞**：

1. **真实拥塞 (Physical Congestion)**：物理队列真的堵了，延迟很高。
    
2. **幻影拥塞 (Phantom Congestion)**：物理队列是空的，但幻影队列计算出需要减速。
    

**代码实现逻辑 (Sender Side):**

C++

```
// 当收到带有 ECN 标记的 ACK 时
if (ack.ecn_marked) {
    // 计算当前测得的排队延迟
    // RTT_base 是该路径的最小物理 RTT
    simtime_picoseconds queue_delay = measured_rtt - rtt_base;

    double md_scale = 1.0;

    // 判断是否仅仅是幻影拥塞
    // 如果排队延迟接近 0 (或者非常小)，说明物理队列是空的
    if (queue_delay <= TINY_THRESHOLD) {
        // [cite: 386, 387]
        // 此时是幻影队列在告警，但物理路况还行
        // 采用 "Gentle Reduction" (温和减少)
        md_scale = 0.3; 
    } else {
        // [cite: 391, 392]
        // 物理队列也堵了，必须全速降低
        md_scale = 1.0;
    }

    // 执行乘法减小 (MD)
    // MD_ECN 是标准衰减因子
    cwnd = cwnd * (1 - MD_ECN * md_scale);
}
```

### 总结 (Summary for Implementation)

要复现 Phantom Queue，你需要在 `htsim` 中做两处主要修改：

1. **在 Switch/Queue 类中**：实现一个比物理出口速率慢 10% 的虚拟计数器，并基于这个计数器的值来打 ECN 标记。
    
2. **在 Sender/UnoCC 类中**：在处理 ECN 时检查 RTT。如果 RTT 很低但有 ECN，说明是幻影队列在起作用，此时应将窗口减少的力度（MD Factor）乘以 0.3。
    

这是 Uno 能够同时处理 Intra-DC（低延迟）和 Inter-DC（大吞吐）流量且互不干扰的关键所在。