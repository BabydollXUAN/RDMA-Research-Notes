#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <isa-l.h> // 引入ISA-L头文件

// 定义配置：4个数据块 + 2个校验块
#define K 4
#define P 2
#define DATA_LEN 1024 // 每个块的大小 (1KB)

int main() {
    int i, j;
    
    // ==========================================
    // 1. 内存分配
    // ==========================================
    
    // 定义指针数组，用来存放指向各个数据块的指针
    unsigned char *frag_ptrs[K];      // 指向数据块
    unsigned char *recover_outp[P];   // 指向校验块 (结果存放地)
    
    // 分配实际的内存空间
    for (i = 0; i < K; i++) {
        frag_ptrs[i] = malloc(DATA_LEN);
        // 填充一些假数据，方便观察 (例如 'A', 'B', 'C', 'D')
        memset(frag_ptrs[i], 'A' + i, DATA_LEN);
        printf("[数据块 %d] 准备完毕, 内容首字符: %c\n", i, frag_ptrs[i][0]);
    }
    
    for (i = 0; i < P; i++) {
        recover_outp[i] = malloc(DATA_LEN);
        // 校验块先清零
        memset(recover_outp[i], 0, DATA_LEN);
    }

    // ==========================================
    // 2. 初始化矩阵 (SDR-RDMA 论文中的数学部分)
    // ==========================================
    
    unsigned char *encode_matrix = malloc(K * (K + P));
    unsigned char *g_tbls = malloc(K * P * 32); // ISA-L需要的各种查找表缓存

    // 生成 Reed-Solomon 编码矩阵 (Vandermonde matrix)
    gf_gen_rs_matrix(encode_matrix, K + P, K);
    
    // 关键点：ISA-L使用倒置矩阵的后P行来做编码
    // 初始化编码表 (为了AVX-512加速，必须做这一步)
    ec_init_tables(K, P, &encode_matrix[K * K], g_tbls);

    // ==========================================
    // 3. 执行编码 (性能关键路径)
    // ==========================================
    
    printf("\n>>> 开始执行 EC 编码 (ec_encode_data)...\n");
    
    // 这里的参数解释：
    // DATA_LEN: 每个块多大
    // K: 源数据块数量
    // P: 校验块数量
    // g_tbls: 刚才初始化的加速表
    // frag_ptrs: 源数据在哪里
    // recover_outp: 结果存哪里
    ec_encode_data(DATA_LEN, K, P, g_tbls, frag_ptrs, recover_outp);
    
    printf(">>> 编码完成！\n\n");

    // ==========================================
    // 4. 验证结果
    // ==========================================
    
    // 打印校验块的前几个字节看看是否生成了东西
    for (i = 0; i < P; i++) {
        printf("[校验块 %d] 生成内容 (Hex): %02x %02x %02x %02x ...\n", 
               i, recover_outp[i][0], recover_outp[i][1], recover_outp[i][2], recover_outp[i][3]);
    }

    printf("\n成功：你刚刚复现了 SDR-RDMA 的数据包编码过程。\n");

    // 释放内存 (略)
    return 0;
}