# gdbus-learn

一个基于 `gdbus` 的双工程示例项目，用来演示下面这些能力：

- 服务端/客户端分层通信
- `gdbus-codegen` 生成接口绑定
- 普通数据的 `Set/Get + Signal` 同步
- 动态库封装客户端调用逻辑
- 基于共享内存的大文件分片传输
- 在真实全局 `system bus` 上运行

## 项目是做什么的

项目提供一个 D-Bus 服务 `com.example.Training`，包含两类能力：

1. 普通数据通信  
   支持 `bool`、`int`、`double`、`string` 和结构体 `TestInfo` 的远程 `Set/Get`。

2. 文件传输  
   支持客户端上传文件到服务端，也支持客户端从服务端下载文件到本地。

当前代码默认运行在 **全局 `system bus`** 上，不再依赖手工启动 `session bus`，也不需要手工设置 `DBUS_SESSION_BUS_ADDRESS`。

## 项目结构

```text
gdbus-learn/
├── Common/
│   ├── Include/
│   ├── Interfaces/
│   └── Sources/
├── ServiceProject/
│   ├── Include/
│   ├── Sources/
│   ├── Library/
│   └── app/
├── ClientProject/
│   ├── Include/
│   ├── Sources/
│   ├── app/
│   └── tests/
└── deploy/
    ├── dbus/
    └── systemd/
```

- `Common/`
  公共接口、工具类、序列化和共享内存辅助逻辑。
- `ServiceProject/`
  服务端实现，产出 `server`。
- `ServiceProject/Library/`
  动态库实现，产出 `libtraining.so`，供客户端运行时加载。
- `ClientProject/`
  客户端包装层、命令行例程和测试代码。
- `deploy/dbus/`
  system bus policy 配置。
- `deploy/systemd/`
  `systemd` 服务模板。

## 通信原理

### 1. 普通数据同步

普通数据通过 D-Bus 方法调用和广播信号完成同步：

1. client 调用 `Set`
2. service 更新状态
3. service 发送 `OnTest*Changed`
4. client 收到广播后再次调用对应的 `Get`
5. client 用最新结果更新本地缓存

这种方式避免客户端只依赖广播载荷，保证最终状态以服务端为准。

### 2. 文件传输

文件内容不直接放进 D-Bus 消息，而是采用“控制面走 D-Bus、数据面走共享内存”的方式：

- D-Bus 传控制参数
- 共享内存传分片数据
- 分片大小固定为 `1KB`
- 服务端最终做 MD5 校验和落盘

上传方向：

- 客户端写共享内存
- 服务端读共享内存

下载方向：

- 客户端先创建共享内存
- 服务端只打开并写入共享内存
- 客户端再读取共享内存

这样做的原因是：在真实 `systemd + system bus` 场景下，服务端和客户端可能不是同一用户。  
如果下载时由服务端重新创建共享内存，容易触发权限问题。当前代码已经按这个约束修正过。

## 运行前需要什么

### 环境要求

- Linux 系统
- 系统已经启动全局 `system bus`
- 安装以下依赖：
  - `cmake`
  - `g++`
  - `pkg-config`
  - `libglib2.0-dev`
  - `dbus`

Ubuntu / Debian 可直接安装：

```bash
sudo apt update
sudo apt install -y build-essential cmake pkg-config libglib2.0-dev dbus
```

### 权限要求

运行前需要满足两类权限：

1. D-Bus 权限  
   服务端必须被允许在 `system bus` 上占用 `com.example.Training`。

2. 可执行权限  
   至少保证下面这些文件可执行：

```bash
chmod 755 ./build/ServiceProject/app/server
chmod 755 ./build/ClientProject/training_client
```

如果是通过 `cmake --build` 生成，一般会自动带上正确权限。

## 如何配置和编译

项目提供了两个部署文件：

- `deploy/dbus/com.example.Training.conf`
- `deploy/systemd/training.service.in`

它们的作用分别是：

- `com.example.Training.conf`
  给全局 `system bus` 增加 policy，允许服务端占用服务名，也允许客户端调用服务。
- `training.service.in`
  用于生成 `systemd` 服务，适合车机或长期运行场景。

### 构建项目

```bash
cmake -DCMAKE_BUILD_TYPE:STRING=Release -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE -DBUILD_STAGE=ALL \
      --no-warn-unused-cli -S gdbus-learn -B build -G "CodeBlocks - Unix Makefiles"
cmake --build build --config Release --target all -j$(nproc)
```

主要产物：

- `build/ServiceProject/app/server`
- `build/ServiceProject/Library/libtraining.so`
- `build/ClientProject/training_client`

### 安装 system bus policy

```bash
sudo cmake --install build
sudo systemctl restart dbus
```

执行完成后，默认会安装：

- `/usr/share/dbus-1/system.d/com.example.Training.conf`
- `/usr/lib/systemd/system/training.service`
- `/usr/local/libexec/training/server`

如果想确认 policy 已安装：

```bash
ls -l /usr/share/dbus-1/system.d/com.example.Training.conf
```

## 如何启动例程

### 方式 1：手动前台启动服务端

这是开发调试最直接的方式。

```bash
./build/ServiceProject/app/server
```

> *_若出现`training_service error: failed to own D-Bus name "com.example.Training": name already owned or denied by bus policy`_
>
>_请关闭系统服务`sudo systemctl stop training.service`_

特点：

- 服务端是手动启动的
- 连接 `system bus` 是自动完成的
- 不需要设置环境变量
- 关闭时直接按 `Ctrl+C`

### 方式 2：作为 systemd 服务启动

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now training.service
systemctl status training.service
```

更适合真实设备部署。

### 验证服务是否已注册到全局 system bus

```bash
gdbus introspect --system --dest com.example.Training --object-path /com/example/Training
```

如果能看到 `SetTestInt`、`GetTestInt`、`SendFileChunk`、`BeginFileDownload`、`ReadFileChunk` 等接口，就说明服务已经真正挂到全局 `system bus` 上。

## 如何测试例程

### 1. 自动化测试

```bash
ctest --test-dir build --output-on-failure
```

当前主要覆盖：

- 基础 `Set/Get`
- 广播同步
- 上传
- 下载
- 并发传输
- 覆盖下载
- 超时恢复
- 可继承客户端
- 重复启动服务端占名失败

### 2. 手动交互测试

先启动服务端：

```bash
./build/ServiceProject/app/server
```

再开一个终端运行客户端：

```bash
./build/ClientProject/training_client
```

#### 基础通信测试

在客户端菜单输入：

```text
2
123
```

表示执行 `SetTestInt(123)`。

如果再开一个客户端终端，也运行 `training_client`，应能看到广播：

```text
[Listener] OnTestIntChanged: 123
```

然后输入：

```text
7
```

应看到：

```text
GetTestInt -> 123
```

#### 上传/下载测试

先准备一个小文件：

```bash
printf 'hello-system-bus\n' > /tmp/manual_upload.txt
```

如果要验证负载文件，可以再创建一个 1MB 文件：

```bash
dd if=/dev/zero of=/tmp/manual_upload_1mb.bin bs=1K count=1024
```

客户端菜单上传：

```text
11
/tmp/manual_upload.txt
manual/demo.txt
```

客户端下载：

```text
12
manual/demo.txt
/tmp/manual_download.txt
```

检查结果：

```bash
cat /tmp/manual_download.txt
```

如果使用 1MB 文件，建议再校验一次：

```bash
cmp -s /tmp/manual_upload_1mb.bin /tmp/manual_download.txt && echo files-match
```

### 3. 新系统最小验收流程

如果是一台全新 Linux 机器，按这 6 步即可：

1. 安装依赖
2. 编译项目
3. `sudo cmake --install build`
4. `sudo systemctl restart dbus`
5. 手动运行 `./build/ServiceProject/app/server`
6. 运行 `./build/ClientProject/training_client` 做交互验证

## 常见问题

### 1. 服务端启动报 `AccessDenied`

说明 system bus policy 没装好，或者装完后没有重载 `dbus`。

先检查：

```bash
ls -l /usr/share/dbus-1/system.d/com.example.Training.conf
sudo systemctl restart dbus
```

### 2. 客户端报 `failed to create Training proxy`

通常说明：

- 服务端没启动
- 服务端没有成功注册到 `system bus`

先检查：

```bash
systemctl status training.service
gdbus introspect --system --dest com.example.Training --object-path /com/example/Training
```

### 3. 下载时报共享内存 `Permission denied`

这是 system bus 场景里要特别注意的问题。

常见原因：

- 服务端和客户端用不同用户运行
- 下载共享内存的创建方和打开方处理不当

当前项目已经按“客户端下载先创建、服务端只打开写入”的方式修复过这个问题。  
如果后续修改下载流程、切换 `systemd` 用户、或者改共享内存逻辑，这部分必须回归验证。
