# gdbus-learn

一个基于 `gdbus` 的学习项目，用来练习以下内容：

- 使用 `CMake` 组织双工程
- 使用 `gdbus-codegen` 根据 XML 生成接口绑定代码
- 使用 `gdbus` 完成 `service` / `client` 进程间通信
- 通过动态库封装客户端调用逻辑
- 使用共享内存完成大文件分片传输

当前项目已经实现了：

- `ITestService` 的 `Set` / `Get` 接口
- `ITestListener` 广播通知
- `server` 保存最新状态并向 client 广播
- `client` 侧命令行交互
- `libtraining.so` 动态库加载
- 基于共享内存的文件发送、组包、MD5 校验和落盘
- 多终端联调
- `TrainingClient` 可继承扩展，便于后续接入 UI

## 目录结构

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
└── build/
```

各目录职责如下：

- `Common/`
  放公共接口、公共工具、序列化和共享内存辅助逻辑。
- `ServiceProject/`
  放服务端实现，最终生成 `server` 和 `libtraining.so`。
- `ServiceProject/Library/`
  放动态库内部实现，负责封装 client 侧对 D-Bus 的访问。
- `ClientProject/`
  放客户端包装类、命令行程序和测试代码。

## 工程结构说明

项目由两个工程组成：

### 1. ServiceProject

负责编译出：

- `server`
- `libtraining.so`

其中：

- `server` 是真正提供 D-Bus 服务的进程
- `libtraining.so` 是供客户端运行时加载的动态库

`server` 负责：

- 创建 D-Bus connection
- 注册 skeleton 和 method 回调
- 保存 `TestInfo` 最新状态
- 处理 `Set` / `Get`
- 广播 `OnTest*Changed`
- 接收共享内存文件分片并组包落盘

### 2. ClientProject

负责编译出：

- `training_client`

`training_client` 本身不直接实现底层 D-Bus 调用，而是：

- 自己实现 `TrainingClient`
- 继承 `ITestService` 和 `ITestListener`
- 在运行时加载 `libtraining.so`
- 通过动态库导出的接口完成 `Set` / `Get` / 文件发送

## 接口与通信方式

项目使用 `Common/Interfaces/com.example.Training.xml` 作为 D-Bus 接口描述文件，并通过 `gdbus-codegen` 生成对应绑定代码。

当前主要包含两类能力：

### 1. 普通数据通信

- `SetTestBool`
- `SetTestInt`
- `SetTestDouble`
- `SetTestString`
- `SetTestInfo`
- `GetTestBool`
- `GetTestInt`
- `GetTestDouble`
- `GetTestString`
- `GetTestInfo`

广播接口：

- `OnTestBoolChanged`
- `OnTestIntChanged`
- `OnTestDoubleChanged`
- `OnTestStringChanged`
- `OnTestInfoChanged`

基本流程如下：

1. client 调用 `Set`
2. service 更新状态
3. service 发出广播
4. client 收到广播后，再主动调用一次对应的 `Get`
5. client 用 `Get` 的结果更新本地缓存

### 2. 文件传输

文件传输不直接把大块数据放进 D-Bus 消息，而是采用：

- 共享内存传输实际分片数据
- D-Bus 只传控制参数

当前规则如下：

- 共享内存大小固定为 `1KB`
- client 将文件按 `1KB` 分片
- 每次把一片写入共享内存后，同步调用 `SendFileChunk`
- service 收到后读取共享内存内容，追加写入临时文件
- 全部分片完成后，service 使用 `md5sum` 校验文件完整性
- 校验通过后，文件保存到 `server` 可执行文件所在目录

同名文件并发规则：

- 不同文件名允许并发发送
- 同名文件如果已有活跃传输，后来的直接拒绝

## 依赖环境

需要以下基础工具：

- `cmake`
- `g++`
- `pkg-config`
- `gdbus-codegen`
- `gio-2.0`
- `glib-2.0`
- `gobject-2.0`
- `gio-unix-2.0`
- `dbus-run-session` 或 `dbus-daemon`
- `md5sum`

在 Ubuntu / Debian 系环境下，可以安装：

```bash
sudo apt update
sudo apt install -y build-essential cmake pkg-config libglib2.0-dev dbus
```

## 构建方式

```bash
cmake -S . -B build
cmake --build build -j$(nproc)
```

编译完成后，主要产物为：

- `build/ServiceProject/app/server`
- `build/ServiceProject/Library/libtraining.so`
- `build/ClientProject/training_client`

## 快速运行

最简单的单终端演示方式：

```bash
dbus-run-session -- bash
./build/ServiceProject/app/server
```

另开一个终端进入同一个 shell 会话不方便时，更建议使用下面的多终端方式。

## 多终端运行与单步调试

如果要完成以下场景：

- 一个终端运行 `server`
- 一个终端使用 `gdb` 单步调试 `server`
- 另外一个或多个终端运行 `training_client`

那么不要只用：

```bash
dbus-run-session -- bash
```

因为它只对当前 shell 生效，不适合多个独立终端共享。

更适合的方法是手动启动一个共享的 session bus。

### 1. 在任意终端启动 bus

```bash
dbus-daemon --session --fork --print-address=1 --print-pid=1
```

它会输出两行内容，类似：

```text
unix:path=/tmp/dbus-xxxx,guid=yyyy
12345
```

第一行是 `DBUS_SESSION_BUS_ADDRESS`，第二行是 bus 进程的 pid。

### 2. 在所有需要参与联调的终端里设置同一个环境变量

```bash
export DBUS_SESSION_BUS_ADDRESS='unix:path=/tmp/dbus-xxxx,guid=yyyy'
```

### 3. 启动服务端

普通运行：

```bash
./build/ServiceProject/app/server
```

或使用 `gdb` 单步调试：

```bash
gdb ./build/ServiceProject/app/server
```

### 4. 在其他终端启动客户端

```bash
./build/ClientProject/training_client
```

可以同时打开多个终端运行多个 client，它们都会连接到同一个 D-Bus session。

### 5. 结束后关闭 bus

```bash
kill 12345
```

这里的 `12345` 换成前面输出的实际 pid。

## 命令行演示

`training_client` 提供了一个简单的命令行菜单，可以演示：

- `SetTestBool`
- `SetTestInt`
- `SetTestDouble`
- `SetTestString`
- `SetTestInfo`
- `GetTestBool`
- `GetTestInt`
- `GetTestDouble`
- `GetTestString`
- `GetTestInfo`
- `SendFilePath`

典型验证流程：

1. 在 client A 中执行 `SetTestInt`
2. 在 client B 中看到广播日志
3. 在 client B 中执行 `GetTestInt`
4. 读取到最新值

文件发送流程：

1. 在菜单里选择发送文件
2. 输入本地文件路径
3. client 分片写入共享内存
4. service 读取、组包、校验并保存

## 测试

项目当前包含以下回归测试：

- `client_wrapper_roundtrip`
- `file_transfer_roundtrip`
- `concurrent_file_transfer_roundtrip`
- `inheritable_client_roundtrip`

运行方式：

```bash
cd build
ctest --output-on-failure
```

## 可继承客户端

`TrainingClient` 被设计成可以继续继承，便于后续扩展 UI 或自定义行为。

子类可以重写：

- `OnTestBoolChanged`
- `OnTestIntChanged`
- `OnTestDoubleChanged`
- `OnTestStringChanged`
- `OnTestInfoChanged`

以及远端通知入口：

- `OnRemoteTestBoolChanged`
- `OnRemoteTestIntChanged`
- `OnRemoteTestDoubleChanged`
- `OnRemoteTestStringChanged`
- `OnRemoteTestInfoChanged`

这样后续如果接入 Qt 或其他界面层，可以直接在子类里接入自己的刷新逻辑，而不需要修改底层动态库。

## 当前项目特点

这个项目不是为了追求最少代码量，而是为了把几个实际工程里常见的问题串起来：

- 双工程组织方式
- 公共接口抽象
- `gdbus-codegen` 的使用
- 动态库封装与运行时加载
- 广播回调与本地缓存同步
- 共享内存文件传输
- 多终端联调
- 面向后续 UI 扩展的继承设计
