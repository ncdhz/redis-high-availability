#include "slave-config.h"
#include <fcntl.h>
#include <sys/stat.h>
#include "zmalloc.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

// 换行符定义
const char * NEW_LINE = "\n";
// 空格定义
const char BLANK_SPACE = ' ';
// 失败符号
char NO ='n';
// 成功符号
char YES = 'y';

// 注解
const char NOTES = '#';

// 用于分割host,host
const char * GNU_SED = ",";
// 用于分割ip:port
const char IP_PORT_SPLIT = ':';

CI ci={.open_master_slave='n',.slave_address = NULL,.slave_address_len = 0};

//是否打开主从模式
const char * OPEN_MASTER_SLAVE = "open-master-slave";

//主从模式的ip和端口
const char * SLAVE_ADDRESS = "slave-address";


// 获取文件长度
int getfileLen(char * filename){
    struct stat fm;
    stat(filename,&fm);
    return fm.st_size;
}
// 用于读取文件数据载入 buf 中
int readFileData(char * filename, char * buf) {
    int fd = open(filename, O_RDONLY);
    long len = getfileLen(filename);
    read(fd, buf, len);
    close(fd);
    return strlen(buf);
}

void trim(char *str){
    if (str == NULL) return;
    int i = 0, j = 0;
    while (str[i] == ' ') i++;
    while (str[i] != '\0')  str[j++] = str[i++];
    str[j] = '\0';
    str = str+strlen(str)-1;
    while (*str == ' ') --str;
    *(str+1) = '\0';
}

// 查看配置是否匹配
// OPEN_MASTER_SLAVE 如果匹配返回 1
// SLAVE_ADDRESS 如果匹配返回 2
// 如果没有匹配项返回 -1
int configMatching(char * value){
    if (strcasecmp(value, OPEN_MASTER_SLAVE) == 0){
        return 1;
    }
    if (strcasecmp(value, SLAVE_ADDRESS) == 0){
        return 2;
    }
    return -1;
}
// 获取地址
url * getUrl(char * value) {
    char * value1 = value;
    if (value == NULL) {
        return NULL;
    }
    while (*value != '\0' && *value != IP_PORT_SPLIT) {
        value ++;
    }
    if (*value == '\0') {
        exit(1);
    }
    *value = '\0';
    value ++;
    url * h_node =  zmalloc(sizeof(url));
    trim(value);
    trim(value1);
    h_node->host = zstrdup(value1);
    h_node->port = strtol(value, NULL, 10);
    return h_node;
}

// 用于生成 ip 和 port 的结构体
void hostList(char * value){
    char * data = strtok(value, GNU_SED);
    while (data != NULL){
        
        url * h_node = getUrl(data);
        
        data = strtok(NULL, GNU_SED);
        
        url * url_slave = ci.slave_address;
        char err = NO;
        while (url_slave!=NULL) {
            if (strcasecmp(url_slave->host, h_node->host) == 0 && h_node->port == url_slave->port) {
                err = YES;
                break;
            }
            url_slave = url_slave->next;
        }
        if (err == YES) {
            zfree(h_node->host);
            zfree(h_node);
            continue;
        }
        h_node->next = ci.slave_address;
        ci.slave_address = h_node;
        ci.slave_address_len ++ ;
    }
}

void cFree(int num , ...){
    va_list ap;
    va_start(ap, num);
    for (int i = 0; i < num; ++i) {
        void * f = va_arg(ap, void *);
        zfree(f);
    }
    va_end(ap);
}


// 用于解析 slave 的配置文件
void analysisSlaveConfig(char * filename) {
    
    int len = getfileLen(filename);
    char * buf = zmalloc(len * sizeof(char));
    readFileData(filename, buf);
    char * lines = strtok(buf, NEW_LINE);
    while (lines != NULL){
        // 去除字符串前后空格
        trim(lines);
        int size = strlen(lines);
        char * config_data = zmalloc((size + 1) * sizeof(char));
        char * config_data_1 = config_data;
        // 用于提取有效信息
        // 去除注释掉的信息
        while (*lines!=NOTES && (*config_data_1 = *lines) != '\0'){
            config_data_1++;
            lines++;
        }
        lines = strtok(NULL,NEW_LINE);
        
        *config_data_1 = '\0';
        
        trim(config_data);
        
        if (*config_data == '\0') continue;
        int config_data_len = strlen(config_data);
        char * name = zmalloc(config_data_len * sizeof(char));
        char * value = zmalloc(config_data_len * sizeof(char));
        char is_name = YES;
        int i = 0,j = 0;
        for (; config_data[i] != '\0'; ++i) {
            if( is_name == YES ){
                if(config_data[i]==BLANK_SPACE){
                    is_name = NO;
                    name[j]='\0';
                    j = 0;
                    continue;
                }
                name[j++] = config_data[i];
            } else{
                value[j++] = config_data[i];
            }
        }
        value[j] = '\0';
        trim(value);
        
        int config_matching = configMatching(name);
        
        if (config_matching == 1){
            if (strcmp(value, "false")==0){
                cFree(3,name,value,config_data);
                break;
            } else if(strcmp(value,"true") == 0){
                ci.open_master_slave = YES;
            } else {
                cFree(3,name,value,config_data);
                break;
            }
        } else if (config_matching == 2){
            hostList(value);
        }
        cFree(3,name,value,config_data);
    }
    cFree(1, buf);
}
