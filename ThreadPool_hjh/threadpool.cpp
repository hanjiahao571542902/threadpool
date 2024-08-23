#include "threadpool.h"


const int TASK_MAX_THRESHHOLD = INT32_MAX;
const int THREAD_MAX_THRESHHOLD = 6;
const int THREAD_MAX_IDLE_TIME = 10;// 秒


ThreadPool::ThreadPool() // 有构造就有析构
	:initThreadSize_(0)
	, taskSize_(0)
	, idleThreadSize_(0)
	, curThreadSize_(0)
	, taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
	, threadSizeThreshHold_(THREAD_MAX_THRESHHOLD)// 不同用户能力不同
	, poolMode_(PoolMode::MODE_FIXED)
	, isPoolRunning_(false)
{}
ThreadPool::~ThreadPool()
{
	isPoolRunning_ = false;



	// 等待线程池里面所有线程返回  有两种状态：阻塞 & 正在执行任务
	std::unique_lock<std::mutex> lock(taskQueMtx_);

	// 没事做了线程都在wait，我们先把它跳到阻塞状态
	notEmpty_.notify_all();
	std::cout << "要析构了" << std::endl;
	exitCond_.wait(lock, [&]()->bool {return threads_.size() == 0; });
	std::cout << "析构完成" << std::endl;
}

// 设置线程池工作模式
void ThreadPool::setMode(PoolMode mode) {
	if (checkRunningState()) {
		return;
	}
	poolMode_ = mode;
}

// 设置task任务队列上限阈值
void ThreadPool::setTaskQueMaxThreshHold(int threshhold) {
	if (checkRunningState()) {
		return;
	}
	taskQueMaxThreshHold_ = threshhold;
}

// 设置cached模式下线程阈值
void ThreadPool::setThreadMaxThreshHold(int threshhold) {
	if (checkRunningState()) {
		return;
	}
	if (poolMode_ == PoolMode::MODE_CACHED) {
		threadSizeThreshHold_ = threshhold;
	}
}

// 给线程池提交任务 用户调用该接口，传入任务对象，生产任务
Result ThreadPool::submitTask(std::shared_ptr<Task> sp) {
	// 获取锁
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	// 看队列状态    wait  wait_for   wait_until
	// wait死等，后两个可以加时间参数，wait_for等多长  waituntil等到。。。
	// 用户提交任务，最长不能阻塞超过1s，否则判断提交任务失败，返回
	// 如果一秒到了等到了，怎么区分呢，返回值是个bool
	if (!notFull_.wait_for(lock, std::chrono::seconds(1), [&]()->bool {return taskQue.size() < (size_t)taskQueMaxThreshHold_; })) {
		// 等了1s还是不行
		std::cerr << "task queue is full,submit task fail." << std::endl;
		return Result(sp, false);
		// return Result(task) 还是task->getResult()???
		// 线程执行完task，task就被析构了，task生命周期延长到拿到res
	}

	// 有空余就提交任务
	taskQue.emplace(sp);
	taskSize_++;

	// 队列有任务了，来用
	notEmpty_.notify_all();

	// cached模式：任务处理比较紧急  场景：小儿快的任务 需要根据空闲线程数量和任务数量，判断是否需要创建新线程
	// 耗时的任务不适合cached，因为会创建很多线程
	if (poolMode_ == PoolMode::MODE_CACHED
		&& idleThreadSize_ < taskSize_
		&& curThreadSize_ < threadSizeThreshHold_) {

		std::cout << ">>> create new thread" << std::endl;

		// 创建新线程
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
		threads_[threadId]->start();

		//修改相关线程数量
		curThreadSize_++;
		idleThreadSize_++;
	}


	return Result(sp, true);
}

// 开启线程池
void ThreadPool::start(int initThreadSize) {
	isPoolRunning_ = true;
	initThreadSize_ = initThreadSize;
	curThreadSize_ = initThreadSize;

	// 创建线程对象
	// 先创建会先启动，所以为了公平,启动再下面
	for (int i = 0; i < initThreadSize_; ++i) {
		// 创建线程对象是，把线程函数给到线程对象
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
	}

	// 启动所有线程 std::vector<Thread*> threads_;
	for (int i = 0; i < initThreadSize_; i++) {
		threads_[i]->start();
		idleThreadSize_++; //记录初始空闲线程
	}
}

// 定义线程函数 线程池的所有线程从任务队列消费任务
void ThreadPool::threadFunc(int threadid) { // 线程函数结束，相应的线程也结束
	//std::cout << "begin threadFunc tid: " << std::this_thread::get_id()<< std::endl;
	//std::cout << "end threadFunc  tid: " << std::this_thread::get_id() << std::endl;

	auto lastTime = std::chrono::high_resolution_clock().now();

	//	所有任务必须执行完成，线程池才可以回收所有线程资源
	// 不能拿着锁去执行任务，不然效率太低了
	while (1) {
		std::shared_ptr<Task> task;
		{
			// 获取锁
			std::unique_lock<std::mutex> lock(taskQueMtx_);

			std::cout << "tid: " << std::this_thread::get_id() << "尝试获取任务。。" << std::endl;

			// cached模式下，有可能已经创建很多线程，但是空闲时间超过60s，应该结束回收
			// 超过initThreadSize_的线程要进行回收
			// 当前时间-上一次线程执行的时间 > 60s
				// 每一秒返回一次 怎么区分超时返回还是有任务待执行返回
			// 锁+双重判断
			while (taskQue.size() == 0) {
				// 线程池要结束，回收线程资源
				if (!isPoolRunning_)
				{
					// 做完任务的线程回头，发现while（false），出来到这释放资源
					threads_.erase(threadid);

					std::cout << "准备接任务的线程看到线程池关了所以threadid:" << std::this_thread::get_id() << " exit!" << std::endl;;
					exitCond_.notify_all();
					return;
				}
				if (poolMode_ == PoolMode::MODE_CACHED)
				{
					if (std::cv_status::timeout ==
						notEmpty_.wait_for(lock, std::chrono::seconds(1)))
					{
						// 条件变量超时返回
						auto now = std::chrono::high_resolution_clock().now();
						auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
						if (dur.count() >= THREAD_MAX_IDLE_TIME && curThreadSize_ > initThreadSize_) {
							// 开始回收当前线程
							// 记录线程数量的相关变量的值的修改
							// 把线程对象从队列列表容器中修改 没办法 threadfunc匹配哪个thread对象
							// threadid
							threads_.erase(threadid); // 注意不要传入std::this_thread_get_id()
							curThreadSize_--;
							idleThreadSize_--;

							std::cout << "cached多余threadid:" << std::this_thread::get_id() << " exit!" << std::endl;;
							return;
						}
					}
				}

				else
				{
					notEmpty_.wait(lock);
				}

				// 线程池要回收资源
				//if (!isPoolRunning_) {
				//	threads_.erase(threadid); // 注意不要传入std::this_thread_get_id()
				//	//curThreadSize_--;
				//	//idleThreadSize_--;  这会还有必要维护他们吗，把线程池资源拿下就可以

				//	std::cout << "threadid:" << std::this_thread::get_id() << " exit!" << std::endl;;
				//	exitCond_.notify_all();
				//	return;
				//}
			}

			idleThreadSize_--;

			std::cout << "tid: " << std::this_thread::get_id() << "获取任务成功" << std::endl;

			task = taskQue.front();
			taskQue.pop();
			taskSize_--;

			// 如果依然有剩余任务，继续通知其他的线程执行任务
			if (taskQue.size() > 0) {
				notEmpty_.notify_all();
			}

			// 执行完一个任务，得进行通知
			notFull_.notify_all();
		}// 把锁释放掉!!!!!!!!!!!!!!!再运行

		// 保守处理，程序不能挂
		if (task != nullptr) {
			task->exec();
		}

		idleThreadSize_++;
		// 更新线程执行完任务的时间
		lastTime = std::chrono::high_resolution_clock().now();
	}


}

bool ThreadPool::checkRunningState() const {
	return isPoolRunning_;
}

//////////// 线程方法实现

int Thread::generateId_ = 0;

// 线程构造
Thread::Thread(ThreadFunc func) : func_(func), threadId_(generateId_++) {}

// 线程析构
Thread::~Thread() {}
// 启动线程
void Thread::start() {
	// 创建一个线程来执行一个线程函数
	std::thread t(func_, threadId_);// C++11来说，线程对象t和线程函数func_
	t.detach(); // 设置分离线程 pthread.detach
}

int Thread::getId() const {
	return threadId_;
}

/////////////// Task方法实现
Task::Task()
	:result_(nullptr)
{}
void Task::exec() {
	//std::cout << "exec" << std::endl;
	if (result_ != nullptr) {
		result_->setVal(run());
	}
}

void Task::setResult(Result* res) {
	result_ = res;
}

////////////// Result方法的实现
Result::Result(std::shared_ptr<Task> task, bool isValid)
	:isValid_(isValid)
	, task_(task)
{
	task_->setResult(this);
}

// 问题一：setVal方法，获取任务执行完的返回值的
void Result::setVal(Any any) {
	this->any_ = std::move(any);
	sem_.post();
}
// 问题二：get方法，用户调用这个放大获取task的返回值
Any Result::get() {
	if (!isValid_) {
		return "";
	}
	sem_.wait();
	return std::move(any_);
}

