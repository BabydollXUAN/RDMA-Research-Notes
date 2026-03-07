实验图分析报告

## 图 1: 数据中心内部与跨数据中心通信的延迟特性对比

<img src="https://files.manuscdn.com/user_upload_by_module/session_file/310519663384251806/PxGOAFtWgNoTIJlR.webp" width="600"/>

### 基本要素

- **图 1A**: 展示了微软 Azure 在欧洲的数据中心间的网络拓扑，标注了数据中心之间的往返延迟时间（Round-Trip Time, RTT），单位为毫秒（ms）。
- **图 1B**: 一个折线图，展示了消息完成时间与消息大小的关系。
    - **X轴**: 消息大小（Message Size），从 32 KiB 到 64 GiB，采用对数坐标轴。
    - **Y轴**: 延迟开销 / 吞吐量边界（Latency Overhead / Throughput Bound），表示消息传输完成时间中，由延迟或吞吐量决定的部分所占的百分比。
    - **图例 (Legend)**:
        - **Intra-DC RTT (10 µs)**: 代表数据中心内部的通信，其 RTT 极低（10微秒），作为基准场景。
        - **Inter-DC RTT (1 ms, 20 ms, 40 ms)**: 代表跨数据中心的通信，展示了不同地理距离下的 RTT 场景。
        - **Latency Bound**: 虚线部分，表示在消息传输中，时间开销完全由光速传播延迟决定的理论边界。
        - **Throughput Bound**: 实线填充区域，表示时间开销完全由网络带宽（吞吐量）决定的理论边界。

### 核心趋势

- **图 1A** 直观地显示了跨数据中心通信的延迟远高于数据中心内部，RTT 从几毫秒到数十毫秒不等，存在显著的异构性。
- **图 1B** 的核心趋势揭示了两种通信模式的本质区别：
    1.  **数据中心内部 (Intra-DC)**: 对于非常小的消息，传输时间受延迟影响；但一旦消息体积超过 256 KiB，传输瓶颈便迅速转移到网络吞吐量上。这表明在数据中心内部，绝大多数流量的传输效率是由**带宽**决定的。
    2.  **跨数据中心 (Inter-DC)**: 随着 RTT 的增加，延迟成为决定消息完成时间的主要因素。例如，在 40ms 的 RTT 下，即使是 1 GiB 大小的消息，其传输时间绝大部分（超过80%）仍然受限于延迟。消息越大，吞吐量的影响才逐渐显现。
- **拐点现象**: 图中清晰地展示了从“延迟主导”到“吞吐量主导”的拐点。这个拐点的位置随着 RTT 的增大而向右移动，意味着 RTT 越高，需要传输的消息就必须越大，其完成时间才会开始受带宽限制。

### 对比分析

此图并非直接将作者提出的 Uno 方法与其他基线进行性能对比，而是通过对比两种流量（Intra-DC vs. Inter-DC）的物理特性，来阐述问题的根源。分析如下：

- **问题场景**: 传统的数据中心拥塞控制算法（如 DCTCP）主要为低延迟、高带宽的同质化内部网络设计，其核心假设是流量传输受吞吐量限制。然而，图 1 的分析表明，这一假设在跨数据中心的长延迟场景下完全不成立。
- **机制解释**: 将为 Intra-DC 设计的算法直接应用于 Inter-DC 流量会引发一系列问题。例如，依赖快速反馈（ECN 标记、丢包等）的算法在长延迟环境下会做出滞后且错误的决策，导致网络利用率不足、收敛缓慢以及不公平等问题。反之亦然，为广域网设计的协议（如 BBR）也难以适应数据中心内部对超低延迟和精细化控制的需求。

### 核心结论

**一句话总结：图 1 通过理论分析和真实世界的数据中心拓扑，深刻揭示了跨数据中心（Inter-DC）通信以延迟为瓶颈、而数据中心内部（Intra-DC）通信以吞吐量为瓶颈的根本性差异，从而论证了设计一个能够统一、高效、公平地处理这两种混合流量的新一代拥塞控制协议的必要性和紧迫性，为本文提出的 Uno 解决方案奠定了核心动机。**

这个脚本根本没有触发底层的网络仿真器。它内部是一段纯数学公式（计算在不同的延迟和消息大小下的传输时间占比） 的 matplotlib 绘图代码，写死了几个延迟常数来绘制理论曲线。因此这图永远都不会变。

## 图 2: Uno 架构概览

<img src="https://files.manuscdn.com/user_upload_by_module/session_file/310519663384251806/RPuTGelVAkkqSMzA.webp" width="600"/>

### 基本要素

- **图 2** 是一张概念性架构图，展示了本文提出的 Uno 系统的整体设计及其两大核心组件：
    - **UnoCC (Congestion Control component)**: 拥塞控制组件，负责处理网络拥塞。
        - **Vastly diverse RTTs and delayed feedback**: 指出跨数据中心通信中 RTT 差异大和反馈延迟的问题。
        - **Uses phantom queues to mitigate BDP mismatch**: 采用虚拟队列来解决带宽延迟积（BDP）不匹配的问题。
        - **Fair between different RTTs**: 旨在实现不同 RTT 流量之间的公平性。
    - **UnoRC (Reliable Connectivity component)**: 可靠连接组件，负责处理数据传输的可靠性。
        - **Most messages are latency bound**: 强调跨数据中心通信中大多数消息受延迟限制。
        - **Incorporates erasure coding with load balancing (UnoLB) to limit drops**: 结合纠删码和负载均衡（UnoLB）来减少丢包。
        - **Mismatch between BDP and buffer capacity**: 指出 BDP 与缓冲区容量不匹配的问题。
        - **Single loss can have a dramatic impact**: 强调单个丢包对长延迟链路的影响巨大。

### 核心趋势

图 2 整体呈现了 Uno 作为一个**统一的、端到端解决方案**的架构理念。它将拥塞控制（UnoCC）、负载均衡和丢包恢复（UnoRC）紧密集成，以应对数据中心内部（Intra-DC）和跨数据中心（Inter-DC）流量的异构性挑战。核心趋势是强调通过集成化设计，解决传统方法在处理混合流量时遇到的碎片化和低效问题。

### 对比分析

图 2 并非直接与基线进行性能对比，而是通过展示 Uno 的集成式架构，间接说明其如何克服现有方法的局限性。结合论文前文的讨论：

- **传统方法的局限性**: 针对 Intra-DC 和 Inter-DC 流量，传统上采用不同的拥塞控制协议（例如，DCTCP 适用于 Intra-DC，BBR 适用于 WAN）。这种分离导致了：
    - **反馈粒度不匹配**: Intra-DC 流量的反馈速度远快于 Inter-DC，导致混合流量场景下的不公平和资源利用不足。
    - **BDP 异构性**: Inter-DC 的 BDP 远大于 Intra-DC，导致为 Intra-DC 设计的缓冲区管理策略在 Inter-DC 场景下失效。
    - **低效的丢包处理**: 长延迟链路中单个丢包的影响被放大，传统重传机制效率低下。
- **Uno 的优势**: Uno 通过其 UnoCC 和 UnoRC 组件的紧密集成，旨在提供一个“一站式”解决方案：
    - **统一拥塞控制**: UnoCC 使用“幻影队列”等机制，能够为不同 RTT 的流量提供公平的拥塞控制，解决了反馈粒度不匹配和 BDP 异构性问题。
    - **可靠连接**: UnoRC 结合纠删码和负载均衡，在长延迟、易丢包的跨数据中心链路上提供高效的丢包恢复，避免了传统重传的延迟开销。
    - **协同优化**: 两个组件协同工作，确保在 Intra-DC 和 Inter-DC 混合流量场景下，网络资源得到高效利用，同时保证低延迟和高可靠性。

### 核心结论

**一句话总结：图 2 形象地展示了 Uno 作为一种“一站式”解决方案的整体架构，通过其拥塞控制（UnoCC）和可靠连接（UnoRC）两大核心组件的紧密集成，旨在克服传统方法在处理数据中心内部和跨数据中心混合流量时面临的挑战，从而实现统一、高效、公平的网络性能，是本文方法的核心设计理念和贡献的体现。**


## 图 3: 混合 Incast 场景下 Uno 的带宽公平性与收敛速度

<img src="https://files.manuscdn.com/user_upload_by_module/session_file/310519663384251806/kcZpkbZOWkKMfKnK.webp" width="600"/>

### 基本要素

- **图 3A**: 实验设置示意图，展示了两个数据中心（DC1, DC2）通过多条链路连接，其中 DC2 存在一个**热点（Hotspot）**，接收来自 DC1 的 Intra-DC 流和 Inter-DC 流。发送方（Sending to 15）和接收方（Receiver 15）表示有 15 个发送方和 15 个接收方，==**形成 Incast 场景**==。
- **图 3B**: 展示了 Gemini 协议在混合 Incast 场景下 Intra-DC 流和 Inter-DC 流的发送速率（Gbps）随时间（ms）的变化。
    - **X轴**: 时间（Time），单位为毫秒（ms）。
    - **Y轴**: 发送速率（Sending Rate），单位为 Gbps。
    - **图例 (Legend)**:
        - **Gemini**: 一种现有的拥塞控制协议。
        - **Intra-DC flow**: 数据中心内部流量。
        - **Inter-DC flow**: 跨数据中心流量。
- **图 3C**: 展示了 MPRDMA+BBR 协议在混合 Incast 场景下 Intra-DC 流和 Inter-DC 流的发送速率（Gbps）随时间（ms）的变化。
    - **X轴**: 时间（Time），单位为毫秒（ms）。
    - **Y轴**: 发送速率（Sending Rate），单位为 Gbps。
    - **图例 (Legend)**:
        - **MPRDMA+BBR**: 另一种现有的拥塞控制协议组合。
        - **Intra-DC flow**: 数据中心内部流量。
        - **Inter-DC flow**: 跨数据中心流量。
- **图 3D**: 展示了 Uno 协议在混合 Incast 场景下 Intra-DC 流和 Inter-DC 流的发送速率（Gbps）随时间（ms）的变化，并与理想公平发送速率（Ideal fair sending rate）进行对比。
    - **X轴**: 时间（Time），单位为毫秒（ms）。
    - **Y轴**: 发送速率（Sending Rate），单位为 Gbps。
    - **图例 (Legend)**:
        - **Uno**: 本文提出的拥塞控制协议。
        - **Ideal fair sending rate**: 理想的公平发送速率。
        - **Intra-DC flow**: 数据中心内部流量。
        - **Inter-DC flow**: 跨数据中心流量。

### 核心趋势

- **Gemini (图 3B)**: Gemini 协议在混合流量场景下，虽然最终能达到一定的公平性，但收敛速度非常缓慢。Intra-DC 流和 Inter-DC 流的发送速率在很长一段时间内波动剧烈，且未能迅速达到稳定状态，这表明其在处理混合流量时效率不高。
- **MPRDMA+BBR (图 3C)**: MPRDMA+BBR 协议在混合流量场景下表现出明显的不公平性。Intra-DC 流和 Inter-DC 流的发送速率差异显著，其中一种流量（通常是 Intra-DC 流）可能会“饿死”另一种流量，无法实现公平的带宽共享。
- **Uno (图 3D)**: Uno 协议在混合流量场景下表现出快速收敛和良好的公平性。Intra-DC 流和 Inter-DC 流的发送速率能够迅速收敛到接近理想公平发送速率的水平，且两种流量的速率保持一致，波动较小。

### 对比分析

- **与 Gemini 对比**: Uno 在收敛速度上显著优于 Gemini。Gemini 虽然能够**保证公平性，但其收敛时间过长**，在实际应用中可能导致网络资源长时间处于非最优状态。Uno 通过其统一的拥塞控制逻辑和快速适应机制（如 Quick Adapt），能够更快地达到公平状态，从而提高网络利用率和用户体验。
- **与 MPRDMA+BBR 对比**: Uno 在公平性上显著优于 MPRDMA+BBR。MPRDMA+BBR 协议由于其分离的拥塞控制机制，导致在混合流量场景下无法有效协调 Intra-DC 和 Inter-DC 流量，造成带宽分配不公平。Uno 通过统一的反馈机制和幻影队列等设计，确保了不同类型流量在共享瓶颈链路时能够获得公平的带宽份额。
- **优势原因**: Uno 取得这些优势的核心机制在于其**统一的拥塞控制逻辑**和**快速适应能力**。UnoCC 组件通过幻影队列（Phantom Queues）解决了 BDP 不匹配问题，使得对 Intra-DC 和 Inter-DC 流量的拥塞反馈能够以相同的粒度进行处理。同时，Quick Adapt 机制使得 Uno 在检测到极端拥塞时能够迅速降低发送速率，避免网络过载，从而实现快速收敛和公平性。

### 核心结论

**一句话总结：图 3 通过在混合 Incast 场景下与现有协议 Gemini 和 MPRDMA+BBR 的对比，有力证明了 Uno 在实现 Intra-DC 和 Inter-DC 流量带宽公平性以及快速收敛方面的卓越性能，从而验证了本文提出的统一拥塞控制和可靠连接机制的有效性，是 Uno 协议性能优势的关键实验证据。**


## 图 4: 幻影队列对 Intra-DC 流量的影响

<img src="https://files.manuscdn.com/user_upload_by_module/session_file/310519663384251806/PznQDfmPVyEecjeh.webp" width="600"/>

### 基本要素

- **图 4A**: 展示了在没有幻影队列（Without Phantom Queue）的情况下，Incast 接收端的队列占用随时间的变化。
    - **X轴**: 时间（Time），单位为毫秒（ms）。
    - **Y轴**: 队列占用（Queue Occupancy），单位为包（Packets）。
    - **图例 (Legend)**: 未明确给出，但曲线代表了队列中排队的包数量。
- **图 4B**: 展示了在有幻影队列（With Phantom Queue）的情况下，Incast 接收端的队列占用随时间的变化。
    - **X轴**: 时间（Time），单位为毫秒（ms）。
    - **Y轴**: 队列占用（Queue Occupancy），单位为包（Packets）。
    - **图例 (Legend)**:
        - **Real queue remains empty**: 实际队列保持为空。
        - **Phantom queue stable**: 幻影队列保持稳定。
- **图 4C**: 比较了在有/无幻影队列情况下，短 Intra-DC 消息的流完成时间（Flow Completion Time, FCT）。
    - **X轴**: 场景（Scenarios），分为“Without Phantom Queues”和“With Phantom Queues”。
    - **Y轴**: 平均 FCT（Average FCT），单位为微秒（µs）。
    - **图例 (Legend)**: 未明确给出，但柱状图代表了两种场景下的平均 FCT。

### 核心趋势

- **无幻影队列 (图 4A)**: 在没有幻影队列的情况下，接收端的队列**占用波动剧烈**，**在 Incast 场景下迅速堆积大量数据包，表明网络拥塞严重**，可能导致高延迟和丢包。
- **有幻影队列 (图 4B)**: 引入幻影队列后，实际队列（Real queue）几乎保持为空，而幻影队列（Phantom queue）则保持稳定。这表明幻影队列成功地在不实际占用物理缓冲区的情况下，实现了对拥塞的有效管理和信号传递。
- **FCT 对比 (图 4C)**: 引入幻影队列后，短 Intra-DC 消息的平均 FCT 从约 793 µs 显著降低到约 395 µs，几乎减半。这表明幻影队列有效地**降低了消息的传输延迟**。

### 对比分析

- **与无幻影队列对比**: 幻影队列在 Incast 场景下展现出显著优势。在没有幻影队列时，物理队列会迅速饱和，导致高延迟和潜在的丢包。而幻影队列通过在逻辑层面模拟队列行为，实现了“近零排队”（Near-zero queuing），即物理队列保持空闲，从而避免了传统队列拥塞带来的延迟。这种机制使得拥塞信号能够更早、更平滑地传递给发送方，避免了物理队列的剧烈波动和过度填充。
- **优势原因**: 幻影队列的核心机制在于它是一个**虚拟队列**，不实际消耗物理缓冲区。它通过在逻辑上跟踪流量的“排队”情况，并根据其状态生成拥塞信号，从而在不引入额外物理延迟的情况下，实现了对拥塞的有效管理。这对于 Intra-DC 流量尤其重要，因为 Intra-DC 流量对延迟非常敏感。通过保持物理队列空闲，幻影队列确保了低延迟，同时通过其稳定的状态避免了网络利用率不足。

### 核心结论

**一句话总结：图 4 通过对比有无幻影队列在 Incast 场景下对队列占用和流完成时间的影响，清晰地展示了幻影队列能够有效实现“近零排队”，显著降低 Intra-DC 流量的传输延迟，从而验证了幻影队列作为 UnoCC 核心机制之一，在提升数据中心内部通信性能方面的关键作用和贡献。**


## 图 5: Uno 的整体架构

<img src="https://files.manuscdn.com/user_upload_by_module/session_file/310519663384251806/PznQDfmPVyEecjeh.webp" width="600"/>

### 基本要素

- **图 5** 是一张 Uno 整体架构的示意图，展示了其两大核心组件 UnoCC 和 UnoRC，以及它们内部的关键机制。
    - **UnoCC (Congestion Control component)**: 拥塞控制组件。
        - **ECN Based**: 基于显式拥塞通知（ECN）进行拥塞控制。
        - **Phantom Queue**: 幻影队列，用于缓解 BDP 不匹配问题。
        - **Quick Adapt (QA)**: 快速适应机制，用于应对极端拥塞。
        - **AIMD**: 加性增、乘性减算法，用于窗口调整。
    - **UnoRC (Reliable Connectivity component)**: 可靠连接组件。
        - **Subflow-level LB (UnoLB)**: 子流级别的负载均衡。
        - **Erasure Coding (EC)**: 纠删码，用于提高可靠性。

### 核心趋势

图 5 描绘了 Uno 作为一个**统一系统**的内部工作原理，强调了其通过集成多种机制来解决数据中心内部和跨数据中心通信挑战的策略。核心趋势是 Uno 将拥塞控制和可靠性功能解耦并分别优化，但又通过一个统一的框架进行协调，以实现低延迟、高吞吐量和高可靠性。

### 对比分析

图 5 作为一个架构图，不直接进行性能对比，但它展示了 Uno 如何通过其内部机制来克服现有方法的不足：

- **传统拥塞控制的局限**: 传统的拥塞控制协议往往只关注单一类型的网络环境（如 Intra-DC 或 Inter-DC），导致在混合流量场景下性能不佳。例如，为 Intra-DC 设计的协议可能无法有效处理 Inter-DC 的高延迟和丢包，而为 Inter-DC 设计的协议可能无法满足 Intra-DC 的低延迟需求。
- **Uno 的优势**: Uno 通过以下机制解决了这些问题：
    - **UnoCC 的创新**: 结合 ECN、幻影队列、Quick Adapt 和 AIMD，UnoCC 能够对不同 RTT 的流量提供公平且快速响应的拥塞控制。幻影队列解决了 BDP 异构性问题，使得拥塞信号能够更早、更准确地传递，而 Quick Adapt 则确保了在极端拥塞情况下的快速恢复。
    - **UnoRC 的创新**: 通过子流级别的负载均衡和纠删码，UnoRC 提高了跨数据中心链路的可靠性和效率。纠删码减少了对传统重传机制的依赖，从而降低了长延迟链路上的恢复时间，而负载均衡则优化了多路径利用。
    - **统一框架**: Uno 将 UnoCC 和 UnoRC 整合在一个统一的系统下，使得拥塞控制和可靠性机制能够协同工作，共同优化混合流量的性能。

### 核心结论

**一句话总结：图 5 清晰地展示了 Uno 作为一个统一的、多功能解决方案的内部架构，通过其 UnoCC 和 UnoRC 组件中集成的 ECN、幻影队列、Quick Adapt、AIMD、子流负载均衡和纠删码等关键机制，全面应对了数据中心内部和跨数据中心通信中的拥塞控制和可靠性挑战，是本文方法设计理念和技术创新的集中体现。**


## 图 6: UnoCC 的设计

<img src="https://files.manuscdn.com/user_upload_by_module/session_file/310519663384251806/kYGkaTrbezgVWFvK.webp" width="600"/>

### 基本要素

- **图 6** 是一张 UnoCC（拥塞控制组件）的内部设计示意图，展示了其关键组成部分和工作流程。
    - **Sender (发送方)**: 流量的源头。
    - **Network (网络)**: 数据传输的路径。
    - **Receiver (接收方)**: 流量的目的地。
    - **Transmission-bound messages**: 传输受带宽限制的消息。
    - **Congestion control crucial**: 拥塞控制至关重要。
    - **Vastly different RTTs and BDPs**: 巨大的 RTT 和 BDP 差异。
    - **Physical queue**: 物理队列，实际的缓冲区。
    - **Phantom Queue**: 幻影队列，UnoCC 的核心机制之一。
    - **ECN Based AI**: 基于 ECN 的加性增。
    - **MD**: 乘性减。
    - **QA**: Quick Adapt 快速适应机制。

### 核心趋势

图 6 描绘了 UnoCC 如何通过集成多种机制来应对数据中心内部和跨数据中心流量的拥塞控制挑战。核心趋势是 UnoCC 旨在提供一个**统一且自适应的拥塞控制方案**，能够处理不同 RTT 和 BDP 的流量，并通过幻影队列、ECN、AIMD 和 Quick Adapt 等机制实现低延迟、高公平性和快速收敛。

### 对比分析

图 6 作为一个设计图，不直接进行性能对比，但它展示了 UnoCC 如何通过其内部机制来克服现有拥塞控制协议的不足：

- **传统拥塞控制的局限**: 传统的拥塞控制协议通常难以在具有巨大 RTT 和 BDP 差异的混合流量环境中保持公平性和效率。例如，一些协议可能对高延迟链路反应迟钝，导致网络利用率不足；另一些协议可能在低延迟环境中过度激进，导致队列溢出和丢包。
- **UnoCC 的优势**: UnoCC 通过以下机制解决了这些问题：
    - **幻影队列**: 解决了 BDP 不匹配问题，使得拥塞信号能够更早、更平滑地传递，避免了物理队列的过度填充，从而实现了“近零排队”和低延迟。
    - **ECN Based AI 和 MD**: 结合 ECN 信号进行加性增、乘性减，使得 UnoCC 能够根据网络拥塞程度动态调整发送速率，实现公平性。
    - **Quick Adapt (QA)**: 在检测到极端拥塞时，QA 机制能够迅速降低发送速率，避免网络崩溃，并加速恢复。
    - **统一处理**: UnoCC 将这些机制整合在一个框架下，能够统一处理 Intra-DC 和 Inter-DC 流量，确保在不同网络条件下都能提供稳定的性能。

### 核心结论

**一句话总结：图 6 详细展示了 UnoCC 拥塞控制组件的内部设计，突出了幻影队列、ECN Based AI、MD 和 Quick Adapt 等关键机制的协同作用，旨在为具有巨大 RTT 和 BDP 差异的混合流量提供统一、高效、公平的拥塞控制，是 Uno 协议实现其核心性能目标的设计基础。**


## 图 7: UnoRC 的设计

<img src="https://files.manuscdn.com/user_upload_by_module/session_file/310519663384251806/WYVlsTPYsKQlzuZS.webp" width="600"/>

### 基本要素

- **图 7** 是一张 UnoRC（可靠连接组件）的内部设计示意图，展示了其两大核心机制：纠删码（Erasure Coding, EC）和子流负载均衡（Subflow Routing, UnoLB）。
    - **Sender (发送方)**: 流量的源头。
    - **Receiver (接收方)**: 流量的目的地。
    - **Latency-bound messages**: 延迟受限的消息。
    - **The inter-DC RTT is high**: 跨数据中心 RTT 高。
    - **A single timeout/retx is bad**: 单次超时/重传影响大。
    - **Subflow Routing (UnoLB) with Erasure Coding (EC)**: 结合纠删码的子流路由（UnoLB）。
    - **Packets go on different paths**: 数据包通过不同路径传输。
    - **Sender removes bad path even with failures**: 即使出现故障，发送方也能移除不良路径。
    - **Receiver can decode message**: 接收方可以解码消息。

### 核心趋势

图 7 描绘了 UnoRC 如何通过纠删码和子流负载均衡来提高跨数据中心通信的可靠性和效率。核心趋势是 UnoRC 旨在提供一个**高效的丢包恢复和多路径利用方案**，以应对跨数据中心链路的高延迟和潜在丢包问题，从而减少对传统重传机制的依赖，降低消息完成时间。

### 对比分析

图 7 作为一个设计图，不直接进行性能对比，但它展示了 UnoRC 如何通过其内部机制来克服现有可靠性协议的不足：

- **传统可靠性协议的局限**: 在高延迟的跨数据中心环境中，传统的基于重传的可靠性协议（如 TCP）在发生丢包时会引入显著的延迟。单个丢包可能导致长时间的超时和重传，严重影响消息完成时间。此外，传统的多路径路由（如 ECMP）可能存在哈希冲突，导致流量集中在少数路径上，降低了多路径的利用效率。
- **UnoRC 的优势**: UnoRC 通过以下机制解决了这些问题：
    - **纠删码 (EC)**: UnoRC 采用纠删码技术，将原始数据包与冗余的校验包一起发送。这意味着即使部分数据包在传输过程中丢失，接收方仍然可以通过剩余的数据包和校验包来恢复原始数据，从而避免了耗时的重传。这对于延迟敏感的跨数据中心流量至关重要。
    - **子流负载均衡 (UnoLB)**: UnoRC 将一个流量分解成多个子流，并通过不同的路径传输。这不仅提高了带宽利用率，还增加了对链路故障的容忍度。UnoLB 能够动态地识别和避免不良路径，进一步提升了可靠性。
    - **协同作用**: 纠删码和子流负载均衡协同工作，使得 UnoRC 能够在高延迟、易丢包的跨数据中心环境中提供更强的可靠性和更低的延迟。子流负载均衡分散了风险，而纠删码则提供了快速的丢包恢复能力。

### 核心结论

**一句话总结：图 7 详细展示了 UnoRC 可靠连接组件的内部设计，突出了纠删码和子流负载均衡两大关键机制的协同作用，旨在通过高效的丢包恢复和多路径利用，显著提高跨数据中心通信的可靠性和效率，减少对传统重传的依赖，是 Uno 协议实现其高可靠性目标的设计基础。**


## 图 8: Uno 在混合流量场景下的公平性与性能对比

<img src="https://files.manuscdn.com/user_upload_by_module/session_file/310519663384251806/SsWVJDVVnVoEFrvg.webp" width="600"/>

### 基本要素

- **图 8 上半部分 (Uno Fairness)**: 展示了在不同 Inter-DC 流量数量下，Uno 协议的 Intra-DC 和 Inter-DC 流完成时间（Completion Time）。
    - **X轴**: Inter-DC 流的数量（0 Inter-DC, 4 Inter-DC, 8 Inter-DC）。
    - **Y轴**: 完成时间（Completion Time），单位为毫秒（ms）。
    - **图例 (Legend)**:
        - **Ideal completion time**: 理想的完成时间。
        - **Inter-DC flow**: 跨数据中心流的完成时间。
        - **Intra-DC flow**: 数据中心内部流的完成时间。
        - **Ideal completion time with phantom slowdown**: 考虑幻影队列减速后的理想完成时间（主要针对 Intra-DC 流）。
- **图 8 下半部分 (Uno Comparisons)**: 比较了 Uno 与其他基线协议（Gemini, MPRDMA+BBR）在不同 Inter-DC 流数量下的平均流完成时间（Average FCT）和 99 百分位流完成时间（P99 FCT）。
    - **X轴**: 不同的协议和 Inter-DC 流数量（0 Inter-DC, 4 Inter-DC, 8 Inter-DC）。
    - **Y轴**: 平均 FCT 和 P99 FCT，单位为毫秒（ms）。
    - **图例 (Legend)**:
        - **Uno+ECMP**: Uno 协议结合 ECMP 负载均衡。
        - **Uno**: 完整的 Uno 协议（包含 UnoLB 和 EC）。
        - **Gemini**: 基线协议。
        - **MPRDMA+BBR**: 基线协议。

**平均 FCT 决定了网络的“日常表现”，而 P99 FCT 决定了网络在复杂环境下的“可靠性底线**
### 核心趋势

- **Uno Fairness (上半部分)**: 随着 Inter-DC 流数量的增加，Inter-DC 和 Intra-DC 流的完成时间普遍增加，但 Uno 协议下的完成时间始终接近理想完成时间，表明其在混合流量场景下能够保持良好的公平性和效率。
- **Uno Comparisons (下半部分)**: Uno（包括 Uno+ECMP 和完整的 Uno）在所有场景下都显著优于 Gemini 和 MPRDMA+BBR，无论是在平均 FCT 还是 P99 FCT 上。特别是在 Inter-DC 流数量增加时，Uno 与基线协议的性能差距更加明显，尤其体现在 P99 FCT 上。完整的 Uno 协议（包含 UnoLB）通常比 Uno+ECMP 表现更好。

### 对比分析

- **与 Gemini 和 MPRDMA+BBR 对比**: Uno 在流完成时间（FCT）方面取得了显著优势。它能够提供更低的平均 FCT 和 P99 FCT，这意味着在混合流量场景下，Uno 能够更快地完成数据传输，并提供更稳定的低延迟性能。这种优势在 Inter-DC 流量增多时尤为突出，表明 Uno 在处理跨数据中心流量的挑战方面表现卓越。
- **优势原因**: Uno 取得这些优势的核心机制在于其**统一的拥塞控制逻辑（UnoCC）**和**高效的可靠连接机制（UnoRC）**。
    - **UnoCC 的统一性**: 传统的协议（如 MPRDMA+BBR）可能将 Intra-DC 和 Inter-DC 流量分开处理，导致在共享资源时出现不公平。Gemini 虽然尝试统一，但收敛速度较慢。UnoCC 通过幻影队列和 Quick Adapt 等机制，能够为两种流量提供统一且快速响应的拥塞反馈，确保了公平的带宽分配和快速收敛。
    - **UnoRC 的可靠性与效率**: 完整的 Uno 协议（包含 UnoRC）通过纠删码和子流负载均衡（UnoLB）进一步提升了性能。纠删码减少了对传统重传的依赖，降低了长延迟链路上的丢包恢复时间。UnoLB 则通过智能的多路径利用，优化了流量分布，进一步降低了 FCT，尤其是在面对链路故障或拥塞时。

### 核心结论

**一句话总结：图 8 通过在不同混合流量场景下与现有协议的全面对比，有力证明了 Uno 在实现低流完成时间（FCT）和高公平性方面的卓越性能，尤其是在 Inter-DC 流量增多时，从而验证了 Uno 协议作为统一解决方案在数据中心内部和跨数据中心混合流量环境中的有效性和优越性，是 Uno 协议核心性能优势的关键实验证据。**


## 图 9: 置换工作负载下 Uno 的性能展示

<img src="https://files.manuscdn.com/user_upload_by_module/session_file/310519663384251806/nTnGOSwRjknNLxoW.webp" width="600"/>

### 基本要素

- **图 9 上半部分 (8 DC-to-DC links)**: 在 **8 条数据中心间链路**的场景下，比较了 Uno+ECMP、Uno、Gemini 和 MPRDMA+BBR 在平均流完成时间（Avg FCT）和 99 百分位流完成时间（P99 FCT）上的性能。
    - **X轴**: 协议类型。
    - **Y轴**: 完成时间（Completion Time），单位为毫秒（ms）。
    - **图例 (Legend)**: Uno+ECMP, Uno, Gemini, MPRDMA+BBR。
- **图 9 下半部分 (128 DC-to-DC links)**: 在 128 条数据中心间链路的场景下，比较了 Uno+ECMP、Uno、Gemini 和 MPRDMA+BBR 在平均流完成时间（Avg FCT）和 99 百分位流完成时间（P99 FCT）上的性能。
    - **X轴**: 协议类型。
    - **Y轴**: 完成时间（Completion Time），单位为毫秒（ms）。
    - **图例 (Legend)**: Uno+ECMP, Uno, Gemini, MPRDMA+BBR。

==**更多的链路直接带来了更大的整体带宽容量，从而有效提升了数据的传输效率。**==
### 核心趋势

- **整体趋势**: 在两种不同数量的 DC-to-DC 链路场景下，Uno（包括 Uno+ECMP 和完整的 Uno）在平均 FCT 和 P99 FCT 方面均显著优于 Gemini 和 MPRDMA+BBR。
- **链路数量的影响**: 随着 DC-to-DC 链路数量从 8 增加到 128，所有协议的 FCT 都有所下降，这符合预期（更多链路意味着更大的带宽容量）。然而，Uno 协议的优势依然保持，甚至在 P99 FCT 上，Uno 的表现与基线协议的差距更大，表明其在极端情况下的稳定性。
- **Uno 的内部比较**: 在 8 条链路场景下，Uno（完整的 Uno 协议，包含 UnoLB）略优于 Uno+ECMP。在 128 条链路场景下，Uno 的优势更加明显，尤其是在 P99 FCT 上，这表明 UnoLB 在多链路场景下对性能的提升作用。

### 对比分析

- **与 Gemini 和 MPRDMA+BBR 对比**: Uno 在置换工作负载（permutation workload）下，无论是在平均 FCT 还是 P99 FCT 上，都展现出卓越的性能。这意味着 Uno 能够更有效地处理数据中心内部和跨数据中心流量的混合场景，并提供更低的延迟和更稳定的性能。
- **优势原因**: Uno 取得这些优势的核心机制在于其**统一的拥塞控制逻辑（UnoCC）**和高效的可靠连接机制（UnoRC）**。
    - **UnoCC 的统一性**: UnoCC 能够为不同 RTT 和 BDP 的流量提供统一且快速响应的拥塞反馈，确保了公平的带宽分配和快速收敛，这在置换工作负载这种流量模式多变、异构性强的场景下尤为重要。
    - **UnoRC 的可靠性与效率**: 完整的 Uno 协议（包含 UnoRC）通过纠删码和子流负载均衡（UnoLB）进一步提升了性能。纠删码减少了对传统重传的依赖，降低了长延迟链路上的丢包恢复时间。UnoLB 则通过智能的多路径利用，优化了流量分布，进一步降低了 FCT，尤其是在多链路和复杂流量模式下。

### 核心结论

**一句话总结：图 9 通过在不同数量的 DC-to-DC 链路下，与现有协议 Gemini 和 MPRDMA+BBR 在置换工作负载场景中的性能对比，有力证明了 Uno 在平均流完成时间（FCT）和 99 百分位流完成时间（P99 FCT）方面的显著优势，尤其是在多链路和复杂流量模式下，从而验证了 Uno 协议在处理==混合流量和复杂网络拓扑==时的鲁棒性和高效性，是 Uno 协议性能优势的进一步实验证据。**

在计算机网络（尤其是数据中心网络）研究中，P99 FCT 是一个极其核心的评价指标，很多时候它比平均流完成时间（Average FCT）更能反映网络协议的真实水平。
现代数据中心的应用绝大多数是高度分布式的，通常采用“分片-聚合（Partition-Aggregate）”架构。例如论文中用来做实验的 Google Web Search 工作负载 ，以及 AI 分布式训练中的 AllReduce 梯度同步操作 。 在这类应用中，一个请求会被拆分发给成百上千个服务器，系统**必须等待最后一个最慢的响应返回**，才能组合出最终结果或进行下一次迭代。这意味着，即便 99 个流都在 1 毫秒内完成了，只要有 1 个流花费了 100 毫秒，那么整个应用层感受到的延迟就是 100 毫秒。因此，优化 P99 FCT 就是在直接优化分布式应用的真实瓶颈。

## 图 10: Uno 在不同网络负载下的性能对比

<img src="https://files.manuscdn.com/user_upload_by_module/session_file/310519663384251806/ZvnbITTzDQKxFIxH.webp" width="600"/>

### 基本要素

- **图 10** 包含六个子图，分别展示了在不同网络负载（40%、60%、80%）下，Uno 与其他基线协议在三种流量类型（All Flow, Intra-DC Flows, Inter-DC Flows）的平均流完成时间（Avg FCT）和 99 百分位流完成时间（P99 FCT）上的性能对比。
    - **X轴**: 网络负载条件（Load Conditions），包括 40% Load, 60% Load, 80% Load。
    - **Y轴**: 流完成时间（FCT），单位为毫秒（ms）或微秒（µs）。
    - **图例 (Legend)**:
        - **Uno**: 本文提出的协议。
        - **Uno+ECMP**: Uno 协议结合 ECMP 负载均衡。
        - **Gemini**: 基线协议。
        - **MPRDMA+BBR**: 基线协议。

40%负载-中低度拥挤
60%负载-中高度拥挤
80%负载-极限高压测试

**在 Uno 论文的模拟实验中，这个负载是如何控制的呢？** 根据论文的设定，研究人员是**通过控制数据流的“到达率”（arrival rates）来模拟不同的网络负载**的。具体来说，数据流的到达时间是基于指数分布生成的，研究人员通过按比例缩放这些发送速率（scaled to achieve a desired network load），从而精准地将整体网络带宽的占用率控制在 40%、60% 或 80% 。
### 核心趋势

- **整体趋势**: 在所有网络负载条件下，Uno 协议（包括 Uno 和 Uno+ECMP）在所有流量类型（All Flow, Intra-DC Flows, Inter-DC Flows）的 Avg FCT 和 P99 FCT 上均显著优于 Gemini 和 MPRDMA+BBR。
- **负载增加的影响**: 随着网络负载从 40% 增加到 80%，所有协议的 FCT 普遍增加，但 Uno 协议的性能优势保持稳定，甚至在某些情况下，与基线协议的差距更大，尤其是在 P99 FCT 上。
- **流量类型差异**: Uno 在 Inter-DC Flows 上的性能提升尤为显著，这表明其在处理跨数据中心流量方面具有强大的优势。对于 Intra-DC Flows，Uno 也能保持较低的 FCT，尽管与基线协议的差距可能不如 Inter-DC Flows 那么大。

### 对比分析

- **与 Gemini 和 MPRDMA+BBR 对比**: Uno 在不同网络负载下，无论是平均 FCT 还是 P99 FCT，都展现出卓越的性能。这表明 Uno 能够有效地管理网络拥塞，并在高负载条件下保持低延迟和高稳定性。
- **优势原因**: Uno 取得这些优势的核心机制在于其**统一的拥塞控制逻辑（UnoCC）**和**高效的可靠连接机制（UnoRC）**。
    - **UnoCC 的统一性**: UnoCC 通过幻影队列和 Quick Adapt 等机制，能够为 Intra-DC 和 Inter-DC 流量提供统一且快速响应的拥塞反馈。这使得 Uno 能够更好地适应不同负载条件下的网络变化，避免了传统协议在混合流量场景下的性能瓶颈。
    - **UnoRC 的可靠性与效率**: UnoRC 通过纠删码和子流负载均衡，显著提高了跨数据中心链路的可靠性和效率。在高负载条件下，丢包率可能增加，UnoRC 的纠删码机制能够有效减少重传，从而降低 FCT。同时，子流负载均衡确保了流量在多路径上的高效分布，进一步提升了整体性能。

### 核心结论

**一句话总结：图 10 通过在不同网络负载下，与现有协议 Gemini 和 MPRDMA+BBR 在三种流量类型上的全面性能对比，有力证明了 Uno 在平均流完成时间（FCT）和 99 百分位流完成时间（P99 FCT）方面的显著优势，尤其是在高负载和跨数据中心流量场景下，从而验证了 Uno 协议在复杂网络环境中的鲁棒性和高效性，是 Uno 协议在实际应用中性能优越性的关键实验证据。**


## 图 11: Uno 在不同 Inter-DC RTT 比例下的延迟改进

<img src="https://files.manuscdn.com/user_upload_by_module/session_file/310519663384251806/ZvnbITTzDQKxFIxH.webp" width="600"/>

### 基本要素

- **图 11** 包含两个子图，展示了在 40% 网络负载下，随着 Inter-DC 最小 RTT 与 Intra-DC 最小 RTT 比例的增加，Uno 与其他基线协议在平均流完成时间（Avg FCT）和 99 百分位流完成时间（P99 FCT）减缓比（Slowdown Ratio）上的性能对比。
    - **X轴**: Inter/Intra RTT Ratio（Inter-DC RTT 与 Intra-DC RTT 的比例），从 8 增加到 512。
    - **Y轴**: FCT Slowdown Ratio（流完成时间减缓比）。
    - **图例 (Legend)**:
        - **Uno**: 本文提出的协议。
        - **Uno+ECMP**: Uno 协议结合 ECMP 负载均衡。
        - **Gemini**: 基线协议。
        - **MPRDMA+BBR**: 基线协议。

### 核心趋势

- **整体趋势**: 随着 Inter-DC/Intra-DC RTT 比例的增加，所有协议的 FCT 减缓比都呈现上升趋势，表明 RTT 差异越大，对性能的影响越大。然而，Uno 协议（包括 Uno 和 Uno+ECMP）的 FCT 减缓比始终显著低于 Gemini 和 MPRDMA+BBR。
- **Uno 的优势**: 当 RTT 比例较小（例如 8）时，Uno 的性能与 MPRDMA+BBR 接近，甚至略差于 MPRDMA+BBR（论文中提到是由于幻影队列的轻微减速）。但随着 RTT 比例的增大，Uno 的优势变得越来越明显。当 RTT 比例达到 512 时，Uno 的尾部 FCT 减缓比比 MPRDMA+BBR 和 Gemini 低 5 倍，这表明 Uno 在处理 RTT 差异巨大的网络环境方面具有卓越的性能。

### 对比分析

- **与 Gemini 和 MPRDMA+BBR 对比**: Uno 在处理 Intra-DC 和 Inter-DC RTT 差异巨大的混合流量场景下，展现出显著的性能优势。传统的协议在 RTT 差异增大时，性能下降更为剧烈，而 Uno 能够更好地维持较低的 FCT 减缓比。
- **优势原因**: Uno 取得这些优势的核心机制在于其**统一的拥塞控制逻辑（UnoCC）**和**幻影队列**。
    - **UnoCC 的统一性**: UnoCC 能够统一处理 Intra-DC 和 Inter-DC 流量，并针对不同 RTT 特性进行优化。它通过幻影队列机制，有效地解决了 Intra-DC 和 Inter-DC 流量之间 BDP 差异带来的挑战，使得拥塞信号能够更早、更准确地传递，从而避免了物理队列的过度填充和高延迟。
    - **幻影队列的适应性**: 幻影队列的设计使得 Uno 能够更好地适应 RTT 差异巨大的网络环境。即使在 Inter-DC RTT 远大于 Intra-DC RTT 的情况下，幻影队列也能确保物理队列保持低占用，从而为延迟敏感的 Intra-DC 流量提供低延迟保证，同时有效地管理 Inter-DC 流量的拥塞。

### 核心结论

**一句话总结：图 11 通过在不同 Inter-DC RTT 与 Intra-DC RTT 比例下，与现有协议 Gemini 和 MPRDMA+BBR 在流完成时间减缓比上的对比，有力证明了 Uno 在处理 RTT 差异巨大的混合流量场景下，能够显著降低 FCT 减缓比，从而验证了 Uno 协议在应对数据中心内部和跨数据中心通信中 RTT 异构性方面的卓越适应性和性能优势，是 Uno 协议核心设计理念（特别是幻影队列）有效性的关键实验证据。**


## 图 12: Uno 在不同队列容量下的性能对比

<img src="https://files.manuscdn.com/user_upload_by_module/session_file/310519663384251806/dujdWPGtsJHtZfvU.webp" width="600"/>

### 基本要素

- **图 12** 包含六个子图，展示了在 40% 网络负载下，当数据中心内部和跨数据中心队列容量不同时，Uno 与其他基线协议在三种流量类型（All Flows, Inter-DC Flows, Intra-DC Flows）的平均流完成时间（Avg FCT）和 99 百分位流完成时间（P99 FCT）上的性能对比。
    - **X轴**: 协议类型。
    - **Y轴**: 流完成时间（FCT），单位为毫秒（ms）或微秒（µs）。
    - **图例 (Legend)**:
        - **Uno+ECMP**: Uno 协议结合 ECMP 负载均衡。
        - **Uno**: 完整的 Uno 协议（包含 UnoLB 和 EC）。
        - **Gemini**: 基线协议。
        - **MPRDMA+BBR**: 基线协议。

### 核心趋势

- **整体趋势**: 在 Intra-DC 使用浅缓冲区（shallow-buffered switches）和 Inter-DC 使用深缓冲区（deep-buffered switches）的场景下，Uno 协议（包括 Uno 和 Uno+ECMP）在所有流量类型（All Flows, Inter-DC Flows, Intra-DC Flows）的 Avg FCT 和 P99 FCT 上均显著优于 Gemini 和 MPRDMA+BBR。
- **Uno 的优势**: Uno 在 Inter-DC Flows 上的性能提升尤为显著，无论是平均 FCT 还是 P99 FCT，都远低于其他基线协议。对于 Intra-DC Flows，Uno 也能保持较低的 FCT，并且 P99 FCT 也有明显改善。
- **Uno 与 Uno+ECMP 的比较**: 完整的 Uno 协议（包含 UnoLB 和 EC）在大多数情况下表现优于 Uno+ECMP，尤其是在 Inter-DC Flows 的 P99 FCT 上，这表明 UnoLB 和 EC 在复杂队列容量配置下的有效性。

### 对比分析

- **与 Gemini 和 MPRDMA+BBR 对比**: Uno 在不同队列容量配置下，特别是 Intra-DC 浅缓冲区和 Inter-DC 深缓冲区的混合场景中，展现出卓越的性能。这表明 Uno 能够有效地适应异构的队列环境，并提供更低的延迟和更稳定的性能。
- **优势原因**: Uno 取得这些优势的核心机制在于其**幻影队列**和**UnoRC 的可靠性与效率**。
    - **幻影队列的适应性**: 幻影队列使得 UnoCC 能够有效地管理 Intra-DC 浅缓冲区和 Inter-DC 深缓冲区的混合场景。通过幻影队列，Uno 能够在不实际占用物理缓冲区的情况下，实现“近零排队”，从而为延迟敏感的 Intra-DC 流量提供低延迟保证。同时，它也能有效地处理 Inter-DC 流量，即使在深缓冲区环境下也能保持良好的拥塞控制。
    - **UnoRC 的可靠性与效率**: UnoRC 通过纠删码和子流负载均衡，显著提高了跨数据中心链路的可靠性和效率。在深缓冲区环境下，虽然缓冲区容量大，但如果拥塞控制不当，仍然可能导致高延迟。UnoRC 的机制能够确保流量在多路径上的高效分布，并减少丢包重传，从而在不同队列容量下都能维持低 FCT。

### 核心结论

**一句话总结：图 12 通过在数据中心内部和跨数据中心队列容量差异化的场景下，与现有协议 Gemini 和 MPRDMA+BBR 在三种流量类型上的全面性能对比，有力证明了 Uno 在平均流完成时间（FCT）和 99 百分位流完成时间（P99 FCT）方面的显著优势，尤其是在 Inter-DC 流量和尾部延迟方面，从而验证了 Uno 协议在异构队列容量网络环境中的鲁棒性和高效性，是 Uno 协议适应复杂网络基础设施的关键实验证据。**


## 图 13: Uno 在不同故障场景和工作负载下的性能

<img src="https://files.manuscdn.com/user_upload_by_module/session_file/310519663384251806/fpHFzWRZJYJwfXNU.webp" width="600"/>

### 基本要素

- **图 13** 包含三个子图（A, B, C），展示了 Uno 在不同故障场景和工作负载下与基线协议的性能对比。
    - **X轴**: 不同的负载均衡和可靠性方案，包括 Spray, Spray+EC, UnoLB, UnoLB+EC, PLB, PLB+EC。
    - **Y轴**: 流完成时间（FCT），单位为毫秒（ms），或 AllReduce 运行时与理想运行时的比率。
    - **图例 (Legend)**: Max, P99, Mean, Min（在 A 和 B 子图中）。

- **图 13A (Link Failure)**: 在链路故障场景下，比较了不同方案的 FCT。
    - **X轴**: 负载均衡和可靠性方案。
    - **Y轴**: FCT (ms)。
- **图 13B (Random Drops)**: 在随机丢包场景下，比较了不同方案的 FCT。
    - **X轴**: 负载均衡和可靠性方案。
    - **Y轴**: FCT (ms)。
- **图 13C (Random Drops + Link Failure)**: 在随机丢包和链路故障的混合场景下，模拟 AI 训练工作负载，比较了不同方案的 AllReduce 运行时与理想运行时的比率。
    - **X轴**: 负载均衡和可靠性方案。
    - **Y轴**: AllReduce Runtime Over Ideal Time。

### 核心趋势

- **整体趋势**: 在所有故障场景下，Uno（特别是 UnoLB+EC）的性能均显著优于其他基线方案（Spray, PLB）。这表明 Uno 在应对网络故障和丢包方面具有强大的鲁棒性和恢复能力。
- **纠删码 (EC) 的影响**: 在所有场景中，引入纠删码（+EC）的方案通常比没有纠删码的方案表现更好，尤其是在 P99 FCT 和 AllReduce 运行时方面，这突出了纠删码在提高可靠性和降低延迟方面的关键作用。
- **UnoLB 的优势**: UnoLB（子流负载均衡）在性能上明显优于 Spray 和 PLB，这表明 UnoLB 在多路径利用和故障适应性方面的有效性。

### 对比分析

- **与 Spray 和 PLB 对比**: Uno 在各种故障场景下都展现出卓越的性能。传统的负载均衡方案（如 Spray 和 PLB）在面对链路故障或随机丢包时，性能下降明显，尤其是在尾部延迟（P99 FCT）方面。而 Uno 通过其 UnoRC 组件中的纠删码和子流负载均衡机制，能够有效地缓解这些问题。
- **优势原因**: Uno 取得这些优势的核心机制在于其**UnoRC 的可靠性与效率**。
    - **纠删码 (EC)**: 纠删码通过引入冗余信息，使得 Uno 能够在部分数据包丢失的情况下，无需等待重传即可恢复原始数据。这在高延迟、易丢包的跨数据中心环境中尤为重要，因为它显著减少了消息完成时间，并提高了整体可靠性。
    - **子流负载均衡 (UnoLB)**: UnoLB 能够将流量分散到多条路径上，从而降低了单点故障的影响。此外，UnoLB 还能自适应地避免有问题的链路，进一步提高了网络的鲁棒性。在故障场景下，UnoLB 结合纠删码，能够更有效地利用可用资源，确保数据传输的连续性和低延迟。
    - **AI 训练工作负载**: 在 AI 训练场景中，AllReduce 操作对延迟和带宽都非常敏感。Uno 在此场景下的优异表现（比第二好的算法快 2 倍以上，仅比理想情况慢 30%）证明了其在高性能计算和分布式应用中的潜力。

### 核心结论

**一句话总结：图 13 通过在链路故障、随机丢包以及 AI 训练工作负载等多种故障场景下，与现有负载均衡和可靠性方案的全面对比，有力证明了 Uno（特别是结合纠删码的 UnoLB）在显著降低流完成时间（FCT）和提高 AllReduce 运行时效率方面的卓越性能，从而验证了 Uno 协议在复杂、不可靠网络环境中提供高可靠性和低延迟的关键能力，是 Uno 协议在实际部署中应对网络挑战的决定性实验证据。**

