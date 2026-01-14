### 核心方法拆解：详细处理流程 (Step-by-Step Pipeline)

LR2 的核心思想是 **"分工解耦"**：将复杂的选择性重传（SR）逻辑下沉到网内交换机（Sentry & Depot），而保留端侧 RNIC 简单的 GBN 逻辑。

我们将流程分为 **正常传输**、**长途丢包检测与反馈**、**发送端协作恢复** 三个阶段。

#### Phase 1: 正常传输与状态维护 (Normal Transmission)

- **Step 1.1: Sentry 放行与记录 (Sender Side)**
    
    - 靠近发送端的 DCI 交换机（**Sentry**）接收源 RNIC 发出的数据包。
        
    - Sentry 维护流状态 $ePSN_s$（期望序列号）。如果数据包按序，则放行进入长途链路 。
        
- **Step 1.2: Depot 备份与转发 (Receiver Side)**
    
    - 靠近接收端的 DCI 交换机（**Depot**）接收来自长途链路的数据包。
        
    - 如果数据包按序 ($PSN = ePSN$)，Depot 将其转发给目的 RNIC，并在本地 **Backup Pool** 中保留副本 。
        
    - _注：Backup Pool 用于解决接收端数据中心内部（Intra-DC）的丢包，避免触发长途重传。_
        

#### Phase 2: 长途丢包检测与反馈 (Long-haul Loss Detection & Feedback)

- **Step 2.1: Depot 检测乱序 (Gap Detection)**
    
    - 假设长途链路发生丢包（例如 Seq 10丢失，Seq 11-20到达）。
        
    - Depot 检测到 $PSN > ePSN$，进入 **"Reordering Mode"**。它不再转发乱序包给接收端（防止接收端 RNIC 触发 GBN NACK），而是将 Seq 11-20 暂时缓存到 **Reordering Pool** 。
        
- **Step 2.2: 生成网内 NACK (Depot-NACK)**
    
    - Depot 利用 **Dual-Register** 机制（记录 $ePSN$ 和 $ePSN\_D$）计算丢包区间，生成一个特殊的 **Depot-NACK** 发回发送端。这个 NACK 携带了具体的丢失范围信息 。
        

#### Phase 3: 发送端协作恢复 (Collaborative Recovery)

- **Step 3.1: Sentry 解析与伪装 (NACK Translation)**
    
    - Sentry 收到 Depot-NACK，解析出丢失的序列号（如 Seq 10），并在本地 **Bitmap** 中标记该包为“待重传”。
        
    - Sentry 将这个特殊的 NACK 转换为普通的 RNIC 可识别的 NACK（伪装成 GBN NACK），发给源 RNIC 。
        
- **Step 3.2: 源 RNIC 盲目重传 (Blind GBN Retransmission)**
    
    - 源 RNIC 收到 NACK，认为是普通丢包，根据 GBN 逻辑，开始重传 Seq 10 以及其后的 Seq 11-20 。
        
- **Step 3.3: Sentry 过滤冗余 (SR Filtering - 核心步骤)**
    
    - 重传的数据流到达 Sentry。Sentry 查阅本地 Bitmap：
        
        - **命中 (Seq 10)**：这是真正丢失的包 -> **放行** 并高优先级转发。
            
        - **未命中 (Seq 11-20)**：这是 Depot 已经缓存的包 -> **直接丢弃 (Filter out)**，节省长途带宽 。
            
- **Step 3.4: Depot 重组与提交 (Reconstruction)**
    
    - 重传的 Seq 10 到达 Depot。
        
    - Depot 将 Seq 10 与缓存池中的 Seq 11-20 拼接，恢复按序状态。
        
    - Depot 将 Seq 10-20 一次性按序发给目的 RNIC，完成恢复 。
        

---

### 流程可视化 (Mermaid Diagram)

这张图展示了 LR2 如何拦截并“欺骗” RNIC，从而在不修改硬件的情况下实现高效重传。

代码段

```
sequenceDiagram
    participant SrcRNIC as Source RNIC (GBN)
    participant Sentry as Sentry Switch (Filter)
    participant Link as Long-haul Link
    participant Depot as Depot Switch (Buffer)
    participant DstRNIC as Dest RNIC

    Note over SrcRNIC, DstRNIC: Phase 1: Normal Transmission
    SrcRNIC->>Sentry: Send Pkt 10, 11, 12
    Sentry->>Link: Forward Pkt 10, 11, 12
    Link--xDepot: Pkt 10 Lost (X)
    Link->>Depot: Arrive Pkt 11, 12

    Note over Depot: Step 2.1: Detect Gap (Exp:10, Got:11)
    Depot->>Depot: Buffer Pkt 11, 12 (Reordering Pool)
    Note over Depot: Step 2.2: Generate Depot-NACK (Lost: 10)
    Depot-->>Link: Send Depot-NACK (Gap: 10)
    Link-->>Sentry: Arrive Depot-NACK

    Note over Sentry: Step 3.1: Update Bitmap (Need: 10)
    Sentry-->>SrcRNIC: Forward as GBN NACK (Req: 10)

    Note over SrcRNIC, Sentry: Phase 3: Collaborative Recovery
    Note over SrcRNIC: Step 3.2: GBN Retransmit All (10, 11, 12)
    SrcRNIC->>Sentry: Retransmit Pkt 10
    SrcRNIC->>Sentry: Retransmit Pkt 11 (Redundant)
    SrcRNIC->>Sentry: Retransmit Pkt 12 (Redundant)

    Note over Sentry: Step 3.3: SR Filtering
    Sentry->>Link: Forward Pkt 10 (Match Bitmap)
    Sentry-xSentry: Drop Pkt 11 (Already at Depot)
    Sentry-xSentry: Drop Pkt 12 (Already at Depot)

    Link->>Depot: Arrive Pkt 10
    Note over Depot: Step 3.4: Reassemble
    Depot->>Depot: Combine Pkt 10 + [11, 12]
    Depot->>DstRNIC: Deliver Pkt 10, 11, 12 In-Order
```

### 总结

对于我们搞 AI Infra 的人来说，LR2 的本质是 **用 Switch 的计算能力和 Buffer 换取了 WAN 的带宽效率**。它不需要端侧 RNIC 具备昂贵的 SR 逻辑（这对大规模部署商用网卡非常友好），却能达到 SR 的效果，极大地减少了长尾延迟（Tail Latency），这对大模型训练中的梯度同步稳定性至关重要。