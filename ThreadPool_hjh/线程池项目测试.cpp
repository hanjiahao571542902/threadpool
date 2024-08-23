#include<iostream>
#include<chrono>
#include<thread>
#include"threadpool.h"

/*
有些场景需要获取线程执行返回值
1+ 。。。 +30000
thread 1 1+...10000
thread 2 10001+...+..

mainthread 再把返回值求和
*/

using uLong = unsigned long long;

class MyTask :public Task
{
public:
	MyTask(int begin, int end)
		:begin_(begin)
		, end_(end) {}
	// 问题一：怎么设计run的返回值，表示任意类型
	// Java python  Object时所有其他类类型的基类
	// C++17 Any类型
	// 如何设计任务的result机制，
	Any run() {//线程代码
		std::cout << "tid: " << std::this_thread::get_id() << "begin!" << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(3));
		uLong sum = 0;
		for (uLong i = begin_; i <= end_; ++i) {
			sum += i;
		}
		std::cout << "tid: " << std::this_thread::get_id() << "end!" << std::endl;
		//std::cout << "hello" << sum << std::endl;
		return sum;
	}

private:
	int begin_;
	int end_;
};
int main() {
	{
		ThreadPool pool;
		pool.setMode(PoolMode::MODE_CACHED);
		pool.start(2);
		Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
		Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
		Result res3 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
		Result res4 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
		Result res5 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
		uLong sum1 = res1.get().cast_<uLong>();
		std::cout << sum1 << std::endl;
	}
	std::cout << "main over!" << std::endl;
#if 0
	// 问题：threadpool对象析构以后，怎么把线程池相关的线程资源回收，因为都在等锁！！
	{
		ThreadPool pool;

		// 要保证start前执行setMode
		pool.setMode(PoolMode::MODE_CACHED);
		pool.start(4);

		// 如何设计任务的result机制，同步
		Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
		Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
		Result res3 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));

		pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));

		// 随着task被执行完，task对象没了，依赖于task对象的Result对象也没了
		// // 返回一个Any，怎么转换为具体类型
		uLong sum1 = res1.get().cast_<uLong>();
		uLong sum2 = res2.get().cast_<uLong>();
		uLong sum3 = res3.get().cast_<uLong>();

		/*std::cout << sum1 << std::endl;
		std::cout << sum1 << std::endl;
		std::cout << sum1 << std::endl;*/

		// Master - Slave 线程模型
		// Master线程用来分解任务，然后给各个Slave线程分配任务
		// 等待多个Slave线程执行完任务，返回结果
		// Master线程合并各个任务结果，输出
		uLong sum = 0;
		for (int i = 0; i <= 300000000; ++i) {
			sum += i;
		}
		std::cout << sum << std::endl;
		std::cout << (sum1 + sum2 + sum3) << std::endl;
		/*pool.submitTask(std::make_shared<MyTask>());
		pool.submitTask(std::make_shared<MyTask>());
		pool.submitTask(std::make_shared<MyTask>());
		pool.submitTask(std::make_shared<MyTask>());
		pool.submitTask(std::make_shared<MyTask>());
		pool.submitTask(std::make_shared<MyTask>());
		pool.submitTask(std::make_shared<MyTask>());
		pool.submitTask(std::make_shared<MyTask>());
		pool.submitTask(std::make_shared<MyTask>());
		pool.submitTask(std::make_shared<MyTask>());*/
	}
	// 这里出了右括号，主线程被阻塞在析构里了
	// exitCond_.wait(lock, [&]()->bool{return threads_.size()==0; });没人唤醒

	getchar();
#endif

	//std::this_thread::sleep_for(std::chrono::seconds(5));
}