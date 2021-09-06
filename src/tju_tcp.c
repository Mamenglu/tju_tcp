#include "tju_tcp.h"


/*
创建 TCP socket 
初始化对应的结构体
设置初始状态为 CLOSED
*/
tju_tcp_t* tju_socket(){
    tju_tcp_t* sock = (tju_tcp_t*)malloc(sizeof(tju_tcp_t));
    sock->state = CLOSED;
    
    pthread_mutex_init(&(sock->send_lock), NULL);
    sock->sending_buf = (send_queue*)malloc(sizeof(send_queue));
    sock->send_head=NULL;
    sock->sending_len = 0;

    pthread_mutex_init(&(sock->recv_lock), NULL);
    sock->received_buf = (recv_queue*)malloc(sizeof(recv_queue));
    sock->received_len = 0;
    
    if(pthread_cond_init(&sock->wait_cond, NULL) != 0){
        perror("ERROR condition variable not set\n");
        exit(-1);
    }
    sock->wnd_send = (sender_window_t*)malloc(sizeof(sender_window_t));

    sock->wnd_send->rwnd =TCP_RECVWN_SIZE;
    sock->wnd_send->cwnd=MAX_DLEN;
    sock->wnd_send->window_size=((sock->wnd_send->cwnd) < (sock->wnd_send->rwnd) ) ? (sock->wnd_send->cwnd) : sock->wnd_send->rwnd;
    printf("\n窗口初始化%d\n",sock->wnd_send->window_size);
    sock->wnd_send->nextseq=0;
    sock->wnd_send->base=0;
    sock->wnd_send->persist_timer=(timer*)malloc(sizeof(timer));
    sock->wnd_send->ack_cnt=0;
    sock->wnd_recv = (receiver_window_t*)malloc(sizeof(receiver_window_t));
    sock->wnd_recv->expect_seq=0;
    sock->wnd_recv->recv_len=0;
    sock->wnd_recv->window=(recv_queue*)malloc(sizeof(recv_queue));
    sock->wnd_send->estmated_rtt=0;
    sock->wnd_send->rtt_var=0;
    sock->wnd_send->timeout=INITIAL_RTO;
    sock->wnd_send->congestion_status=SLOW_START;
    sock->wnd_send->ssthresh=16 * MAX_DLEN;
    

    struct itimerval timeval;
    timeval.it_value.tv_sec = 0;
    timeval.it_value.tv_usec = TIME_SCALE;
    timeval.it_interval.tv_sec = 0;
    timeval.it_interval.tv_usec = TIME_SCALE;
    setitimer(ITIMER_REAL , &timeval , NULL ); 
    signal( SIGALRM , signalhandler );

    return sock;
}

/*
绑定监听的地址 包括ip和端口
*/
int tju_bind(tju_tcp_t* sock, tju_sock_addr bind_addr){
    sock->bind_addr = bind_addr;
    return 0;
}

/*
被动打开 监听bind的地址和端口
设置socket的状态为LISTEN
注册该socket到内核的监听socket哈希表
*/
int tju_listen(tju_tcp_t* sock){
    sock->state = LISTEN;
    int hashval = cal_hash(sock->bind_addr.ip, sock->bind_addr.port, 0, 0);
    listen_socks[hashval] = sock;

    //为监听的socket分配半连接和全连接队列
    tju_queue* nw_queue=(tju_queue*)malloc(sizeof(tju_queue));
    sock_queue[hashval]=nw_queue;

    printf("监听sockrt的hashval为 %d",hashval);
    return 0;
}

/*
接受连接 
返回与客户端通信用的socket
这里返回的socket一定是已经完成3次握手建立了连接的socket
因为只要该函数返回, 用户就可以马上使用该socket进行send和recv
*/
tju_tcp_t* tju_accept(tju_tcp_t* listen_sock){
    /*
    tju_tcp_t* new_conn = (tju_tcp_t*)malloc(sizeof(tju_tcp_t));
    memcpy(new_conn, listen_sock, sizeof(tju_tcp_t));

    tju_sock_addr local_addr, remote_addr;*/
    /*
     这里涉及到TCP连接的建立
     正常来说应该是收到客户端发来的SYN报文
     从中拿到对端的IP和PORT
     换句话说 下面的处理流程其实不应该放在这里 应该在tju_handle_packet中
    */ 
   /*
    remote_addr.ip = inet_network("10.0.0.2");  //具体的IP地址
    remote_addr.port = 5678;  //端口

    local_addr.ip = listen_sock->bind_addr.ip;  //具体的IP地址
    local_addr.port = listen_sock->bind_addr.port;  //端口

    new_conn->established_local_addr = local_addr;
    new_conn->established_remote_addr = remote_addr;

    // 这里应该是经过三次握手后才能修改状态为ESTABLISHED
    //new_conn->state = ESTABLISHED;
    

    // 将新的conn放到内核建立连接的socket哈希表中
    int hashval = cal_hash(local_addr.ip, local_addr.port, remote_addr.ip, remote_addr.port);
    established_socks[hashval] = new_conn;

    // 如果new_conn的创建过程放到了tju_handle_packet中 那么accept怎么拿到这个new_conn呢
    // 在linux中 每个listen socket都维护一个已经完成连接的socket队列
    // 每次调用accept 实际上就是取出这个队列中的一个元素
    // 队列为空,则阻塞 
    return new_conn;*/

        int listen_hashval = cal_hash(listen_sock ->bind_addr.ip , listen_sock -> bind_addr.port
                                    ,0,0);
    printf("accept hashval is : %d \n",listen_hashval);
    while (1)
    {
    // 如果new_conn的创建过程放到了tju_handle_packet中 那么accept怎么拿到这个new_conn呢
    // 在linux中 每个listen socket都维护一个已经完成连接的socket队列
    // 每次调用accept 实际上就是取出这个队列中的一个元素
    // 队列为空,则阻塞 
    if(sock_queue[listen_hashval] -> accept_queue[0] != NULL){
            //拿出一个socket(已经建立好连接)
            tju_tcp_t* return_socket = sock_queue[listen_hashval] -> accept_queue[0];
            int return_sock_hashval = cal_hash( return_socket -> established_local_addr.ip , return_socket -> established_local_addr.port
                                            ,return_socket -> established_remote_addr.ip , return_socket -> established_remote_addr.port);
            printf("建立在服务器上的Socket的hash为： %d \n",return_sock_hashval);
            //把这个socket注册到EST里面,这里的hash为新socket的hash
            established_socks[return_sock_hashval] = return_socket;
            sock_queue[listen_hashval] -> accept_queue[0] = NULL;
            

            pthread_t send_thread_id = 2000;
            void *arg = malloc(sizeof(&return_sock_hashval));
            memcpy(arg, &return_sock_hashval, sizeof((&return_sock_hashval)));
            int rst = pthread_create(&send_thread_id, NULL, send_thread, arg);
            return return_socket;
        }
    }
}


/*
连接到服务端
该函数以一个socket为参数
调用函数前, 该socket还未建立连接
函数正常返回后, 该socket一定是已经完成了3次握手, 建立了连接
因为只要该函数返回, 用户就可以马上使用该socket进行send和recv
*/
int tju_connect(tju_tcp_t* sock, tju_sock_addr target_addr){

    sock->established_remote_addr = target_addr;

    tju_sock_addr local_addr;
    local_addr.ip = inet_network("10.0.0.2");
    local_addr.port = 5678; // 连接方进行connect连接的时候 内核中是随机分配一个可用的端口
    sock->established_local_addr = local_addr;

        // 将建立了连接的socket放入内核 已建立连接哈希表中
    int hashval = cal_hash(local_addr.ip, local_addr.port, target_addr.ip, target_addr.port);
    established_socks[hashval] = sock;
    printf("客户端注册端口%d\n",hashval);

    // 这里也不能直接建立连接 需要经过三次握手
    // 实际在linux中 connect调用后 会进入一个while循环
    // 循环跳出的条件是socket的状态变为ESTABLISHED 表面看上去就是 正在连接中 阻塞
    // 而状态的改变在别的地方进行 在我们这就是tju_handle_packet
    //sock->state = ESTABLISHED;
    /*char* create_packet_buf(uint16_t src, uint16_t dst, uint32_t seq, uint32_t ack,
    uint16_t hlen, uint16_t plen, uint8_t flags, uint16_t adv_window, 
    uint8_t ext, char* data, int len);*/
    
    uint32_t seq=0;
    char* msg_syn=create_packet_buf(local_addr.port, target_addr.port, seq, 0,DEFAULT_HEADER_LEN,DEFAULT_HEADER_LEN,0x8,0, 
    0,NULL,0);////connnect 发送SYN报文
    sendToLayer3(msg_syn,DEFAULT_HEADER_LEN);
    sock->state=SYN_SENT;
    printf("发送SYN，开始第一次握手\n");
    while(sock->state!=ESTABLISHED){

    }//进入establish状态前阻塞

    printf("ESTABLISHED!\n");
    printf("建立在客户端上的socket %d\n",cal_hash(sock->established_local_addr.ip,sock->established_local_addr.port,sock->established_remote_addr.ip,sock->established_remote_addr.port));

                pthread_t send_thread_id = 2000;
            void *arg = malloc(sizeof(&hashval));
            memcpy(arg, &hashval, sizeof((&hashval)));
            int rst = pthread_create(&send_thread_id, NULL, send_thread, arg);
            //return return_socket;

    return 0;
}

int tju_send(tju_tcp_t* sock, const void *buffer, int len){
    // 这里当然不能直接简单地调用sendToLayer3
    
    if(sock->state!=ESTABLISHED&&sock->state!=CLOSE_WAIT){//其它状态不会在发送信息
        return -1;
    }
    
    char* data = malloc(len);
    memcpy(data, buffer, len);
    
    int i=0;
    while(i<len){//信息没有完全发送
        
        char* send_data=data+i;
        //printf("%s",data+i);
        int send_len=len-i > MAX_DLEN ? MAX_DLEN : len-i;
        i+=send_len;
        if(sock->sending_len+send_len<TCP_BUF_SIZE){
            add_to_send_buf(sock,send_data,send_len,NO_FLAG);
            //printf("\n问题1\n");
            // pthread_mutex_lock(&sock->send_lock);
            // skb_node* nw_node=(skb_node*)malloc(sizeof(skb_node));
            // nw_node->data=send_data;
            // nw_node->len=len;
            
            // nw_node->next=NULL;
            // nw_node->prev=NULL;
            // nw_node->skb_timer=NULL;

            // //printf("\n问题2\n");

            // if(sock->sending_buf->head==NULL){//缓冲区为空
            //     //printf("\n问题3\n");
            //     sock->sending_buf->head=nw_node;
            //     sock->sending_buf->tail=nw_node;
            //     sock->send_head=sock->sending_buf->head;
            //     sock->sending_len+=send_len;
            //     //printf("\n放入发送缓冲区%s\n",nw_node->data);
                

            // }
            // else{
            //     //printf("\n问题4\n");
            //     sock->sending_buf->tail->next=nw_node;
            //     nw_node->prev=sock->sending_buf->tail;
            //     //nw_node->next=sock->sending_buf->head;
            //     sock->sending_buf->tail=nw_node;
            //     if(sock->send_head==NULL){
            //         sock->send_head=nw_node;
            //     }
            //     sock->sending_len+=send_len;
            //     //printf("\n第二次放入发送缓冲区%s\n",nw_node->data);
            // }
            // //printf("\n问题5\n");
            // //send_one(sock);
            // pthread_mutex_unlock(&sock->send_lock);
            


        }
        else{
            printf("\n发送缓冲区满\n");
        }
        
        //printf("\n发送完成\n");
    }
    
    return 0;
}
int tju_recv(tju_tcp_t* sock, void *buffer, int len){
    //printf("\n阻塞\n");
    while(sock->received_len<=0){
        // 阻塞
        
    }
    //printf("\n问题1\n");

    while(pthread_mutex_lock(&(sock->recv_lock)) != 0); // 加锁
    //printf("\n获取锁\n");
    int now_len=0;
    //while (sock->received_len>0)
    //{
        //printf("\n问题2\n");
        recv_skb_node* nw_node=sock->received_buf->head;
        if(now_len+nw_node->len<=len){
            //printf("\n读数据%d\n",nw_node->len);
            memcpy(buffer, nw_node->data, nw_node->len);
            //printf("\n%p\n",buffer);
            now_len+=nw_node->len;
            sock->received_buf->head=sock->received_buf->head->next;
            sock->received_len-=nw_node->len;
            //printf("\n读完成\n");
        }
        else{
            
            int read_len=now_len+nw_node->len-len;
            printf("\n第二次读%d\n",read_len);
            memcpy(buffer, nw_node->data, read_len);
            char * nw_buffer=malloc(nw_node->len-read_len);
            memcpy(nw_buffer, nw_node->data+read_len, nw_node->len-read_len);
            nw_node->next=sock->received_buf->head->next;
            sock->received_buf->head=nw_node;
            sock->received_len-=read_len;
            now_len+=read_len;
            //printf("\n第二次读完成\n");

        }
        free(nw_node);
        //printf("\n问题5\n");

        
    //}
    //printf("\n接收完成\n");
    

    pthread_mutex_unlock(&(sock->recv_lock)); // 解锁

    return 0;
}

int tju_handle_packet(tju_tcp_t* sock, char* pkt){
    
    uint32_t data_len = get_plen(pkt) - DEFAULT_HEADER_LEN;

    // 把收到的数据放到接受缓冲区
    // while(pthread_mutex_lock(&(sock->recv_lock)) != 0); // 加锁

    // if(sock->received_buf == NULL){
    //     sock->received_buf = malloc(data_len);
    // }else {
    //     sock->received_buf = realloc(sock->received_buf, sock->received_len + data_len);
    // }
    // memcpy(sock->received_buf + sock->received_len, pkt + DEFAULT_HEADER_LEN, data_len);
    // sock->received_len += data_len;

    // pthread_mutex_unlock(&(sock->recv_lock)); // 解锁

    int listen_hashval=cal_hash(sock->bind_addr.ip,sock->bind_addr.port,0,0);

    //判断收到数据包的标志位
    int is_test=(get_flags(pkt)&TEST_FLAG_MASK)>>4;
    int is_syn=(get_flags(pkt)&SYN_FLAG_MASK)>>3;
    int is_ack=(get_flags(pkt)&ACK_FLAG_MASK)>>2;
    int is_fin=(get_flags(pkt)&FIN_FLAG_MASK)>>1;

    uint32_t seq=get_seq(pkt);
    uint32_t ack=get_ack(pkt);
    uint32_t len=get_plen(pkt);
    int flags=get_flags(pkt);

    switch(sock->state){
        case LISTEN://服务端，收到SYN=1，ACK=0
            if(is_syn&&(!is_ack)){
                printf("收到SYN\n");
                sock->state=SYN_RECV;

                //建立一个新的socket,放入半连接队列
                tju_tcp_t* nw_sock=tju_socket();

                nw_sock -> established_local_addr.ip=inet_network("10.0.0.1");
                nw_sock -> established_local_addr.port=get_dst(pkt);
                nw_sock -> established_remote_addr.ip   = inet_network("10.0.0.2");
                nw_sock -> established_remote_addr.port = get_src(pkt);

                nw_sock->state=SYN_RECV;//
                printf("新sockt创建完成\n");

                //int cal_hash(uint32_t local_ip, uint16_t local_port, uint32_t remote_ip, uint16_t remote_port)

                //int hashval=cal_hash(nw_sock -> established_local_addr.ip,nw_sock -> established_local_addr.port,nw_sock -> established_remote_addr.ip,nw_sock -> established_remote_addr.port);

                //将新建的socket放入半连接队列，使用监听socket的hashval
                sock_queue[listen_hashval] -> syn_queue[0]=nw_sock;
                uint32_t seq=2000;
                uint32_t ack=get_seq(pkt)+1;
                char* msg_syn_ack=create_packet_buf(get_dst(pkt),get_src(pkt),seq,ack,DEFAULT_HEADER_LEN,DEFAULT_HEADER_LEN,0xC,0, 
                0,NULL,0);////connnect 发送SYN报文
                sendToLayer3(msg_syn_ack,DEFAULT_HEADER_LEN);
                printf("发送SYN_ACK包\n");
                
            }
            break;

        case SYN_SENT://客户端，收到SYN=1，ACK=1
            if(is_syn&&is_ack){
                printf("收到SYN+ACK\n");
                sock->state=ESTABLISHED;
                char* msg_ack=create_packet_buf(get_dst(pkt),get_src(pkt),get_ack(pkt),get_seq(pkt)+1,DEFAULT_HEADER_LEN,DEFAULT_HEADER_LEN,0x4,0,0,NULL,0);
                sendToLayer3(msg_ack,DEFAULT_HEADER_LEN);
                printf("发送ACK\n");
                
            }
            break;
        case SYN_RECV:
            if(is_ack){
                printf("三次握手完成\n");
    
                sock_queue[listen_hashval] -> accept_queue[0]=sock_queue[listen_hashval]->syn_queue[0];
                sock_queue[listen_hashval] -> syn_queue[0]=NULL;
                sock_queue[listen_hashval] -> accept_queue[0]->state=ESTABLISHED;

            }
            break;
           //四次挥手过程
        case ESTABLISHED:
            if(is_fin){

                sock->state = CLOSE_WAIT;
                printf("The server accepts a FIN message\n");
                add_to_send_buf(sock,NULL,0,0x6);
                // char* msg;
                // //第二次挥手
                // msg=create_packet_buf(get_dst(pkt),get_src(pkt),0,get_ack(pkt)+1,
                //                         DEFAULT_HEADER_LEN , DEFAULT_HEADER_LEN , 0x6 ,0,0,NULL,0);
                // sendToLayer3(msg,20);
                while (1){
                    //第三次挥手
                    if (sock->received_len == 0){     //接受数据缓存为0
                        sock->state = LAST_ACK;
                        printf("The acceptable data buffer on the server is 0 ,send a FIN&&ACK message\n");
                        char* wavehand;
                        wavehand = create_packet_buf(get_dst(pkt),get_src(pkt),0,get_seq(pkt)+1,
                                                    DEFAULT_HEADER_LEN , DEFAULT_HEADER_LEN , 0x6 ,0,0,NULL,0);
                        sendToLayer3(wavehand,20);
                        break;
                    }
                }
            }
            else if(is_test){
                char* pkt = create_packet_buf(sock->established_local_addr.port, sock->established_remote_addr.port,0,
		            sock->wnd_recv->expect_seq, DEFAULT_HEADER_LEN, DEFAULT_HEADER_LEN, ACK_FLAG_MASK, TCP_RECVWN_SIZE-sock->wnd_recv->recv_len, 
		                0, NULL, DEFAULT_HEADER_LEN);
		        sendToLayer3(pkt, DEFAULT_HEADER_LEN);

            }
            else if(is_ack){
                printf("\n收到一个ACK包%d\n",ack);
                //display_pkt(pkt);
                //流量控制
                //printf("\nget_advertised_window(pkt)为%d\n",get_advertised_window(pkt));
                sock->wnd_send->rwnd=get_advertised_window(pkt);
                sock->wnd_send->window_size=sock->wnd_send->cwnd < sock->wnd_send->rwnd ? sock->wnd_send->cwnd : sock->wnd_send->rwnd;
                //printf("\n窗口大小为%d\n",sock->wnd_send->window_size);
                //printf("\ncwnd为%d,rwnd为%d\n",sock->wnd_send->cwnd,sock->wnd_send->rwnd);
                if(get_advertised_window(pkt)==0){
                    sock->wnd_send->persist_timer=creat_timer(10000,persist);    
                    printf("\n创建坚持计时器完成\n");
                }
                else{
                    if(sock->wnd_send->persist_timer!=NULL){
                        //free(sock->wnd_send->persist_timer);
                    }
                }
                if(ack==sock->wnd_send->base){//重复ack
                    sock->wnd_send->ack_cnt++;
                    if(sock->wnd_send->ack_cnt>=3){
                        printf("\n快速重传\n");
                        sock->wnd_send->congestion_status = FAST_RECOVERY ;
                        retransmit(sock);
                        sock->wnd_send->ack_cnt=0;
                    }
                    printf("\n重复ACK\n");

                }
                else{//窗口推进
                    //printf("\n窗口推进1\n");

                    //计算rtt
                    struct timeval recv_time;
                    gettimeofday(&recv_time,NULL);
                    cal_rto(sock,ack,recv_time);

                    int add_len = ack - sock->wnd_send->base;
                    while((add_len > 0)&&(sock->sending_buf->head != NULL))

                    {
                        //printf("\n窗口推进2\n");
                        skb_node* send_node = sock->sending_buf->head;

                        sock->sending_len -= send_node->len;
                        sock->wnd_send->base += send_node->len+DEFAULT_HEADER_LEN;
                        sock->wnd_send->wait_for_ack+=send_node->len;
                        sock->sending_buf->head = send_node->next;
                        add_len-=send_node->len+DEFAULT_HEADER_LEN;
                        //printf("add_len %d,send_node_len %d",add_len,send_node->len);
                        free(send_node);
                        //printf("\n窗口推进3\n");
                    }
                    switch (sock->wnd_send->congestion_status)
                    {
                        case SLOW_START:
                            sock->wnd_send->cwnd=2 * sock->wnd_send->cwnd;
                            if(sock->wnd_send->cwnd >= sock->wnd_send->ssthresh){
                                sock->wnd_send->congestion_status=CONGESTION_AVOIDANCE;
                            }
                            break;
                        case CONGESTION_AVOIDANCE:
                            sock->wnd_send->cwnd += MAX_DLEN;
                            break;
                        case FAST_RECOVERY:
                            sock->wnd_send->ssthresh =( ( sock->wnd_send->cwnd / MAX_DLEN ) /2 ) * MAX_DLEN;
                            sock->wnd_send->cwnd = sock->wnd_send->ssthresh;
                            sock->wnd_send->congestion_status = CONGESTION_AVOIDANCE;
                            break;
                        
                        default:
                            break;
                    }
                    printf("\n待接收ACK号为%d\n",sock->wnd_send->base);
                }


            }
            else{
                //display_pkt(pkt);
                //printf("\n问题1\n");
                //pthread_mutex_lock(&sock->recv_lock);
                //printf("\n接收数据%s\n",pkt+DEFAULT_HEADER_LEN);
                recv_skb_node* nw_node=(recv_skb_node*)malloc(sizeof(recv_skb_node));
                nw_node->len=data_len;
                nw_node->data=malloc(data_len);
                memcpy(nw_node->data,pkt+DEFAULT_HEADER_LEN,data_len);

                nw_node->seq=seq;
                nw_node->flag=flags;
                nw_node->prev=NULL;
                nw_node->next=NULL;
                //printf("\n问题1\n");
                //printf("\n%d %d\n",sock->received_len+data_len,TCP_RECVWN_SIZE);
                if((sock->received_len+nw_node->len>TCP_BUF_SIZE)||(sock->wnd_recv->recv_len+nw_node->len>TCP_RECVWN_SIZE)){
                    printf("\n缓冲区或窗口已满，丢弃数据包\n");
                    char* nw_pkt = create_packet_buf(sock->established_local_addr.port, sock->established_remote_addr.port, 0,sock->wnd_recv->expect_seq,DEFAULT_HEADER_LEN, DEFAULT_HEADER_LEN,0x4,0, 0, NULL, 0);
                    sendToLayer3(nw_pkt, DEFAULT_HEADER_LEN);

                }
                else{
                    if(nw_node->seq==sock->wnd_recv->expect_seq)
                    {
                        printf("\n收到数据包seq=%d\n",nw_node->seq);
                        //printf("\n问题2\n");
                        add_to_wnd_recv(sock,nw_node);
                        //printf("\n问题5\n");
                        recv_skb_node* nw_head=sock->wnd_recv->window->head;
                        // if(nw_head->next==NULL){
                        //     wnd_to_buf(sock,nw_head);
                        //     sock->wnd_recv->recv_len-=nw_node->len;
                        //     sock->wnd_recv->window->head=sock->wnd_recv->window->head->next;
                        //     sock->wnd_recv->expect_seq+=nw_node->len+DEFAULT_HEADER_LEN;
                        // }
                        // else{
                        
                        while((nw_head->next!=NULL)&&(nw_head->seq+data_len+DEFAULT_HEADER_LEN == nw_head->next->seq)){
                            //printf("\n问题6\n");
                            wnd_to_buf(sock,nw_head);
                            //printf("\n问题7\n");
                            sock->wnd_recv->recv_len-=nw_node->len;
                            sock->wnd_recv->window->head=sock->wnd_recv->window->head->next;
                            nw_head=nw_head->next;
                            sock->wnd_recv->expect_seq+=nw_node->len+DEFAULT_HEADER_LEN;
                        }
                        //printf("\n问题8\n");
                        wnd_to_buf(sock,nw_head);
                        sock->wnd_recv->recv_len-=nw_node->len;
                        sock->wnd_recv->window->head=sock->wnd_recv->window->head->next;
                        sock->wnd_recv->expect_seq+=nw_node->len+DEFAULT_HEADER_LEN;
                        printf("\nexpect_seq=%d\n",sock->wnd_recv->expect_seq);


                        //}
                                        
                    }
                    else if(nw_node->seq<sock->wnd_recv->expect_seq){
                        printf("\n重复数据包，丢弃\n");
                    }
                    else{
                        add_to_wnd_recv(sock,nw_node);
                    }
                    //char* nw_pkt = create_packet_buf(sock->established_local_addr.port, sock->established_remote_addr.port, 0,sock->wnd_recv->expect_seq,DEFAULT_HEADER_LEN, DEFAULT_HEADER_LEN,0x4,0, 0, NULL, 0);
                    char* nw_pkt = create_packet_buf(sock->established_local_addr.port, sock->established_remote_addr.port, 0,sock->wnd_recv->expect_seq,DEFAULT_HEADER_LEN, DEFAULT_HEADER_LEN,0x4,TCP_RECVWN_SIZE-sock->wnd_recv->recv_len, 0, NULL, 0);
                    //     //printf("\n问题4\n");
                    sendToLayer3(nw_pkt, DEFAULT_HEADER_LEN);
                    // sendToLayer3(nw_pkt, DEFAULT_HEADER_LEN);
                    // sendToLayer3(nw_pkt, DEFAULT_HEADER_LEN);

                }
                


                // while (pthread_mutex_lock(&(sock->recv_lock)) != 0); // 加锁
                // //printf("\n问题3\n");
                
                // if(sock->received_len+data_len<=TCP_RECVWN_SIZE){
                //     //printf("\n问题1\n");
                //     if(sock->received_buf->head==NULL){
                //         //printf("\n问题2\n");

                //         sock->received_buf->head=nw_node;
                //         sock->received_buf->tail=nw_node;
                //         sock->received_len+=data_len;
                //     }
                //     else{
                //         //printf("\n问题3\n");
                //         sock->received_buf->tail->next=nw_node;
                //         sock->received_buf->tail=nw_node;
                //         sock->received_len+=data_len;
                //     }
                //     //printf("\n问题3\n");
                //     char* nw_pkt = create_packet_buf(sock->established_local_addr.port, sock->established_remote_addr.port, 0,get_seq(pkt)+get_plen(pkt),DEFAULT_HEADER_LEN, DEFAULT_HEADER_LEN,0x4,TCP_RECVWN_SIZE-sock->received_len, 0, NULL, 0);
				//     //printf("\n问题4\n");
                //     sendToLayer3(nw_pkt, DEFAULT_HEADER_LEN);
                //     //printf("\n问题5\n");
                // }
                // pthread_mutex_unlock(&(sock->recv_lock));
                // printf("\n放入接收缓冲区完成%s\n",nw_node->data);


            }
        break;
        case FIN_WAIT_1:
            if(is_ack){
                sock->state = FIN_WAIT_2;
                if(sock->sending_buf->head->flag==0x2){
                    sock->sending_buf->head=sock->sending_buf->head->next;
                }
                printf("The client accepts a ACK message\n");

            }
            else if(is_ack&&is_fin){
                sock->state = TIME_WAIT;
                if(sock->sending_buf->head->flag==0x6){
                    sock->sending_buf->head=sock->sending_buf->head->next;
                }
                printf("The client accepts a FIN&&ACK message\n");
                //第四次挥手
                add_to_send_buf(sock,NULL,0,0x4);

                // char* wavehand;
                // wavehand =create_packet_buf(get_dst(pkt),get_src(pkt), get_ack(pkt) , get_seq(pkt)+1, 
                //                             DEFAULT_HEADER_LEN, DEFAULT_HEADER_LEN, 0x4, 0, 0, NULL, 0);
                // sendToLayer3(wavehand,20);
                sleep(2);
                sock->state = CLOSED;
                printf("The client sends a LASK_ACK message and closes the connection\n");
            }
            break;
        case FIN_WAIT_2:
            if(is_ack&&is_fin){
                sock->state = TIME_WAIT;
                printf("The client accepts a FIN&&ACK message\n");
                //第四次挥手
                add_to_send_buf(sock,NULL,0,0x4);
                // char* wavehand;
                // wavehand =create_packet_buf(get_dst(pkt),get_src(pkt), get_ack(pkt) , get_seq(pkt)+1, 
                //                             DEFAULT_HEADER_LEN, DEFAULT_HEADER_LEN, 0x4, 0, 0, NULL, 0);
                // sendToLayer3(wavehand,20);
                // sleep(2);
                sock->state = CLOSED;
                printf("The client sends a LASK_ACK message and closes the connection\n");
            }
            break;
        case LAST_ACK:
            if(is_ack){
                if(sock->sending_buf->head->flag==0x2){
                    sock->sending_buf->head=sock->sending_buf->head->next;
                }
                sock->state = CLOSED;
                printf("The server accpets LASK_ACK message\n");
                printf("sock colses!\n");
            }
            break;
        default:
            break;


    }

    return 0;
}

int tju_close (tju_tcp_t* sock){
    sock->state = FIN_WAIT_1;
    char* wavehand1;
    add_to_send_buf(sock,NULL,0,0x2);
    //wavehand1 = create_packet_buf(sock->established_local_addr.port , sock->established_remote_addr.port,
                                    // 0, 0 ,DEFAULT_HEADER_LEN , DEFAULT_HEADER_LEN , 0x2 ,0,0,NULL,0);
    //sendToLayer3(wavehand1,20);
    //主动关闭
    printf("The client initiates a shutdown request and sends a FIN message \n");
    while (1)
    {
        if(sock -> state ==CLOSED)
            return 0;
    }
}

// void send_one(tju_tcp_t* sock)
// {
//     //printf("\n问题6\n");
//     if(sock->send_head==NULL){
//         return ;
//     }
//     printf("\n发送数据%s\n",sock->send_head->data);
//     //printf("\n问题7\n");
//     char *pkt=create_packet_buf(sock->established_local_addr.port,sock->established_remote_addr.port,464,0,DEFAULT_HEADER_LEN,DEFAULT_HEADER_LEN+sock->send_head->len,NO_FLAG,1,0,sock->send_head->data,sock->send_head->len);
//     sendToLayer3(pkt,DEFAULT_HEADER_LEN+sock->send_head->len);
//     sock->send_head=sock->send_head->next;
    
//     return ;
// }

void * send_thread(void* arg){
    //printf("\n问题1\n");
    int nw_arg=*((int*)arg);
    free(arg);
    tju_tcp_t* nw_sock;
    if(listen_socks[nw_arg]!=NULL){
        nw_sock=listen_socks[nw_arg];
    }
    else if(established_socks[nw_arg]!=NULL){
        nw_sock=established_socks[nw_arg];
    }
    else{
        printf("该线程socket未注册\n");
        exit(-1);
    }
    
    while(1)
    {
        skb_node* nw_head=nw_sock->send_head;
        //printf("\n问题2\n");
        if(nw_head!=NULL){
            //printf("\n问题2\n");
            while(pthread_mutex_lock(&(nw_sock->send_lock))!=0);
            //printf("发送窗口大小为%d",nw_sock->wnd_send->window_size);
            if(nw_sock->wnd_send->wait_for_ack+nw_head->len< nw_sock->wnd_send->window_size){
                nw_sock->send_head->seq=nw_sock->wnd_send->nextseq;
                gettimeofday(&(nw_sock->send_head->send_time),NULL);
                char* msg=create_packet_buf(nw_sock->established_local_addr.port,nw_sock->established_remote_addr.port,nw_sock->wnd_send->nextseq,0,DEFAULT_HEADER_LEN,DEFAULT_HEADER_LEN+nw_head->len,nw_head->flag,TCP_RECVWN_SIZE-nw_sock->wnd_recv->recv_len,0,nw_head->data,nw_head->len);
                sendToLayer3(msg,nw_head->len+DEFAULT_HEADER_LEN);
                printf("已发送%s,顺序号为%d\n",nw_sock->send_head->data,nw_sock->wnd_send->nextseq);
                nw_sock->send_head=nw_sock->send_head->next;
                nw_sock->wnd_send->nextseq+=nw_head->len+DEFAULT_HEADER_LEN;
                nw_sock->wnd_send->wait_for_ack+=nw_head->len;
                nw_head->skb_timer=creat_timer(nw_sock->wnd_send->timeout,retransmit);
                //printf("\n创建计时器完成\n");
                

            }
            else{
                while(nw_sock->wnd_send->wait_for_ack+nw_head->len >= nw_sock->wnd_send->window_size){}//缓冲区满，阻塞
            }
            pthread_mutex_unlock(&(nw_sock -> send_lock));
        }
    }
}





void add_to_wnd_recv(tju_tcp_t* sock,recv_skb_node* nw_node){
    //printf("\n问题3\n");
    recv_queue* recv_wnd=sock->wnd_recv->window;
    if(recv_wnd->head==NULL){
        //printf("\n问题4\n");
        recv_wnd->head=nw_node;
        recv_wnd->tail=nw_node;
        sock->wnd_recv->recv_len+=nw_node->len;

    }
    else{
        recv_skb_node* tmp=recv_wnd->head;
        if(nw_node->seq<tmp->seq){
            nw_node->next=tmp;
            sock->wnd_recv->window->head=nw_node;
            sock->wnd_recv->recv_len+=nw_node->len;
        }
        else{
            while(nw_node->seq > tmp->seq){
                tmp=tmp->next;
            }
            if(nw_node->seq==tmp->seq){
                printf("\n重复数据包\n");
            }
            else{
                nw_node->prev=tmp->prev;
                nw_node->next=tmp;
                tmp->prev=nw_node;
                sock->wnd_recv->recv_len+=nw_node->len;
            }


        }


    }
}

void wnd_to_buf(tju_tcp_t* sock,recv_skb_node* nw_node){
    while (pthread_mutex_lock(&(sock->recv_lock)) != 0); // 加锁

    if(sock->received_buf->head==NULL){
        //printf("\n问题2\n");

        sock->received_buf->head=nw_node;
        sock->received_buf->tail=nw_node;
        sock->received_len+=nw_node->len;
    }
    else{
        //printf("\n问题3\n");
        sock->received_buf->tail->next=nw_node;
        sock->received_buf->tail=nw_node;
        sock->received_len+=nw_node->len;
    }
    //printf("\n问题3\n");

    
    pthread_mutex_unlock(&(sock->recv_lock));
    //printf("\n放入接收缓冲区完成%s\n",nw_node->data);

}

void retransmit(tju_tcp_t* sock){
    sock->wnd_send->ssthresh =((( sock->wnd_send->cwnd ) / MAX_DLEN ) / 2 ) * MAX_DLEN;
	sock->wnd_send->cwnd = MAX_DLEN;
	sock->wnd_send->congestion_status = SLOW_START;
    if(sock->sending_buf->head != NULL){
        //printf("\n超时重传\n");
        skb_node* nw_node=sock->sending_buf->head;
        char* pkt=create_packet_buf(sock->established_local_addr.port,sock->established_remote_addr.port,sock->wnd_send->base,0,DEFAULT_HEADER_LEN,nw_node->len+DEFAULT_HEADER_LEN,NO_FLAG,0,0,nw_node->data,nw_node->len);
        sendToLayer3(pkt,nw_node->len+DEFAULT_HEADER_LEN);
        printf("\n重传一个数据包%s,%d\n",nw_node->data,sock->wnd_send->base);
    }
    return;
}

void persist(tju_tcp_t* sock){
    printf("\n窗口为0\n");
    char* pkt=create_packet_buf(sock->established_local_addr.port,sock->established_remote_addr.port,sock->wnd_recv->expect_seq,0,DEFAULT_HEADER_LEN,DEFAULT_HEADER_LEN,TEST_FLAG_MASK,0,0,NULL,0);
    sendToLayer3(pkt,DEFAULT_HEADER_LEN);
    printf("\n发送一个探测包\n");
}

void add_to_send_buf(tju_tcp_t* sock,char* send_data,int send_len,int flag){
    pthread_mutex_lock(&sock->send_lock);
    skb_node* nw_node=(skb_node*)malloc(sizeof(skb_node));
    nw_node->data=send_data;
    nw_node->len=send_len;   
    nw_node->next=NULL;
    nw_node->prev=NULL;
    nw_node->skb_timer=NULL;
    nw_node->flag=flag;
    nw_node->seq=0;

    //printf("\n问题2\n");

    if(sock->sending_buf->head==NULL){//缓冲区为空
        //printf("\n问题3\n");
        sock->sending_buf->head=nw_node;
        sock->sending_buf->tail=nw_node;
        sock->send_head=sock->sending_buf->head;
        sock->sending_len+=send_len;
        //printf("\n放入发送缓冲区%s\n",nw_node->data);
        

    }
    else{
        //printf("\n问题4\n");
        sock->sending_buf->tail->next=nw_node;
        nw_node->prev=sock->sending_buf->tail;
        //nw_node->next=sock->sending_buf->head;
        sock->sending_buf->tail=nw_node;
        if(sock->send_head==NULL){
            sock->send_head=nw_node;
        }
        sock->sending_len+=send_len;
        //printf("\n第二次放入发送缓冲区%s\n",nw_node->data);
    }
    //printf("\n问题5\n");
    //send_one(sock);
    pthread_mutex_unlock(&sock->send_lock);
    return;
}

void cal_rto(tju_tcp_t* sock,int ack,struct timeval recv_time){
    skb_node* nw_node=sock->sending_buf->head;
    while(nw_node!=NULL){
        if(nw_node->seq+nw_node->len+DEFAULT_HEADER_LEN==ack){
            break;
        }
        nw_node=nw_node->next;
    }
    int rtt_time=(recv_time.tv_sec-nw_node->send_time.tv_sec)*1000000+(recv_time.tv_usec-nw_node->send_time.tv_usec);
    printf("\nRTT为%d\n",rtt_time);
    if(sock->wnd_send->estmated_rtt==0){
        //rtt_time=(rtt_time/1000)*1000;
        sock->wnd_send->estmated_rtt=rtt_time;
        sock->wnd_send->rtt_var=rtt_time/2;
    }
    else{
        sock->wnd_send->estmated_rtt= 0.875 * sock->wnd_send->estmated_rtt + 0.125 * rtt_time;
        sock->wnd_send->rtt_var = 0.75 * sock->wnd_send->rtt_var + 0.25 * abs(sock->wnd_send->estmated_rtt - rtt_time);        

    }
   
    int rto = sock->wnd_send->estmated_rtt + 4 * sock->wnd_send->rtt_var;
    sock->wnd_send->timeout=rto;
    printf("\nRTO估计值为%d\n",rto);
}

