RDMA, Datacenter Interconnection, Loss Recovery, Programmable Switches
故障恢复、可编程交换机
**Uno和LR2可以形成互补：Uno侧重降低RDMA丢包率，属于预防类机制，而LR2专注于丢包后的高效恢复，属于处理类方案。**

在网络协议，尤其像RDMA这种需要可靠、有序传输的协议，**PSN 实际收到的数据包的序号** 和**ePSN 期待的接收下一个数据包的序号** 是判断数据包顺序的关键。

1.包序号是数据包自身携带的信息。接收端Depot交换机直接从每个传入数据包的包头获取包序号，利用它来识别包的顺序和状态，并进行相应的处理以实现丢包恢复。
2.与ePSN比较：Depot会将收到的数据包的PSN与它为该数据流维护的ePSN进行比较。
**按序到达**：如果收到的PSN等于ePSN，则表示这是一个按序到达的包。Depot会接受该包，并更新ePSN为当前包的PSN加上其长度，以表示下一个预期的包序号。
**乱序到达**：如果收到的PSN大于ePSN，则表示这是一个乱序包。Depot会根据ePSN(以及辅助寄存器ePSN_D，用于跟踪乱序窗口的上限)来检测丢失的包，并生成负确认（NACK）发送给发送端。
**重复或丢包**：Depot会将其丢弃或进行特殊处理（例如，如果是RNIC超时重传的旧包，则会重置状态以重新对齐）。


引用：
Bine Trees: Enhancing Collective Operations by Optimizing Communication Locality