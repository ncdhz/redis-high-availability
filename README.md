# redis 主从/双主配置

1. 获取源码
2. 环境
```
    centos 7 两台
    centos1 ip: 192.168.0.202
    centos2 ip: 192.168.0.203
```
3. 上传源码到两台 `centos`
4. 安装（两台机器操作一样）
   + `make` 源文件  
   ```
    > cd redis-5.0.5
    > make
   ```
   + `make install` 源文件
   ```
    > cd src
    > sudo make install
   ```
5. 修改配置文件
    + 主从配置（只用修改主节点配置文件）
    ```
    > cd redis-5.0.5
    > vi redis-slave.conf
    打开注解 修改 open-master-slave false 为 open-master-slave true 
    打开注解 修改 slave-address localhost:6379 为  slave-address 192.168.0.203:6379
    保存配置文件
    ```
    ```
    启动服务 (两台机器都需要开启)
    > cd redis-5.0.5/src
    > redis-server ../redis.conf
    到这里主从配置就成功了可以通过 redis-cli 在主节点测试了
    ```
    + 主主配置
    ```
    centos1:
    > cd redis-5.0.5
    > vi redis-slave.conf
    打开注解 修改 open-master-slave false 为 open-master-slave true 
    打开注解 修改 slave-address localhost:6379 为  slave-address 192.168.0.203:6379
    保存配置文件

    centos2
    > cd redis-5.0.5
    > vi redis-slave.conf
    打开注解 修改 open-master-slave false 为 open-master-slave true 
    打开注解 修改 slave-address localhost:6379 为  slave-address 192.168.0.202:6379
    保存配置文件
    ```
    ```
    启动服务 (两台机器都需要开启)
    > cd redis-5.0.5/src
    > redis-server ../redis.conf
    到这里主从配置就成功了可以通过 redis-cli 在两台节点测试了
    ```
