#include "threadpool.h"
const int TASKMAXTHRESHHOLD = 4;
const int THREADMAXTHRESHHOLD = 8;
const int THREAD_MAX_IDLE_TIME = 15;


Result::Result(std::shared_ptr<Task> task, bool isValid)
	:task_(task), isValid_(isValid)
{
	// std::shared_ptr<Result> res=std::make_shared<Result>(task,isValid);
	task_->setResult(this);
}
Any Result::get() {
	if (!isValid_) {
		return "";
	}
	sem_.wait();
	return std::move(any_);
}
void Result::setVal(Any any) {
	this->any_ = std::move(any);
	sem_.post();
}

Task::Task() :res_(nullptr) {}
void Task::exec() {
	if (res_ != nullptr) {
		res_->setVal(run());
	}
}
void Task::setResult(Result* res) {
	res_ = res;
}

int Thread::generateId_ = 0;
Thread::Thread(ThreadFunc func) :func_(func), threadId_(generateId_++) {}
Thread::~Thread() {}
int Thread::getId()const
{
	return threadId_;
}
void Thread::start()
{
	std::thread t(func_, threadId_);
	t.detach();
}



ThreadPool::ThreadPool() :initThreadSize_(0), idleThreadSize_(0), curThreadSize_(0), threadMaxThreshhold_(THREADMAXTHRESHHOLD)
, taskSize_(0), taskQueThreshHold_(TASKMAXTHRESHHOLD), poolMode_(PoolMode::MODE_FIXED), isPoolRunning_(false)
{}

ThreadPool::~ThreadPool() {
	isPoolRunning_ = false;



	// 等待线程池里面所有线程返回  有两种状态：阻塞 & 正在执行任务
	std::unique_lock<std::mutex> lock(taskQueMtx_);

	// 没事做了线程都在wait，我们先把它跳到阻塞状态
	notEmpty_.notify_all();

	exitCond_.wait(lock, [&]()->bool {return threads_.size() == 0; });
}
void ThreadPool::setMode(PoolMode mode)
{
	poolMode_ = mode;
}
void ThreadPool::setTaskMaxThreshhold(int threshhold)
{
	taskQueThreshHold_ = threshhold;
}
void ThreadPool::setThreadMaxThreshhold(int threshhold)
{
	threadMaxThreshhold_ = threshhold;
}
void ThreadPool::start(int inialThreadSize)
{
	isPoolRunning_ = true;
	initThreadSize_ = inialThreadSize;
	for (int i = 0; i < inialThreadSize; ++i) {
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
		threads_.emplace(ptr->getId(), std::move(ptr));
	}
	curThreadSize_ = inialThreadSize;
	for (int i = 0; i < inialThreadSize; ++i) {
		threads_[i]->start();
		idleThreadSize_++;
	}
}
Result ThreadPool::submitTask(std::shared_ptr<Task> sp) {
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	/*while (taskQue.size() == taskQueThreshHold_) {
		notFull_.wait(lock);
	}*/
	std::cout << "尝试提交任务..." << std::endl;
	//notFull_.wait(lock, [&]()->bool {return taskQue.size() < taskQueThreshHold_; });
	if (!notFull_.wait_for(lock, std::chrono::seconds(1), [&]()->bool {return taskQue.size() < taskQueThreshHold_; }))
	{
		std::cerr << "task queue is full,submit task fail." << std::endl;
		return Result(sp, false);
	}
	std::cout << "提交任务成功！" << std::endl;
	taskQue.emplace(sp);
	taskSize_++;

	notEmpty_.notify_all();
	if (poolMode_ == PoolMode::MODE_CACHED
		&& curThreadSize_ < threadMaxThreshhold_ && idleThreadSize_ < taskSize_) {
		std::cout << ">>> create new thread" << std::endl;
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
		threads_[threadId]->start();
		curThreadSize_++;
		idleThreadSize_++;
	}
	return Result(sp, true);
}
// 每个线程去任务队列拿任务
void ThreadPool::threadFunc(int threadId) {
	auto lastTime = std::chrono::high_resolution_clock().now();
	while (1) {
		std::shared_ptr<Task> sp;
		{
			std::cout << "tid:" << std::this_thread::get_id() << "尝试获取任务" << std::endl;
			std::unique_lock<std::mutex> lock(taskQueMtx_);

			while (taskQue.size() == 0) {
				if (!isPoolRunning_) {
					threads_.erase(threadId);
					std::cout << "threadid:" << std::this_thread::get_id() << " exit!" << std::endl;;
					exitCond_.notify_all();
					return;
				}

				if (poolMode_ == PoolMode::MODE_CACHED) {
					if (std::cv_status::timeout ==
						notEmpty_.wait_for(lock, std::chrono::seconds(1)))
					{
						auto now = std::chrono::high_resolution_clock().now();
						auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
						if (duration.count() >= THREAD_MAX_IDLE_TIME && curThreadSize_ > initThreadSize_)
						{
							threads_.erase(threadId);
							curThreadSize_--;
							idleThreadSize_--;

							std::cout << "threadid:" << std::this_thread::get_id() << " exit!" << std::endl;;
							return;
						}
					}
				}
				else {
					notEmpty_.wait(lock);
				}
			}

			idleThreadSize_--;
			std::cout << "tid: " << std::this_thread::get_id() << "获取任务成功" << std::endl;
			sp = taskQue.front();
			taskQue.pop();
			taskSize_--;

			if (taskQue.size() > 0) {
				notEmpty_.notify_all();
			}
			notFull_.notify_all();
		}
		sp->exec();
		std::cout << "end thread_func tid:" << std::this_thread::get_id() << std::endl;
		idleThreadSize_++;
		lastTime = std::chrono::high_resolution_clock().now();
	}

}