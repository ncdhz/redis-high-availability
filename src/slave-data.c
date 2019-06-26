//
//  slave-data.c
//  redis
//
//  Created by 马俊龙 on 2019/6/2.
//

#include <string.h>
#include <stdio.h>
#include <string.h>
#include "zmalloc.h"
#include "slave-cli.h"

int __redisSlaveAppendCommand(redisContext * c, const char *cmd) {
    if (cmd == NULL)
        return REDIS_ERR;
    int len = strlen(cmd);
    sds new_buf;
    new_buf = sdscatlen(c->obuf, cmd, len);
    if (new_buf == NULL)
        return REDIS_ERR;
    c->obuf = new_buf;
    return REDIS_OK;
    
}

static void *__redisSlaveBlockForReply(redisContext *c) {
    void *reply;
    
    if (c->flags & REDIS_BLOCK) {
        if (redisGetReply(c,&reply) != REDIS_OK)
            return NULL;
        return reply;
    }
    return NULL;
}
/**
 * 本文件中所有给服务器发送数据的方法传入的数据必须是整理好的
 * 数据提交成功返回 1 否则返回 -1
 */
int redisSlaveCommand(redisContext * c, int count, const char *cmd) {
    if (__redisSlaveAppendCommand(c,cmd) != REDIS_OK)
        return -1;
    for (int i = 0; i < count; ++i) {
        redisReply * reply = __redisSlaveBlockForReply(c);
        if (reply == NULL) {
            return -1;
        }
        // 错误处理
        if (reply->type == 6) {
            char * err_data = "LOADING";
            char * str_err = zmalloc(strlen(err_data) + 1);
            memcpy(str_err,reply->str, strlen(err_data));
            if (strcasecmp(str_err, err_data) == 0) {
                zfree(str_err);
                return -1;
            }
            zfree(str_err);
        }
        freeReplyObject(reply);
    }
    return 1;
}
