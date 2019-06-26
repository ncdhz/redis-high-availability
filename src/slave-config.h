//
//  slave-config.h
//  redis
//
//  Created by 马俊龙 on 2019/5/23.
//
#ifndef slave_config_h
#define slave_config_h
// 失败符号
extern char NO;
// 成功符号
extern char YES;
//配置信息保存
typedef struct url {
    char * host;
    int port;
    struct url * next;
} url;

// slave 配置信息
typedef struct configInformation{
    char open_master_slave;
    url * slave_address;
    int slave_address_len;
} CI;

extern CI ci;
// 解释slave配置文件
void analysisSlaveConfig(char * filename);
// 剔除字符串数组前后空格
void trim(char * str);

#endif /* slave_config_h */
