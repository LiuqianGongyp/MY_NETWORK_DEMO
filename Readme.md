# My_Network_Demo



为理解muduo设计的精妙之处，并学习如何达到高并发，用C++11重构简易网络库。重构的网络库去除了muduo网络库对boost库的依赖、更容易理解。同时，内部实现了HTTP服务器，可支持GET请求和静态资源的访问。除此之外，实现了数据库连接池，可动态管理数据库连接。

# 编译动态链接库

根目录的build.sh中把项目路径修改为自己当前项目的路径，然后执行如下代码生成动态链接库

`sudo ./build.sh`

# 测试

1. 测试回声服务器

   `cd example
   make
   ./EchoServer`

   客户端在命令行输入

   `nc 127.0.0.1 8080`

2. 测试http服务器

   `cd example
   make
   ./HttpServer`

   客户端打开网页，在地址栏输入

   `”xxx.xxx.xxx.xxx:8004“`

3. 测试日志

   `cd example
   make
   ./loggingtest1`

4. 测试数据库连接池

   `cd example
   make
   ./loggingtest1`

