//
//  slave-cli.h
//  redis5
//
//  Created by 马俊龙 on 2019/5/21.
//

#include <hiredis.h>
#ifndef slave_cli_h
#define slave_cli_h
int slaveMain(char * ip,  char * c);

void slaveConfigInit(void);

int redisSlaveCommand(redisContext * c, int count, const char *cmd);
#endif /* slave_cli_h */
