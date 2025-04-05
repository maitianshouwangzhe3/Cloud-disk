# Cloud-disk
# 一个用于学习交流的云盘项目
[体验项目](https://www.cloud-cdisk.cn) 

项目环境：Ubuntu24.04

项目用到的开源框架：nginx(openresty)、fastdfs, rapidjson, muduo

# 运行项目

```shell
#这里默认前面提到的依赖都已经准备好测试无误了
#并且配置文件也修改完毕
mkdir build && cd build
cmake ..
make
```

# 项目实现的功能

#### 有一个web服务器

- 静态资源部署, 都在front目录下
- 动态请求 - 基于muduo的tcpServer
  - 注册
  - 登录
  - 上传
  - 下载
  - 秒传
  - 文件列表的获取
  - 分享
  - 取消分享
  - 下载分享的文件
  - 下载榜

# 项目的整体框架图

![](./source/1527001368556.png)![]()


# 后面的一些计划

​1.增加容量的限制 
2.增加提取码功能

# 最后

有兴趣一起交流的加我QQ:2681457694
