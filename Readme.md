# My_Network_Demo



为理解muduo设计的精妙之处，并学习如何达到高并发，用C++11重构muduo网络库，实现I/O复用机制EPoll的LT模式。重构的网络库去除muduo网络库对boost库的依赖。同时，内部实现了HTTP服务器，可支持GET请求和静态资源的访问。除此之外，还实现了数据库连接池，可动态管理数据库连接。

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

# 并发模型

有多个reactors（包括一个主reactor和多个子reactors）和线程池，多个子reactors共享一个线程池，每个线程独立运行一个事件循环，适用于突发I/O与密集计算。

![image.png](https://camo.githubusercontent.com/17f8e0405bb46021279fa038376b03087149fa955c6edde7abfe60d35f4e23c6/68747470733a2f2f63646e2e6e6c61726b2e636f6d2f79757175652f302f323032322f706e672f32363735323037382f313637303835333133343532382d63383864323766322d313061322d343664332d623330382d3438663736333261326630392e706e673f782d6f73732d70726f636573733d696d616765253246726573697a65253243775f3933372532436c696d69745f30)

1. 主reactor负责监听和派发新连接，监听和派发新连接通过Acceptor实现，当接收到新连接时，通过Acceptor以轮询的方式将新连接分派给子Reactor。
2. 子Reactor负责分派给它的连接的读写事件。每个子Reactor把它计算相关的操作放到线程池中进行。

**主从Reactor模型的优点：**

1. 响应快，不必为单个同步事件所阻塞，虽然 Reactor 本身依然是同步的；
2. 可以最大程度避免复杂的多线程及同步问题，并且避免多线程/进程的切换；
3. 扩展性好，可以方便通过增加 Reactor 实例个数充分利用 CPU 资源；
4. 复用性好，Reactor 模型本身与具体事件处理逻辑无关，具有很高的复用性；

# 主要类介绍

## Channel类

在TCP网络编程中，要以IO复用模型监听某个文件描述符，需要将该文件描述符和它感兴趣的事件注册到 epoll 对象中，然后由epoll 对象返回*发生事件的文件描述符的集合*以及*每个文件描述符都发生了什么事件*。

 为了方便维护，muduo 中的 Channel 类将文件描述符和其感兴趣的事件封装到了一起。事件监听相关的代码放到了 Poller/EPollPoller 类中。

Channel类封装了一个*文件描述符fd*和这个*fd感兴趣事件*，因此可以将这个Channel与这个文件描述符基本相等看待。

### 成员变量

```c++
 static const int kNoneEvent;
 static const int kReadEvent;
 static const int kWriteEvent; 

 EventLoop *loop_; //当前Channel属于的EventLoop
 const int fd_;//fd, Poller的监听对象
 int events_; //注册fd感兴趣的事件
 int revents_; //poller返回的具体发生的事件
 int index_; //在Poller上注册的情况

 //std::weak_ptr<void>是指向任何类型的弱指针
 std::weak_ptr<void> tie_;//弱指针指向TcpConnection
 bool tied_;//标记此Channel是否被调用过Channel::tie方法

 //因为channel通道里面能够获知fd最终发生的具体的事件
 //保存事件到来时的回调函数
 ReadEventCallback readCallback_;
 EventCallback writeCallback_;
 EventCallback closeCallback_;
 EventCallback errorCallback_;
```

- `kNoneEvent`、`kReadEvent`、`kWriteEvent`：事件状态设置会使用的变量

- `int fd_`这个Channel对象照看的文件描述符
- `int events_`代表fd感兴趣的事件类型集合
- `int revents_`代表事件监听器实际监听到该fd发生的事件类型集合，当事件监听器监听到一个fd发生了什么事件，通过`Channel::set_revents()`函数来设置revents值。
- `EventLoop* loop_`这个fd属于哪个EventLoop对象，用来避免跨线程处理函数
- `int index_`用来记录Channel和EPoller相关的几种状态

- `kNew`：是否还未被EPoller监视
- `kAdded`：是否已在被EPoller监视中
- `kDeleted`：是否已被移除
- `std::weak_ptr<void> tie_`用来指向TcpConnection，判断TcpConnection是否还存在
- `bool tied_`表示调用过Channel::tie方法，也就是tie_已经指向TcpConnection了

- `read_callback_` 、`write_callback_`、`close_callback_`、`error_callback_`：这些是std::function类型，代表着这个Channel为这个文件描述符保存的各事件类型发生时的处理函数。比如这个fd发生了可读事件，需要执行可读事件处理函数，这时候Channel类都替你保管好了这些可调用函数，要用执行的时候直接管Channel要就可以了。

### 成员函数

- **向Channel对象注册各类事件的处理函数**

```c++
void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }
```

- **将Channel感兴趣的事件注册到EPoller、从EPoller中删除**

  ```c++
  //向epoll中注册、删除fd感兴趣的事件
  void enableReading(){ events_ |= kReadEvent; update(); }
  void disableReading(){ events_ &= ~kReadEvent; update(); }
  void enableWriting(){ events_ |= kWriteEvent; update(); }
  void disableWriting(){ events_ &= ~kWriteEvent; update(); }
  void disableAll(){ events_ &= kNoneEvent; update(); }
  ```

- **返回fd当前的事件状态**

```c++
//返回fd当前的事件状态
bool isNoneEvent() const { return events_ == kNoneEvent; }
bool isWriting() const { return events_ == kWriteEvent; }//是否注册了可写事件
bool isReading() const { return events_ == kReadEvent; }//是否注册了可读事件
```

- **更新Channel关注的事件**

  ```c++
  /**
   * 更新fd感兴趣的事件，比如Channel刚创建时，需要让Channel管理的fd关注可读事件，此时需要调用Channel::enableReading()
   * 该函数内部再调用update()。update()的本质时在EPoller类中，调用了epoll_ctl，来实现对感兴趣事件的修改
   */
  void Channel::update() {
      //通过该Channel所属的EvenLoop，调用EPoller对应的方法，注册fd的events事件
      loop_->updateChannel(this);
  }
  ```

- **从EPoller中移除**

```c++
void Channel::remove() {
    //把channel从EPollPoller的channels_中移除掉，同时把这个channel的状态标记为kNew
    loop_->removeChannel(this);
}
```

- **防止用户误删操作**

```c++
void Channel::tie(const std::shared_ptr<void> &obj) {
    //tie_指向obj，可以用tie_来判断TcpConnection是否还活着
    tie_ = obj;
    tied_ = true;
}
```

由于用户可以看见TcpConnection，如果用户注册了要监听的事件和处理响应的回调函数，但误删了TcpConnection，EventLoop 不能继续运行下去。因此需要避免用户误删，在`TcpConnection::connectEstablished`中调用`Channel::tie`，也就是在channel内部增加了对TcpConnection对象的引用计数，使得用户误删之后，引用计数也不会降为0，从而保证TcpConnction对象还活着。

- 根据发生事件的不同，**Channel调用不同事件处理函数**，<u>借由回调操作实现了异步。</u>

`void handleEventWithGuard(Timestamp receiveTime);`

## Poller / EpollPoller类

### 基类Poller

muduo 以Poller 为基类，派生出 Poller 和 EPollPoller 两个子类，分别实现poll和epoll IO 复用。

#### 成员变量

```c++
ChannelMap channels_;//存储fd->channel的映射
EventLoop *ownerLoop_;//定义Poller所属事件循环
```
- `channels_`存储fd->channel的映射，在channels_中的元素是注册进epoll的文件描述符
- `ownerLoop_` 定义 Poller 所属的事件循环 EventLoop

#### 成员函数

- `poll`、`updateChannel`和`removeChannel`要在派生类中定义。

```c++
virtual Timestamp poll(int timeoutMs, ChannelList * activeChannels) = 0;
virtual void updateChannel(Channel *channel) = 0;
virtual void removeChannel(Channel *channel) = 0;
```

- **判断channel是否注册到poller中**

```c++
//判断channel是否注册到poller当中
bool hasChannel(Channel *channel) const;
```

- **默认的IO复用实现方式**

```c++
//EventLoop可以通过该接口获取默认的IO复用实现方式（默认epoll）
static Poller* newDefultPoller(EventLoop * Loop);
```

为了避免在Poller.cpp文件中包含派生类的头文件，新建一个.cpp文件写默认的定义

我们只实现了EPollPoller类（用来实现epoll）

```c++
Poller* Poller::newDefultPoller(EventLoop *Loop) {
    if(::getenv("MUDUO_USE_POLL")){//获取名为 MUDUO_USE_POLL 的环境变量的值
        return nullptr;//生成poll实例
    }else{
        return new EPollPoller(Loop);//生成epoll实例
    }
}
```

### EpollPoller类

继承Poller 类，实现epoll。

#### 成员变量

```c++
//using ChannelMap = std::unordered_map<int, Channel*>;
using EventList = std::vector<epoll_event>;

//默认监听事件的数量
static const int kInitEventListSize = 16;

//ChannelMap channels_;//存储channel的映射
//EventLoop * ownerLoop_;//EPoller所属事件循环EventLoop
int epollfd_;//每个EPollPoller都有一个epollfd_，epollfd_是epoll_create在内核创建空间返回的fid
EventList events_; //用于存放epoll_wait返回的所有发生的事件
```

除了继承的以外，

- `kInitEventListSize` 默监听事件的数量
- `epollfd_` 指向 epoll 对象的文件描述符（句柄）
- `EventList events_` 存放epoll_wait返回的所有发生的事件

#### 成员函数

- **要重写的成员函数**

```c++
Timestamp poll(int timeoutMs, ChannelList * activeChannels) override;//将有事件发生的channel通过activeChannels返回
void updateChannel(Channel * channel) override;//更新channel上的状态
void removeChannel(Channel * channel) override;//当连接销毁时，从EPoller移除channel
```

`poll`用::epoll_wait获取发生的事件，如有事件发生，则将时间对应的Channel放入activeChannels

`updateChannel`更新channel在epoll上的状态，

1. 对于未添加和已删除

   - 未添加的在channels_上增加映射，

   - 已删除的直接往后执行，

2. 最后将状态改为已添加。

而对于已添加的，判断是否还有没有感兴趣事件，没有则更新，将状态修改为已删除，有则只更新。

`removeChannel`从EPoller移除channel，如果fd已经被添加到EPoller中了，用update更新删除，最后将状态设置为未注册。

- `update`操作，其本质是调用 `epoll_ctl` 函数，其中`operation`由`updateChannel` 所指定。

```c++
void EPollPoller::update(int operation, Channel *channel) {
    epoll_event event;
    ::memset(&event, 0, sizeof(event));
	int fd = channel->fd();
	event.events = channel->events();
	event.data.fd = fd;
	event.data.ptr = channel;

	if(::epoll_ctl(epollfd_, operation, fd, &event) < 0){
    	if(operation == EPOLL_CTL_DEL){
        	LOG_ERROR << "epoll_ctl() del error:" << errno;
    	}else{
        	LOG_FETAL << "epoll_ctl add/mod error:" << errno;
    	}
	}
}
```
- **填写活跃的连接**

```c++
void EPollPoller::fillActiveChannels(int numEvents, EPollPoller::ChannelList *activeChannels) const {
    for(int i = 0; i < numEvents; ++i){
        Channel *channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activeChannels->emplace_back(channel);
    }
}
```

通过 epollwait 返回的 events 数组内部有指向 channel 的指针，通过此指针在 EPollPoller 模块对 channel 进行操作。 我们需要更新 channel 的返回的事件，并且将此 channel 装入 activeChannels。

### EventLoop类

作为一个网络服务器，需要有持续监听、持续获取监听结果、持续处理监听结果对应的事件的能力，也就是我们需要**循环**的去 调用`Poller:poll`方法获取实际发生事件的Channel集合，然后调用这些Channel里面保管的不同类型事件的处理函数（调用`Channel::HandlerEvent`方法）。

EventLoop类就负责实现这个循环，起到一个驱动循环的功能。

### 成员变量

```c++
using ChannelList = std::vector<Channel*>;std::atomic_bool looping_; //是否正在事件循环中

std::atomic_bool quit_;//是否退出事件循环
std::atomic_bool callingPendingFunctiors_;//是否正在调用待执行的函数
const pid_t threadId_;//当前loop所属线程的id
Timestamp epollReturnTime_;//EPoller管理的fd有事件发生时的时间
std::unique_ptr<EPollPoller> epoller_;//管理当前loop所有EPollPoller的容器
std::unique_ptr<TimerQueue> timerQueue_;//管理当前loop所有定时器的容器

//wakeupFd_用于唤醒EPoller，以免EPoller阻塞了无法执行PendingFunctiors_中的待处理的函数
int wakeupFd_;
std::unique_ptr<Channel> wakeupChannel_;//wakeuoFd_对应的Channel
ChannelList activeChannels_;//有事件发生的Channel集合
std::mutex mutex_;//用于保护pendingFunctors_线程安全操作
std::vector<Functor> pendingFunctors_;//存储loop跨线程需要执行的所有回调操作
```

- `looping_`、`quit_`和`callingPendingFunctiors_`是原子类型

- `wakeupFd_`用于唤醒EPoller，如果需要唤醒某个`EventLoop`执行异步操作，就向这个`EventLoop`的`wakeupFd_`写入数据。
- `wakeupChannel_`wakeuoFd_对应的Channel
- `activeChannels_`有事件发生的Channel集合，用来保存调用`poller_->poll`时会得到的发生了事件的`Channel`
- `pendingFunctors_`存储loop跨线程需要执行的所有回调操作

### 成员函数

- **构造函数**

```c++
EventLoop::EventLoop() :
looping_(false),
quit_(false),
callingPendingFunctiors_(false),
threadId_(CurrentThread::tid()),
epoller_(new EPollPoller(this)),
timerQueue_(new TimerQueue(this)),
wakeupFd_(createEventfd()),
wakeupChannel_(new Channel(this, wakeupFd_)){
    LOG_DEBUG << "EventLoop created " << this <<  ", the threadId is " << threadId_;
    if(t_loopInThisThread){
        LOG_FETAL << "Another EventLoop " << t_loopInThisThread << " exists int this thread" << threadId_;
    }else{
        t_loopInThisThread = this;
    }
    //void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
    //using ReadEventCallback = std::function<void(Timestamp)>;
    //void EventLoop::handleRead()类型是否不一致
    //std::function<void()>可以传递给std::function<void(Timestamp)>
    // 只要保证函数运行时不需要这个参数
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));

    wakeupChannel_->enableReading();
}
```

`wakeupFd_(createEventfd())`调用的`createEventfd`的函数如下：

```c++
int createEventfd(){
    //eventfd是一种事件通知机制，
    int evfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if(evfd < 0) LOG_FETAL << "eventfd error: " << errno;
    else LOG_DEBUG << "create a new wakeupfd, fd = " << evfd;
    return evfd;
}
```

其中，指定为`EFD_NONBLOCK`非阻塞，一边wakeupFd_唤醒`EventLoop`执行异步操作。

`t_loopInThisThread`声明时__thread，是线程本地的变量，用来防止一个线程创建多个EventLoop

`wakeupChannel_`设置的可读事件的回调函数如下：

```c++
//wakeupChannel_可读事件的回调函数
void EventLoop::handleRead() {
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof(one));
    if(n != sizeof(one)){
        LOG_ERROR << "EventLoop::handleRead() reads " << n << " bytes instead of 8";
    }
}
```

- **析构函数**

```c++
EventLoop::~EventLoop(){
    wakeupChannel_->disableAll();//移除感兴趣的事件
    wakeupChannel_->remove();//从EPollPoller中删除，同时让epollfd不再关注wakeupfd_上发生的任何事件
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}
```

EventLoop销毁时，令wakeupChannel不关注任何事件，将wakeupChannel从epoll中移除，关闭文件描述符wakeupFd_，将t_loopInThisThread置空

- **判断是否跨线程调用移除wakeupChannel_感兴趣的事件**

```c++
//判断EventLoop是否在自己的线程
bool isInLoopThread() const { return  threadId_ == CurrentThread::tid(); }
```

1. 每个线程中只有一个`Reactor`
2. 每个`Reactor`中都只有一个`EventLoop`
3. 每个`EventLoop`被创建时都会保存创建它的线程值。

- **核心驱动loop()**

```c++
void EventLoop::loop() {
    looping_ = true;
    quit_ = false;
    LOG_INFO << "EventLoop " << this << " start looping";
    while(!quit_){
        activeChannels_.clear();
        epollReturnTime_ = epoller_->poll(kPollTimeMs, &activeChannels_);
        for(Channel *channel : activeChannels_){
            channel->handleEvent(epollReturnTime_);
        }
        doPendingFunctors();
    }
    looping_ = false;
}
```

在驱动内部：

1. 调用 `epoller_->poll(kPollTimeMs, &activeChannels_)` 将活跃的 Channel 填充到 activeChannels 容器中。
2. 遍历 activeChannels 调用各个事件的回调函数
3. 调用 `doPengdingFunctiors()`处理跨线程调用的回调函数

- **主动关闭事件循环**

```c++
void EventLoop::quit() {
    quit_ = true;
    //由void EventLoop::loop() 的代码可以看到，若当前loop没有任何事件发生，
    //会阻塞在epoller_->poll，因此需要向wakeupFd_写入数据，以解除阻塞
    if(!isInLoopThread()) wakeup();
}
```

 从loop()函数可以看出，要退出事件循环，需要再次执行到while处，而已经令`quit_ = true`，从而退出循环。

但存在阻塞在epoller_->poll的可能，为解决这种情况，需要调用wakeup()向wakeupFd写入数据

每个`EventLoop`的`wakeupFd_`都被加入到`epoll`对象中，只要写了数据就会触发读事件，`epoll_wait `就会返回

```c++
void EventLoop::wakeup() {
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof(one));
    if(n != sizeof(one)){
        LOG_ERROR << "EventLoop::wakeup wtires " << n << " bytes instead of 8";
    }
}
```

- **执行分派任务**

```c++
void EventLoop::runInLoop(EventLoop::Functor cb) {
    //如果当前调用runInLoop的线程正好是EventLoop绑定的线程，则直接执行此函数
    //否则就将回调函数通过queueInLoop()存储到pendingFunctors_中
    if(isInLoopThread()){
        cb();
    }else{
        queueInLoop(cb);
    }
}
```

若在本线程中，直接执行（同步调用），否则执行`queueInLoop`是跨线程调用。

- **保证线程安全**

```c++
void EventLoop::queueInLoop(EventLoop::Functor cb) {//把回调函数放入pendingFunctors_并在必要的时候唤醒EventLoop绑定的线程
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }
    //在执行回调函数的过程中也可能会向pendingFunctors_中添加新的回调，
    // 若此时不进行唤醒，会发生有时间到来但是仍被阻塞的情况
    // 而callingPendingFunctiors_正在调用待执行的函数，即在执行回调函数，
    // 因此callingPendingFunctiors_为true时应进行唤醒
    if(!isInLoopThread() || callingPendingFunctiors_){
        wakeup();
    }
}
```

1. 对于跨线程调用的任务执行，使用`queueInLoop`函数，将此任务加入到任务队列中，并唤醒应当执行此任务得线程。原线程唤醒其他线程之后，就可以继续执行别的操作了，因此这是一个异步的操作。
2. 因为多个线程都可以调用`loop->queueInLoop(cb)`，因此，对`pendingFunctors_`操作的时候需要加锁，我们这里使用了在局部区域用RALL手法生成一个互斥锁。
3. 从下面的判断语句中，我们可以看到有两种情况需要唤醒，一种是跨线程，另一种是EventLoop正在处理当前的PendingFunctors函数时有新的回调函数加入，倘若不唤醒，那么新加入的函数就不会得到处理，会因为下一轮的 epoll_wait 而继续阻塞住。

- **更新channel感兴趣的事件**

```c++
void EventLoop::updateChannel(Channel *channel) {
    epoller_->updateChannel(channel);
}
```

更新这个事件循环中的EPoller中channel的状态

- **移除channel**

```c++
void EventLoop::removeChannel(Channel *channel) {
    epoller_->removeChannel(channel);
}
```

一个loop对应一个epoller，让这个事件循环中的EPoller以后不在关注该channel上的事件

- **判断channel是否在该事件循环的EPoller中**

```c++
bool EventLoop::hasChannel(Channel *channel) {
    return epoller_->hasChannel(channel);
}
```

- **定时器相关函数**

```c++
void EventLoop::runAt(Timestamp timestamp, EventLoop::Functor &&cb) {
    timerQueue_->addTimer(std::move(cb), timestamp, 0.0);
}
```

在time时刻执行回调函数cb

```c++
void EventLoop::runAfter(double waitTime, EventLoop::Functor &&cb) {
    Timestamp time(addTime(Timestamp::now(), waitTime));
    runAt(time, std::move(cb));
}
```

在waitTime秒后执行回调函数cb

```c++
void EventLoop::runEvery(double interval, EventLoop::Functor &&cb) {
    Timestamp timestamp(addTime(Timestamp::now(), interval));
    timerQueue_->addTimer(std::move(cb), timestamp, interval);
}
```

每隔interval秒执行一次回调函数cb

- ## **处理 pendingFunctors 里储存的函数**

```c++
void EventLoop::doPendingFunctors() {
    std::vector<Functor> functors;
    callingPendingFunctiors_ = true;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);//交换的方式减少了锁的临界区范围，提升效率、避免死锁
    }
    for(const Functor &functor : functors){
        functor();
    }
    callingPendingFunctiors_ = false;
}
```

1. `callingPendingFunctiors_ = true;`为了应对loop正在处理当前的PendingFunctors函数时有新的回调函数加入的情况。
2. 用互斥锁保护一个交换的操作，锁的粗粒度小
3. 遍历执行每一个函数

## Acceptor类

1. Accetpor封装服务器监听套接字fd，完成主Reactor轮询分派新连接等任务，其主要是对其他类的方法调用进行封装。
2. 只运行在主线程

### 成员变量

```c++
EventLoop *loop_;
Socket acceptSocket_;
Channel acceptChannel_;
NewConnectionCallback newConnectionCallback_;
bool listenning_;//是否正在监听
int idleFd_; //防止fd数量超过上限，用于占位的fd
```

- `loop_`Accetpor所在的事件循环，一般是mainloop

- `acceptSocket_`服务器监听套接字的文件描述符
- `acceptChannel_`：Channel类，把`acceptSocket_`及其感兴趣事件和事件对应的处理函数都封装进去。
- `newConnectionCallback_:`处理新连接的回调函数。
  -  TcpServer构造函数中将`TcpServer::newConnection( )`函数注册给了这个成员变量。这个`TcpServer::newConnection`函数的功能是公平的选择一个subEventLoop，并把已经接受的连接分发给这个subEventLoop。

- `listenning_`是否正在监听的标志
- `idleFd_`用来解决`EMFILE`的占位符

### 成员函数

- **构造函数**

```c++
Acceptor::Acceptor(EventLoop *loop, const InetAddress &ListenAddr, bool reuseport) :
loop_(loop),
acceptSocket_(createNonblocking()),
acceptChannel_(loop, acceptSocket_.fd()),
listenning_(false){
    LOG_DEBUG << "Acceptor create nonblocking socket, [fd = " << acceptChannel_.fd() << "]";
    acceptSocket_.setReuseAddr(reuseport);//允许在同一个端口和地址重新绑定一个套接字
    acceptSocket_.setReusePort(true);//允许多个套接字绑定到同一个地址和端口
    acceptSocket_.bindAddress(ListenAddr);

    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}
```

将`Acceptor::handleRead`注册到`acceptChannel_`上，`handleRead()`内部还调用了`newConnectionCallback_`保存的函数，用来分发新到来的连接。具体的分发写在`NewConnectionCallback_`回调函数中的

```c++
void Acceptor::handleRead() {
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    if(connfd >= 0){//确实有新连接到来
        if(newConnectionCallback_){
            newConnectionCallback_(connfd, peerAddr);
        }else{
            LOG_DEBUG << "no newConnectionCallback() function";
            ::close(connfd);
        }
    }else{
        LOG_ERROR << "accept() failed";
        //EMFILE表示当前进程打开的文件描述符已经到达系统或进程的上限，
        //可以调整单个服务器的上限，也可以分布式部署
        if(errno == EMFILE){
            LOG_INFO << "sofd reached limit";
            ::close(idleFd_);
            idleFd_ = ::accept(acceptSocket_.fd(), NULL, NULL);
            ::close(idleFd_);
            idleFd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
        }
    }
}
```

- **析构函数**

```c++
Acceptor::~Acceptor(){
    acceptChannel_.disableAll();
    acceptChannel_.remove();
    ::close(idleFd_);
}
```

`Acceptor`销毁时，令`acceptChannel_`不关注任何事件，将`acceptChannel_`从epoll中移除，关闭文件描述符`idleFd_`

- **开启监听**

```c++
void Acceptor::listen() {
    listenning_ = true;
    acceptSocket_.listen();
    //将acceptChannel的读事件注册到poller
    acceptChannel_.enableReading();
}
```

让mainloop监听acceptSocket_。`acceptSocket_.listen()`中调用linux的函数`listen()`，开启`acceptSocket_`的监听同时将`acceptChannel`及其感兴趣事件（可读事件）注册到main EventLoop的事件监听器上。

**设置新连接到来的回调函数**

```c++
void setNewConnectionCallback(const NewConnectionCallback &cb){
    newConnectionCallback_ = cb;
}
```

- **返回监听状态**

```c++
bool listenning() const { return listenning_; }
```

## Socket类

```c++
class Socket : noncopyable {
public:
    explicit Socket(int sockfd) : sockfd_(sockfd){};
    ~Socket();
    //获取sockfd
    int fd() const { return sockfd_; }
    //绑定sockfd
    void bindAddress(const InetAddress& lockaddr);
    //使sockfd为可接受连接状态
    void listen();
    //接受连接
    int accept(InetAddress * peeraddr);
    //设置半关闭
    void shutdownWrite();

    void setTcpNoDelay(bool on); //设置Nagel算法
    void setReuseAddr(bool on);//设置地址复用
    void setReusePort(bool on);//设置端口复用
    void setKeepAlive(bool on);//设置长连接
private:
    const int sockfd_;
};
```

- 只有一个成员变量`sockfd_`
- 该类用于管理TCP连接对应的sockfd的生命周期并提供一些函数来修改sockfd上的选项

## Buffer类

Buffer类封装了一个缓冲区以及向这个缓冲区写数据读数据的一系列方法。

### 为什么要设计缓冲区

> 非阻塞网络编程中应用层buffer是必须的
> 原因：非阻塞IO的核心思想是避免阻塞在read()或write()或其他IO系统调用上，这样可以最大限度复用thread-of-control，让一个线程能服务于多个socket连接。IO线程只能阻塞在IO-multiplexing函数上，如select()/poll()/epoll_wait()。这样一来，应用层的缓冲是必须的，每个TCP socket都要有input buffer和output buffer。
> TcpConnection必须有output buffer
> 原因：使程序在write()操作上不会产生阻塞，当write()操作后，操作系统一次性没有接受完时，网络库把剩余数据则放入output buffer中，然后注册POLLOUT事件，一旦socket变得可写，则立刻调用write()进行写入数据。——应用层buffer到操作系统buffer
> TcpConnection必须有input buffer
> 原因：当发送方send数据后，接收方收到数据不一定是整个的数据，网络库在处理socket可读事件的时候，必须一次性把socket里的数据读完，否则会反复触发POLLIN事件，造成busy-loop。所以网路库为了应对数据不完整的情况，收到的数据先放到input buffer里。——操作系统buffer到应用层buffer
> ————————————————

原文链接：https://blog.csdn.net/qq_36417014/article/details/106190964

### TcpConnection如何使用Buffer

TcpConnection 拥有 inputBuffer 和 outputBuffer 两个缓冲区成员。

1. 当服务端接收客户端数据（从客户端sock读取数据写到inputBuffer），TcpConnection 调用 handleRead 方法从相应的 fd 中读取数据到 inputBuffer 中。在 Buffer 内部 inputBuffer 中的 writeIndex 向后移动。
2. 当服务端向客户端发送数据（从ouputBuffer中读数据到socket中），也就是用outputBuffer。TcpConnection调用 handleWrite 方法将 outputBuffer的数据写入到 TCP 发送缓冲区。outputBuffer内部调用 `retrieve` 方法移动readIndex索引。

### Buffer的设计思路

**读写配合+缓冲区内部调整+动态扩容**（图片来自网络）

![img](https://pic3.zhimg.com/v2-8afb7ea83f097bac4fdd1602f6bb253e_r.jpg)

### 成员变量

```c++
class Buffer {
public:
    static const size_t kCheapPrepend = 8; // 头部预留8个字节
    static const size_t kInitialSize = 1024; // 缓冲区初始化大小 1KB
    explicit Buffer(size_t initialSize = kInitialSize) :
    buffer_(kCheapPrepend + initialSize), readerIndex_(kCheapPrepend), writerIndex_(kCheapPrepend){}
    /*
     * | kCheapPrepend | reder | writer |
     */
    size_t readableBytes() const { return writerIndex_ - readerIndex_; }//可读的大小
    size_t writableBytes() const { return buffer_.size() - writerIndex_; };//可写的大小
    size_t prependableBytes() const { return readerIndex_; }//已读的大小
    const char* peek() const { return begin() + readerIndex_; }//返回缓冲区中可读数据的起始地址
private:
    /**
     * 借助vector实现自动分配内存
     */
     char* begin(){
        return &(*buffer_.begin());//获取buffer_的起始地址
     }
     const char* begin() const{
         return &(*buffer_.begin());//先调用begin()获得首个元素的迭代器，然后再解引用得到这个变量，再取地址
     }
     void makeSpace(int len);
     std::vector<char> buffer_;
     size_t readerIndex_;
     size_t writerIndex_;
     static const char kCRLF[];
};
```

### 重要的成员方法

- **向Buffer写入数据**`ssize_t readfd(int fd, int* saveErrno)`

这里的readfd的read是指的从socket中读，然后写入缓冲区

```c++
ssize_t Buffer::readfd(int fd, int *saveErrno) {
    char extrabuf[65536] = {0};//栈额外空间，用于从套接字往出读，空间不够时，暂存数据
    struct iovec vec[2];
    const size_t writable = writableBytes();

    //第一块缓冲区，指向可写空间
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;

    //第二块缓冲区，指向栈空间
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf);

    //如果缓冲区比额外空间还大，就只用缓冲区，不然则都用
    const int iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1;

    /**
     * ssize_t readv(int fd, const iovec *iov, int iovcnt);
     * 散布读
     * iovcnt 表示缓冲区的个数
     */
    const ssize_t n = ::readv(fd, vec, iovcnt);

    if(n < 0){
        *saveErrno = errno;
    }else if(n <= writable){
        writerIndex_ += n;
    }else{
        writerIndex_ = buffer_.size();
        append(extrabuf, n - writable);
    }
    return n;
}

```

1. 由于缓冲区大小是一定的，但从fd上读数据的时候不知道tcp数据的大小，因此从socket读到缓冲区的方法是使用readv先读至`buffer_`，如果空间不够，会读到栈上65536个字节大小的空间，然后以append的方式追加入`buffer_`。
2. 散布读`ssize_t readv(int fd, const iovec *iov, int iovcnt);`更适合多个缓冲区

其中`append`函数如下：

```c++
void append(const char* data, size_t len){
    //添加数据到writable缓冲区中
    enseureWritableBytes(len);
    std::copy(data, data + len, beginWrite());
    writerIndex_ += len;
}
```

其中`enseureWritableBytes`用来确保可写空间足够，具体如下：

```c++
void enseureWritableBytes(size_t len){
    if(writableBytes() < len){
        makeSpace(len);
    }
}
```

当空间不够时，自动**扩容**

```c++
void makeSpace(int len){//调整可写的空间，即存放socket可读数据的空间
         //扩容
         /*     | kCheapPrepend | xxx | reder | writer |
          * 变为 | kCheapPrepend | reder |         len        |
          */
         if(writableBytes() + prependableBytes() < len + kCheapPrepend){
             buffer_.resize(writerIndex_ + len);
         }else{//调整buffer的两个游标
             /**
             * +-----------------------+------------------+----------------------+
             * |       preoendable     |  readable bytes  |    writable bytes    |
             * | kCheapPrepend | p-kC  |    (CONTENT)     |                      |
             * +---------------+-------+------------------+----------------------+
             * |               |       |  服务端要发送的数据  | 从socket读来数据存放于此|
             * 0               8 <= readerIndex   <=    writerIndex      <=      size
             */
             //调整为
             /**
             * +---------------+------------------+------------------------------+
             * |  preoendable  |  readable bytes  |      新的writable bytes       |
             * | kCheapPrepend |    (CONTENT)     | = p-kC + 之前的writable bytes |
             * +---------------+------------------+-----------------------------+
             * |               |  服务端要发送的数据  |    从socket读来数据存放于此     |
             * 0          readerIndex   <=    writerIndex          <=          size
             */
             size_t readable = readableBytes();
             std::copy(begin() + readerIndex_,
                       begin() + writerIndex_,
                       begin() + kCheapPrepend);
             readerIndex_ = kCheapPrepend;
             writerIndex_ = readerIndex_ + readable;
         }
}
```

- **从Buffer读数据**

```c++
ssize_t Buffer::writeFd(int fd, int *saveErrno) {//将缓冲区中的可读数据写入fd
    ssize_t n = ::write(fd, peek(), readableBytes());
    if(n < 0){
        *saveErrno = errno;
    }
    return n;
}
```

可能还会用到的两个读数据的方法

```c++
	void retrieveUntil(const char* end){
        assert(peek() <= end);
        assert(end <= beginWrite());
        retrieve(end - peek());
    }
    void retrieve(size_t len){
        if(len < readableBytes()){
            readerIndex_ += len;
        }else{
            retrieveAll();
        }
    }
    void retrieveAll(){
        readerIndex_ = kCheapPrepend;
        writerIndex_ = kCheapPrepend;
    }
    std::string retrieveAllAsString(){ return retrieveAllAsString(readableBytes()); }
    std::string retrieveAllAsString(size_t len){
        std::string result(peek(), len);
        retrieve(len);
        return result;
    }
```

1. `void retrieveUntil(const char* end)`：一直读到`end`
2. `void retrieve(size_t len)`：读取len长度数据
3. `void retrieveAll()`：读取所有数据
4. `std::string retrieveAllAsString()`：将可读取的数据按照string类型全部取出
5. `std::string retrieveAllAsString(size_t len)`：读取len长度数据按照string类型取出

## TcpConnection类

封装了一个***已建立的***TCP连接、该TCP连接的服务端和客户端的套接字地址信息、该连接发生的读/写/错误/连接事件对应的处理函数以及控制该TCP连接的方法。

1. Acceptor用于main EventLoop中
2. TcpConnection用于SubEventLoop中

### 成员变量

```c++
    EventLoop *loop_;//属于哪个subloop（单线程则为baseloop）
    const std::string name_;
    std::atomic_int state_;//连接状态
    bool reading_;

    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_;//服务器地址
    const InetAddress peerAddr_;//对端地址

    /*
     * 用户自定义的这些事件的处理函数，然后传递给TcpServer
     */
    ConnectionCallback connectionCallback_;//有新连接时的回调
    MessageCallback messageCallback_;//有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_;//消息发送完成以后的回调
    CloseCallback closeCallback_;//客户端关闭连接的回调
    HighWaterMarkCallback highWaterMarkCallback_;//超出水位实现的回调
    size_t hightWaterMark_;

    Buffer inputBuffer_;//接收数据的缓冲区
    Buffer outputBuffer_;//发送数据的缓冲区
```

- `loop_`：这是一个`EventLoop*`类型，该Tcp连接的Channel注册到了哪一个subloop上。这个loop_对应着那个subloop
- `name_`：连接的名字
- `state_`：标识了当前TCP连接的状态（Connected、Connecting、Disconnecting、Disconnected）
- 
- `socket_`：用于保存已连接套接字文件描述符。
- `channel`_：封装了上面的`socket_`及其各类事件的处理函数（读、写、错误、关闭等事件处理函数）。**这个Channel种保存的各类事件的处理函数是在TcpConnection对象构造函数中注册的。**
- `localAddr_`：服务器地址
- `peerAddr_`：对端地址
- 
- `connetionCallback`_、`messageCallback_`、`writeCompleteCallback_`、`closeCallback_` ： 分别用来保存四个回调函数，分别是 [连接建立/关闭**后**的处理函数] 、[收到消息**后**的处理函数]、[消息发送完**后**的处理函数]和[连接关闭**后**的处理函数]。其中， [连接建立/关闭**后**的处理函数] 、[收到消息**后**的处理函数]、[消息发送完**后**的处理函数]为用户自定义的，[连接关闭**后**的处理函数]为Muduo库中定义的。
- `highWaterMarkCallback_`：超出水位实现的回调
- `inputBuffer_`：这是一个Buffer类，是该TCP连接对应的用户接收缓冲区。
- `outputBuffer_`_：也是一个Buffer类，不过是用于暂存那些暂时发送不出去的待发送数据。因为Tcp发送缓冲区是有大小限制的，假如达到了高水位线，就没办法把发送的数据通过send()直接拷贝到Tcp发送缓冲区，而是暂存在这个outputBuffer_中，等TCP发送缓冲区有空间了，触发可写事件了，再把outputBuffer_中的数据拷贝到Tcp发送缓冲区中。

### 成员函数

- **构造函数**

```c++
TcpConnection::TcpConnection(EventLoop *loop, const std::string &nameArg, int sockfd, const InetAddress &localAddr,
                             const InetAddress &peerAddr) :
                             loop_(loop),
                             name_(nameArg),
                             state_(kConnecting),
                             reading_(true),
                             socket_(new Socket(sockfd)),
                             channel_(new Channel(loop, sockfd)),
                             localAddr_(localAddr),
                             peerAddr_(peerAddr),
                             hightWaterMark_(64 * 1024 * 1024){//64M
    //给channel设置回调函数
    //std::placeholders::_1是占位符
    channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(std::bind(&TcpConnection::handleError, this));

    LOG_INFO << "TcpConnection::dtor[" <<name_.c_str() <<"] at fd = " << sockfd;
    socket_->setKeepAlive(true);
}
```

在TcpConnection对象构造函数中注册`channel_`的各种事件处理函数，各个事件处理函数具体如下：

```c++
void TcpConnection::handleRead(Timestamp receiveTime) {
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readfd(channel_->fd(), &savedErrno);
    if(n > 0){
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }else if(n == 0) {//对方关闭了连接
        handleClose();
    }else{
        errno = savedErrno;//errno是一个全局变量
        LOG_ERROR << "TcpConnection::handleRead() failed";
        handleError();
    }
}

void TcpConnection::handleWrite() {
    if(channel_->isWriting()){
        int savedErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedErrno);
        if(n > 0){
            outputBuffer_.retrieve(n);//把outputBuffer_的readerIndex往前移动n个字节，
            // 因为outputBuffer_中的readableBytes已经发送出去了n字节
            if(outputBuffer_.readableBytes() == 0){
                channel_->disableWriting();//数据发送完毕以后，注销写事件
                if(writeCompleteCallback_){
                    loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
                }
                if(state_ == kDisconnecting){
                    shutdownInLoop();
                }
            }
        }else{
            LOG_ERROR << "TcpConnection::handleWrite() failed";
        }
    }else{
        LOG_ERROR << "TcpConnection fd = " << channel_->fd() << " is down, no more writing";
    }
}

void TcpConnection::handleClose() {
    setState(kDisconnected);
    channel_->disableAll();
    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr);//
    closeCallback_(connPtr);
}

void TcpConnection::handleError() {
    int optval;
    socklen_t  optlen = sizeof(optval);
    int err = 0;
    //获取并清除socket的错误状态
    if(::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0){
        err = errno;
    }else{
        err = optval;
    }
    LOG_ERROR << "TcpConnection::handleError name:" << name_.c_str() << " - SO_ERROR:" << err;
}
```

- **send**

```c++
void TcpConnection::send(const std::string &buf){
    if(state_ == kConnected){
        if(loop_->isInLoopThread()){
            sendInLoop(buf.c_str(), buf.size());
        }else{
            //void(TcpConnection::*)(const std::string&)是一个成员函数指针
            //表示TcpConnection中的一个成员函数，这个成员函数的返回类型是void，并且接受一个const std::string&类型的参数
            loop_->runInLoop(std::bind((void(TcpConnection::*)(const std::string&))&TcpConnection::sendInLoop, this, buf));
        }
    }
}

void TcpConnection::send(Buffer *buf) {
    if(state_ == kConnected){
        if(loop_->isInLoopThread()){
            sendInLoop(buf->retrieveAllAsString());
        }else{
            std::string msg = buf->retrieveAllAsString();
            loop_->runInLoop(std::bind((void(TcpConnection::*)(const std::string&))&TcpConnection::sendInLoop, this, msg));
        }
    }
}
```

1. public，用来给用户使用
2. 利用`loop_->isInLoopThread()`、`loop_->runInLoop`以及`TcpConnection::sendInLoop`保证线程安全

```c++
void TcpConnection::sendInLoop(const void *message, size_t len) {
    ssize_t nwrote = 0; //已经发送的数据长度
    size_t remaining = len; //还剩下多少数据需要发送
    bool faultError = false; //记录是否产生错误

    if(state_ == kDisconnected){//已经断开连接
        LOG_ERROR << "disconnected, give up writing";
        return;
    }
    //刚初始化的channel和数据发送完毕的channel都是没有可写事件在epoll上的，即isWriting返回false
    //channel第一次写数据，且缓冲区没有待发送数据
    if(!channel_->isWriting() && outputBuffer_.readableBytes() == 0){
        nwrote = ::write(channel_->fd(), message, len);
        if(nwrote >= 0){
            remaining = len - nwrote;
            if(remaining == 0 && writeCompleteCallback_){
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }else{
            nwrote = 0;
            if(errno != EWOULDBLOCK) {//EWOULDBLOCK表示非阻塞情况下没有数据后的正常返回 等同于EAGAIN
                LOG_ERROR << "TcpConnection::sendInLoop";
                if(errno == EPIPE || errno == ECONNRESET){//EPIPE表示管道破裂，ECONNRESET表示连接被对端重置
                    faultError = true;
                }
            }
        }
    }
    //一次性没有发送完数据，剩余数据需要保存到缓冲区中，且需要channel注册写事件
    if(!faultError && remaining > 0){
        size_t oldlen = outputBuffer_.readableBytes();//目前发送缓冲区剩余的待发送的数据的长度
        //判断待写数据是否会超过设置的高标志位
        if(oldlen + remaining >= hightWaterMark_ && oldlen < hightWaterMark_ && highWaterMarkCallback_){
            loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldlen + remaining));
        }
        outputBuffer_.append((char *)message + nwrote, remaining);
        if(!channel_->isWriting()){
            channel_->enableWriting();//若不注册，poller不会给channel通知epollout
        }
    }
}
```

1. 该函数先检查该连接上是否还有上次没发送完的数据如果发送完毕了（即`outputBuffer_.readableBytes() == 0`），则直接调用`::write`系统调用发送给对方，如果数据没有`::write`完毕，则直接把剩余的数据存储到outputBuffer_中，同时告诉EPoller关注该连接的可写事件，便于下次接着发送刚才没发送完的数据。关注该连接的可写事件后，有可写事件，则会调用`TcpConnection::handleWrite`函数。也就是说`TcpConnection::handleWrite`函数负责把剩余未发送的数据（即`outputBuffer_`中的数据）发送出去
2. 发送数据是可以在任意线程发送的，即`send`函数可能被跨线程调用，因此，跨线程时用`loop_->queueInLoop`保证one loop per thread。

- **建立连接**

```c++
void TcpConnection::connectEstablished() {
    setState(kConnected);

    channel_->tie(shared_from_this());
    channel_->enableReading();//向EPoller注册channel的EPOLLIN读事件，
    // EPOLLIN在网络编程中通常表示可以从套接字中读取数据或者文件可以读取
    // 可以想成服务器接收数据

    //新连接建立 执行回调
    connectionCallback_(shared_from_this());
}
```

1. 设置连接状态
2. `channel_->tie(shared_from_this());`避免用户误删连接
3. `channel_->enableReading()`注册读事件
4. `connectionCallback_(shared_from_this())`执行回调

- **关闭连接**

```c++
void TcpConnection::shutdown() {
    if(state_ == kConnected){
        setState(kDisconnecting);
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
    }
}

void TcpConnection::shutdownInLoop() {
    if(!channel_->isWriting()){//说明当前outputBuffe_的数据全部向外发送完成
        socket_->shutdownWrite();
    }
}
```

1. 设置连接状态为kDisconnecting

2. 保证线程安全的调用shutdownInLoop

3. 若全部发送完，则关闭套接字的文件描述符

4. 在`void TcpConnection::handleWrite()`有

   ```c++
                   if(state_ == kDisconnecting){
                       shutdownInLoop();
                   }
   ```

   即保证了发送完毕后关闭连接

- **销毁链接**

```c++
void TcpConnection::connectDestroyed() {
    if(state_ == kConnected){
        setState(kDisconnected);
        channel_->disableAll();
        connectionCallback_(shared_from_this());
    }
    channel_->remove();
}
```

会直接关闭连接，不考虑是否读完

# 使用cpp98编写的muduo对boost库的依赖

Muduo 主要依赖于 Boost 的以下几个组件：

1. **Boost.Asio**：用于异步网络和并发编程。Muduo 使用 Boost.Asio 提供的异步 I/O 操作来实现其非阻塞网络通信。
2. **Boost.Function**：提供通用的函数对象包装器。Muduo 使用它来存储和调用各种回调函数。
3. **Boost.Bind**：用于绑定函数和参数。Muduo 使用它来简化回调函数的参数绑定。
4. **Boost.Thread**：提供跨平台的线程管理。Muduo 使用 Boost.Thread 来处理多线程环境下的任务调度和同步。
5. **Boost.SmartPtr**：提供智能指针，包括 `shared_ptr` 和 `weak_ptr`。Muduo 使用智能指针来管理对象的生命周期，避免内存泄漏。
6. **Boost.NonCopyable**：提供不可复制对象的基类。Muduo 使用它来防止某些类对象的拷贝行为。
