#ifndef _GLOBAL_H_
#define _GLOBAL_H_

#include <netinet/in.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "global.h"
#include <pthread.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include "signal.h"
//sudo tc qdisc add dev enp0s8 root netem loss 20% delay 200ms

// 单位是byte
#define SIZE32 4
#define SIZE16 2
#define SIZE8  1

// 一些Flag
#define NO_FLAG 0
#define NO_WAIT 1
#define TIMEOUT 2
#define TRUE 1
#define FALSE 0

// 定义最大包长 防止IP层分片
#define MAX_DLEN 1375 	// 最大包内数据长度
#define MAX_LEN 1400 	// 最大包长度

// TCP socket 状态定义
#define CLOSED 0
#define LISTEN 1
#define SYN_SENT 2
#define SYN_RECV 3
#define ESTABLISHED 4
#define FIN_WAIT_1 5
#define FIN_WAIT_2 6
#define CLOSE_WAIT 7
#define CLOSING 8
#define LAST_ACK 9
#define TIME_WAIT 10

// TCP 拥塞控制状态
#define SLOW_START 0
#define CONGESTION_AVOIDANCE 1
#define FAST_RECOVERY 2

// TCP 接受窗口大小
#define TCP_RECVWN_SIZE 32*MAX_DLEN // 比如最多放32个满载数据包
#define TCP_BUF_SIZE 1024*MAX_DLEN//TCP缓冲区大小

//TCP 半连接队列和全连接队列长度
#define QUEUE_LEN 10


//时间粒度1毫秒
#define TIME_SCALE 1000
#define INITIAL_RTO 10000

typedef struct tju_tcp_t tju_tcp_t;





// TCP 窗口 每个建立了连接的TCP都包括发送和接受两个窗口
// typedef struct {
// 	sender_window_t* wnd_send;
//   	receiver_window_t* wnd_recv;
// } window_t;

typedef struct {
	uint32_t ip;
	uint16_t port;
} tju_sock_addr;

typedef struct timer
{
	int now_time;
	int set_time;
	void (*func)(tju_tcp_t* sock);	
}timer;

typedef struct skb_node
{
	char *data;
	int len;
	int seq;
	int flag;
	struct skb_node* next;
	struct skb_node* prev;
	struct timer* skb_timer; 
	struct timeval send_time;
}skb_node;//缓冲区节点

typedef struct send_queue
{
	skb_node* head;
	skb_node* tail;
}send_queue;//发送缓冲区

typedef struct recv_skb_node
{
	char *data;
	int len;
	int seq;
	int flag;
	struct recv_skb_node* next;
	struct recv_skb_node* prev;
}recv_skb_node;//缓冲区节点

typedef struct recv_queue
{
	recv_skb_node* head;
	recv_skb_node* tail;
}recv_queue;//发送缓冲区

// TCP 发送窗口
// 注释的内容如果想用就可以用 不想用就删掉 仅仅提供思路和灵感
typedef struct {
	uint16_t window_size;
	uint16_t rwnd; 
	uint16_t cwnd; 
	uint32_t nextseq;
	uint32_t base;//未确认数据包
	int wait_for_ack;//等待确认的数据大小
	timer* persist_timer;
	int ack_cnt;//重复ACK数
	uint32_t estmated_rtt;
	uint32_t rtt_var;
	
//   
//   pthread_mutex_t ack_cnt_lock;
//   
//   struct timeval timeout;
   	
//   int congestion_status;
	
//   uint16_t ssthresh; 
} sender_window_t;

// TCP 接受窗口
// 注释的内容如果想用就可以用 不想用就删掉 仅仅提供思路和灵感
typedef struct {
	uint32_t expect_seq;
	recv_queue* window;
	int recv_len;
	//char received[TCP_RECVWN_SIZE];

//   received_packet_t* head;
//   char buf[TCP_RECVWN_SIZE];
//   uint8_t marked[TCP_RECVWN_SIZE];

} receiver_window_t;



// TJU_TCP 结构体 保存TJU_TCP用到的各种数据
struct tju_tcp_t{
	int state; // TCP的状态

	tju_sock_addr bind_addr; // 存放bind和listen时该socket绑定的IP和端口
	tju_sock_addr established_local_addr; // 存放建立连接后 本机的 IP和端口
	tju_sock_addr established_remote_addr; // 存放建立连接后 连接对方的 IP和端口

	pthread_mutex_t send_lock; // 发送数据锁
	send_queue* sending_buf; // 发送数据缓存区
	skb_node* send_head;//待发送数据头
	int sending_len; // 发送数据缓存长度

	pthread_mutex_t recv_lock; // 接收数据锁
	recv_queue* received_buf; // 接收数据缓存区
	int received_len; // 接收数据缓存长度

	pthread_cond_t wait_cond; // 可以被用来唤醒recv函数调用时等待的线程

	//window_t window; // 发送和接受窗口
	sender_window_t* wnd_send;
  	receiver_window_t* wnd_recv;


};

typedef struct{
	tju_tcp_t* syn_queue[QUEUE_LEN];
	tju_tcp_t* accept_queue[QUEUE_LEN];
	int sys_len;
	int accept_len;

}tju_queue;

#endif