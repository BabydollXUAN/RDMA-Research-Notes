这份手册旨在帮助初学者（尤其是研究 **Wide-Area RDMA** 等高性能网络方向的同学）从零开始，快速在 CENI（未来网络试验设施）平台上搭建起高效的科研环境。
## 🛰️ 第一阶段：建立远程连接 (SSH & FRP)

CENI 服务器通常部署在内网，需要通过特定的跳板机或 FRP 转发进行访问。

### 1. 配置本地 SSH `config` 

用VS Code 编辑`config` 的文件（无后缀名），填入以下模板：

代码段

```
Host CENI_Server_Name
    HostName 202.38.68.71  # 替换为实际公网 IP
    User ustc              # 初始用户通常为 ustc 或 root
    Port 12345             # 替换为分配给你的转发端口
```

### 2. 解决 `root` 登录权限 (重要)

如果需要 `root` 权限但 SSH 提示被拒，请先用 `ustc` 登录，在网页控制台执行：

- **修改配置**：`sudo sed -i 's/#PermitRootLogin prohibit-password/PermitRootLogin yes/g' /etc/ssh/sshd_config`。
    
- **重启服务**：`sudo service ssh restart`。
    

### 3.为什么 SSH 连接提示 "Permission denied" 或密码错误？**

- **原因 1**：Ubuntu 18.04 默认禁止 `root` 用户通过密码远程登录。
    
- **原因 2**：用户名输入错误（用了 `ustc` 登录但密码是 `root` 的，或者反之）。
    
- **解决方案**：
    
    1. 在网页控制台执行：`sudo sed -i 's/#PermitRootLogin prohibit-password/PermitRootLogin yes/g' /etc/ssh/sshd_config`。
        
    2. 确保 `PasswordAuthentication yes` 已开启。
        
    3. 重启服务：`sudo service ssh restart`。

---

## 💾 第二阶段：磁盘空间拓容 (150GB+ 挂载)

CENI 的 N1 实例默认系统盘仅 **20GB**，绝大部分空间隐藏在未挂载的分区中。

### 1. 挂载数据盘

在终端输入以下命令，将大容量分区（通常是 `/dev/vda4`）挂载到你的用户目录下：

Bash

```
# 创建挂载点
mkdir -p ~/autodl-tmp
# 挂载分区
sudo mount /dev/vda4 ~/autodl-tmp
# 修改所有权，确保 VS Code 拥有读写权
sudo chown -R $USER:$USER ~/autodl-tmp
```

### 2. 设置开机自动挂载

为了防止重启后数据“消失”，必须写入系统表：

Bash

```
sudo sh -c 'echo "/dev/vda4 /home/ustc/autodl-tmp ext4 defaults 0 0" >> /etc/fstab'
```

---

## 🛠️ 第三阶段：科研软件环境搭建

### 1. 编译器升级 (GCC 9.4.0+)

高性能网络协议通常需要 **C++17** 支持。

- **检查版本**：`gcc --version`。
    
- **安装基础套件**：`sudo apt update && sudo apt install -y build-essential cmake`。
    

### 2. RDMA 开发库安装

这是进行 **Wide-Area RDMA** 实验的基础：

Bash

```
sudo apt install -y libibverbs-dev librdmacm-dev rdma-core ibverbs-utils
```

### 3. Python 环境修复 (针对 Ubuntu 18.04)

如果系统自带 Python 3.6 无法满足绘图库要求，请安装 **Python 3.8**：

- **安装**：`sudo add-apt-repository ppa:deadsnakes/ppa && sudo apt install python3.8`。
    
- **安装 pip**：手动上传 `get-pip.py` 并运行 `python3.8 get-pip.py --user`。
    

---

## 📂 第四阶段：代码与数据同步

### 1. WinSCP 文件搬运

由于 CENI 服务器连接 GitHub 可能存在超时风险，建议使用 **WinSCP** 工具：

- **左侧**：本地 Windows 目录。
    
- **右侧**：服务器目录（务必进入 `/home/ustc/autodl-tmp` 避免撑爆系统盘）。
    
- **操作**：直接拖拽 ZIP 包至右侧，再在终端解压。
    

### 2. Git 镜像克隆

如果必须在终端下载，推荐使用 `ghproxy` 镜像：

Bash

```
git clone https://mirror.ghproxy.com/https://github.com/用户名/仓库名.git
```

---

## 📝 核心参数速查表

|**项目**|**典型值/路径**|**备注**|
|---|---|---|
|**系统版本**|Ubuntu 18.04.6 LTS|见 `cat /etc/os-release`|
|**大硬盘路径**|`/home/ustc/autodl-tmp`|必须在这里跑仿真|
|**默认用户**|`ustc` / `root`|初始密码常见为 `ustc1958`|
|**RDMA 核心库**|`libibverbs`|实验代码编译必备|

---


**未来网络试验服务平台**（由中国科大承建）旨在为个人用户提供一站式的网络试验环境。用户可在线快速申请网络、计算与存储资源，构建灵活的仿真环境。
平台以“**试验**”为核心单元，通过承载虚拟机和链路资源，辅助用户完成技术验证。资源随试验的创建而开启，随试验的结束而回收。试验中的“**节点**”即虚拟服务器，涵盖了从通用 CPU 到高性能 GPU 的全套配置，用户可根据业务需求灵活调整参数，以获取匹配的计算与存储能力。
最近连ceni服务器，遇到了一些困难，随手写下Q&A文档供大家参考。
![[Pasted image 20260211182051.png]]
不用172.16.0.0 连不上外网 必须设置成172.16.0.0
那个20.04疑似是被别人改过环境的 专门用来做bmv2实验的

每次重启后，都要在网页版重新登陆，cd frp_0.42.0_linux_amd64 
./frpc -c ./frpc.ini