//
//  slave-cli.c
//  redis5
//
//  Created by 马俊龙 on 2019/5/21.
//
#include "slave-cli.h"
#include "slave-config.h"
#include <string.h>
#include "zmalloc.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "ae.h"
// 用于过滤掉连接的数据
CI slave_ci;

// 每次提交数据的最大长度
#define DATA_MAX_LEN 102400

pthread_mutex_t slave_data_mutex;

typedef struct slaveDataNode{
    // 数据
    char * data;
    
    // 命令的总长度
    size_t data_len;
    // 命令所包含原子命令个数
    int data_count;
    
    struct slaveDataNode * next;
    
    struct slaveDataNode * before;

} SDN;
/**
 * 双头链表
 */
typedef struct slaveRequestNode {
    
    struct redisContext * redis_client;
    
    SDN * sdn_start;
    
    SDN * sdn_end;
} SRN;

typedef struct pthread_count {
    int count;
} pthread_count;

SRN ** slave_parent;

const char * ENTER_NEWLINE = "\r\n";
/**
 * 获取 SDN
 */
SDN * getSDN(SRN * srn){
    pthread_mutex_lock(&slave_data_mutex);
    SDN * sdn = srn->sdn_start;
    if (sdn != NULL) {
        if (sdn->next !=NULL) {
            srn->sdn_start = sdn->next;
            sdn->next->before = NULL;
        } else {
            srn->sdn_start = NULL;
            srn->sdn_end = NULL;
        }
    }
    pthread_mutex_unlock(&slave_data_mutex);
    return sdn;
}

void addSDN(SRN * srn, SDN * sdn){
    pthread_mutex_lock(&slave_data_mutex);
    SDN * s = srn->sdn_start;
    if (s == NULL) {
        srn->sdn_start = sdn;
        srn->sdn_end = sdn;
    }else{
        srn->sdn_start = sdn;
        sdn->next = s;
        s->before = sdn;
    }
    pthread_mutex_unlock(&slave_data_mutex);
}

/**
 * 请求是否可以过滤
 * 可以过滤返回 1
 * 不可过滤返回 -1
 */
int isFilterData(char * data){
    if(strcasecmp(data, "COMMAND") == 0)return 1;
    else if(strcasecmp(data, "EXISTS") == 0) return 1;
    else if(strcasecmp(data, "KEYS") == 0) return 1;
    else if(strcasecmp(data, "OBJECT") == 0) return 1;
    else if(strcasecmp(data, "PTTL") == 0) return 1;
    else if(strcasecmp(data, "RANDOMKEY") == 0) return 1;
    else if(strcasecmp(data, "TTL") == 0) return 1;
    else if(strcasecmp(data, "TYPE") == 0) return 1;
    else if(strcasecmp(data, "SCAN") == 0) return 1;
    else if(strcasecmp(data, "BITCOUNT") == 0) return 1;
    else if(strcasecmp(data, "GET") == 0) return 1;
    else if(strcasecmp(data, "GETBIT") == 0) return 1;
    else if(strcasecmp(data, "GETRANGE") == 0) return 1;
    else if(strcasecmp(data, "MGET") == 0) return 1;
    else if(strcasecmp(data, "STRLEN") == 0) return 1;
    else if(strcasecmp(data, "HEXISTS") == 0) return 1;
    else if(strcasecmp(data, "HGET") == 0) return 1;
    else if(strcasecmp(data, "HGETALL") == 0) return 1;
    else if(strcasecmp(data, "HKEYS") == 0) return 1;
    else if(strcasecmp(data, "HLEN") == 0) return 1;
    else if(strcasecmp(data, "HMGET") == 0) return 1;
    else if(strcasecmp(data, "HVALS") == 0) return 1;
    else if(strcasecmp(data, "HSCAN") == 0) return 1;
    else if(strcasecmp(data, "BLPOP") == 0) return 1;
    else if(strcasecmp(data, "BRPOP") == 0) return 1;
    else if(strcasecmp(data, "BRPOPLPUSH") == 0) return 1;
    else if(strcasecmp(data, "LINDEX") == 0) return 1;
    else if(strcasecmp(data, "LLEN") == 0) return 1;
    else if(strcasecmp(data, "LRANGE") == 0) return 1;
    else if(strcasecmp(data, "PING") == 0) return 1;
    else if(strcasecmp(data, "ECHO") == 0) return 1;
    else if(strcasecmp(data, "QUIT") == 0) return 1;
    else if(strcasecmp(data, "CLIENT GETNAME") == 0) return 1;
    else if(strcasecmp(data, "CLIENT KILL") == 0) return 1;
    else if(strcasecmp(data, "CLIENT LIST") == 0) return 1;
    else if(strcasecmp(data, "CLIENT SETNAME") == 0) return 1;
    else if(strcasecmp(data, "CONFIG GET") == 0) return 1;
    else if(strcasecmp(data, "DBSIZE") == 0) return 1;
    else if(strcasecmp(data, "DEBUG OBJECT") == 0) return 1;
    else if(strcasecmp(data, "INFO") == 0) return 1;
    else if(strcasecmp(data, "LASTSAVE") == 0) return 1;
    else if(strcasecmp(data, "MONITOR") == 0) return 1;
    else if(strcasecmp(data, "PSYNC") == 0) return 1;
    else if(strcasecmp(data, "TIME") == 0) return 1;
    return -1;
}

// 查看数据是否是“改变数据” 如果是返回 0
// 如果不是返回 -1
int eliminateData(char * data){
    char * new_data = zstrdup(data);
    char * new_d = new_data;
    int i=0,j=0;
    while (*new_d != '\0') {
        if (*new_d == '\r') {
            i++;
        }
        if (i == 3) {
            *new_d = '\0';
            break;
        }
        new_d ++;
        if (i < 2) {
            j++;
        }
    }
    char * d =(new_data + j + 2);
    if (isFilterData(d) == 1) {
        zfree(new_data);
        return -1;
    }
    zfree(new_data);
    return 0;
}

int timerHandler(struct aeEventLoop * eventLoop, long long id, void * d){
    pthread_count * count = d;
    SRN * srn = slave_parent[count->count];
    redisContext * context = srn->redis_client;
    SDN * sdn = getSDN(srn);
    while (sdn != NULL) {
        int s = redisSlaveCommand(context,sdn->data_count ,sdn->data);
        if (s == -1){
            addSDN(srn,sdn);
            redisReconnect(context);
            break;
        }
        zfree(sdn->data);
        zfree(sdn);
        sdn = getSDN(srn);
    }
    return 100;
}

/**
 * 用于创建 SDN
 */
void createSDN(int count, char * d){
    pthread_mutex_lock(&slave_data_mutex);
    size_t str_len = strlen(d);
    for (int i=0; i < slave_ci.slave_address_len; i++) {
        SRN * srn = slave_parent[i];
        SDN * sdn = srn->sdn_end;
        if (sdn != NULL) {
            size_t data_len = sdn->data_len + str_len;
            if (data_len < DATA_MAX_LEN) {
                char * data = zmalloc(data_len + 1);
                strcpy(data, sdn->data);
                zfree(sdn->data);
                strcat(data, d);
                sdn->data = data;
                sdn->data_count = sdn->data_count + count;
                sdn->data_len = data_len;
                continue;
            }
        }
        SDN * sdn_1 = zmalloc(sizeof(SDN));
        sdn_1->data = zstrdup(d);
        sdn_1->before = NULL;
        sdn_1->next = NULL;
        sdn_1->data_len = str_len;
        sdn_1->data_count = count;
        if (sdn != NULL) {
            sdn->next = sdn_1;
            sdn_1->before = sdn;
        }else {
            srn->sdn_start = sdn_1;
        }
        srn->sdn_end = sdn_1;
    }
    pthread_mutex_unlock(&slave_data_mutex);
}
/**
 * 用于初始化 slaveRequestNode 产生一个 slaveRequestNode 数组
 */
SRN ** slaveRequestNodeInit() {
    // 连接超时时间
    struct timeval t = {.tv_sec = 2};
    url * h = slave_ci.slave_address;
    SRN ** all_srn = zmalloc(slave_ci.slave_address_len * sizeof(SRN));
    for (int i=0; i< slave_ci.slave_address_len && h != NULL; i++) {
        
        SRN * srn = zmalloc(sizeof(SRN));
        srn->redis_client = redisConnectWithTimeout(h->host,h->port, t);
        srn->sdn_start = NULL;
        srn->sdn_end = NULL;
        all_srn[i] = srn;
        h = h->next;
    }
    return all_srn;
}
/**
 * 数据处理程序 slaveRequestNode 节点
 */
void * slaveDataHandler(void * count){
    aeEventLoop * ae = aeCreateEventLoop(64);
    aeCreateTimeEvent(ae, 100, timerHandler, count, NULL);
    aeMain(ae);
    aeDeleteEventLoop(ae);
    zfree(count);
    return NULL;
}
/**
 * 用于创建 slave 线程
 */
void slaveThreadCreate(){
    for (int i=0; i<slave_ci.slave_address_len; i++) {
        pthread_count * count = zmalloc(sizeof(pthread_count));
        count->count = i;
        pthread_t slave_main_thread;
        if (pthread_create( &slave_main_thread, NULL, slaveDataHandler , count) == -1) {
            exit(1);
        }
        // 当线程运行结束时可以释放资源也不会使线程堵塞
        pthread_detach(slave_main_thread);
    }
}

void slaveConfigInit(){
    
    analysisSlaveConfig("../redis-slave.conf");
    slave_ci = ci;
    
    if (ci.open_master_slave == YES && ci.slave_address_len > 0){

        /**
         * 用于初始化从节点的所有通道
         */
        slave_parent = slaveRequestNodeInit();
        // 数据同步锁
        pthread_mutex_init(&slave_data_mutex, NULL);
        /**
         * 当 open_master_slave 为 true
         * 并且 slave_address_len 的长度大于 0
         * 初始化线程 线程数量与备用数据库一致
         */
        slaveThreadCreate();
    }
}

int slaveMain(char * ip,  char * c){
    if (slave_ci.slave_address_len > 0 && slave_ci.open_master_slave == YES ){
        if (c != NULL) {
            /**
             * 用于检测数据来自哪个客户端
             * 如果来自主备客户端
             * 过滤掉数据
             */
            url * u = slave_ci.slave_address;
            while (u!=NULL) {
                if (strcmp(ip, u->host) == 0) {
                    return -1;
                }
                u = u->next;
            }
            // 用于剔除不需要备份的命令
            int data_count = 0;
            char * data = zstrdup(c);
            char * data_1 = zmalloc((strlen(c) + 1) * sizeof(char));
            char * d = strtok(data, "*");
            while (d != NULL) {
                if (*d != '0' && eliminateData(d) == 0) {
                    if (data_count == 0) {
                        strcpy(data_1, "*");
                        strcat(data_1, d);
                    }else {
                        strcat(data_1, "*");
                        strcat(data_1, d);
                    }
                    data_count++;
                }
                d = strtok(NULL, "*");
            }
            zfree(data);
            // 检查剔除数据之后是否还存在数据
            // 没有数据返回 -1
            // 存在数据继续执行
            if (data_count == 0) {
                zfree(data_1);
                return -1;
            }
            // 创建数据节点
            createSDN(data_count, data_1);
            zfree(data_1);
        }
    }
    return 0;
}
