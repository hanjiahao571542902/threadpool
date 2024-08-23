#include<iostream>
#include<chrono>
#include "threadpool.h"
using namespace std;
typedef long long ll;
class MyTask :public Task
{
public:
	MyTask(int begin, int end) :begin_(begin), end_(end) {};

	Any run()
	{
		std::cout << std::this_thread::get_id() << "run" << std::endl;
		std::this_thread::sleep_for(chrono::seconds(3));
		ll sum = 0;
		for (int i = begin_; i <= end_; ++i) {
			sum += i;
		}
		cout << "sum=" << std::endl;
		cout << sum << endl;
		return sum;
	}
private:
	int begin_;
	int end_;
};
int main() {
	{
		ThreadPool pool;
		pool.setMode(PoolMode::MODE_FIXED);
		pool.start(2);
		// 任务结束后，返回一个result
		Result res = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
		Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
		Result res3 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		Result res4 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		Result res5 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		Result res6 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		Result res7 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		Result res8 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		//std::this_thread::sleep_for(chrono::seconds(10));
		ll sum = res.get().Cast<ll>() + res2.get().Cast<ll>();
		cout << "fires=" << sum << endl;
	}
	cout << "final";
}