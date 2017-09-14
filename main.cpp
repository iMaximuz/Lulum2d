
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <queue>
#include <chrono>
#include <string>
#include <map>
#include <atomic>

std::mutex printer_mutex;
/*
std::mutex queueMutex;
std::condition_variable signal;
std::queue<std::string> workQueue;

std::map<std::thread::id, std::string> threadNames;
bool running = true;

void Printer(std::string str);


void DoWork(int workerId) {

	using namespace std::chrono_literals;

	std::this_thread::sleep_for(500ms);
	std::unique_lock<std::mutex> ul(queueMutex);
	std::string workerName = threadNames[std::this_thread::get_id()];

	while(running || workQueue.size() > 0) {
		
		if (workQueue.size() > 0) {
			
			std::string work = workQueue.front();
			workQueue.pop();
			ul.unlock();
		
			Printer(workerName + ": " + work);

			ul.lock();
		}
		else {
			//Printer(workerName + ": " + "waiting...");
			signal.wait(ul);
		}
	}

}

void PushWork(std::string work) {
	std::unique_lock<std::mutex> ul(queueMutex);
	std::string workerName = threadNames[std::this_thread::get_id()];
	if (running) {
		workQueue.push(work);
		signal.notify_one();
	}
	else {
		Printer(workerName +  ": ERROR Tried to push work while stopping work pool");
	}
}

void Stop() {
	running = false;
	signal.notify_all();
}
*/

void Printer(std::string str) {
	std::unique_lock<std::mutex> ul(printer_mutex);
	std::cout << str << std::endl;
}

void Adder(int n1, int n2) {
	int result = n1 + n2;
	Printer("Add " + std::to_string(n1) + " + " + std::to_string(n2) + " = " + std::to_string(result));
}

class Work {
public:
	//Work() {}
	virtual void operator()() = 0;
private:
	//std::function<void()> function;
};
using IWork = Work*;

/*
template<typename T, typename ... f_args>
class Work {
public:

	Work() {}
	Work(std::function<void(f_args...)> _taskFunction) : taskFunction(_taskFunction) { }

	std::function<void(f_args...)> taskFunction;

	void operator()() {
		self().executeTask();
	}

private:
	
	void executeTask() {
		taskFunction();
	}

	T& self() {
		return static_cast<T&>(*this);
	}
};*/

template<typename T, typename ... f_args>
class Task : public Work{
public:

	Task() {}
	Task(std::function<void(f_args...)> _taskFunction) : taskFunction(_taskFunction) { }

	std::function<void(f_args...)> taskFunction;

	virtual void operator()(){
		self().executeTask();
	}



private:

	void executeTask() {
		taskFunction();
	}

	T& self() {
		return static_cast<T&>(*this);
	}
};


class PrinterTask : public Task<PrinterTask, std::string>{
	friend Task<PrinterTask, std::string>;
public:

	PrinterTask(std::function<void(std::string)> _taskFunction, std::string _data) {
		taskFunction = _taskFunction;
		data = _data;
	}

	std::string data;

private:
	void executeTask() {
		taskFunction(data);
	}
};

class AddTask : public Task<AddTask, int, int> {
	friend Task<AddTask, int, int>;
public:
	AddTask(std::function<void(int, int)> _taskFunction, int _a, int _b) {
		taskFunction = _taskFunction;
		a = _a;
		b = _b;
	}
	int a;
	int b;
private:
	void executeTask() {
		taskFunction(a, b);
	}
};

template<typename _WTy>
class WorkQueue {
public:

	WorkQueue(int nWorkers = 0, bool shouldStart = true) {
		//TODO: Use max workers?
		int systemMax = std::thread::hardware_concurrency();
		if (nWorkers > systemMax || nWorkers <= 0)
			m_nWorkers = systemMax;
		else
			m_nWorkers = nWorkers;

		if (shouldStart) {
			Start();
		}
	}

	~WorkQueue() { Abort(); }

	void Start() {
		if (!m_started) {
			for (int i = 0; i < m_nWorkers; i++) {
				std::thread t(&WorkQueue::DoWork, this);
				m_workers.push_back(std::move(t));
			}
			m_started = true;
		}
	}

	void Abort() {
		if (m_started) {
			m_exit = true;
			m_finishWork = false;
			m_signal.notify_all();
			JoinAll();
			{ 
				std::lock_guard<std::mutex> lg(m_mutex);
				m_work.clear();
			}
		}
	}

	void Stop() {
		if (m_started) {
			m_exit = true;
			m_finishWork = true;
			m_signal.notify_all();
		}
	}

	void WaitToFinish() {
		if (m_started) {
			Stop();
			JoinAll();
		}
	}

	void Submit(std::shared_ptr<_WTy> task) {
		if (m_exit) {
			throw std::runtime_error("Caught task submission to work queue that is desisting.");
		}

		{
			std::lock_guard<std::mutex> lg(m_mutex);
			m_work.push_back(std::move(task));
			m_signal.notify_one();
		}
	}

private:

	int m_nWorkers;
	std::vector<std::thread> m_workers;
	std::mutex m_mutex;
	std::condition_variable m_signal;
	std::deque<std::shared_ptr<_WTy>> m_work;

	bool m_started{ false };
	std::atomic<bool> m_exit{ false };
	std::atomic<bool> m_finishWork{ true };

	void DoWork() {
		std::unique_lock<std::mutex> ul(m_mutex);

		while (!m_exit || (m_finishWork && !m_work.empty())) {
			if (m_work.size() > 0) {

				std::shared_ptr<_WTy> ptr_task = m_work.front();
				m_work.pop_front();
				ul.unlock();

				(*ptr_task)();

				ul.lock();
			}
			else {
				m_signal.wait(ul);
			}
		}
	}

	void JoinAll() {
		for (auto& w : m_workers) {
			w.join();
		}
		m_workers.clear();
	}

	void operator=(const WorkQueue&) = delete;
	WorkQueue(const WorkQueue&) = delete;
};

int main() {
	/*
	std::vector<std::thread> workers;
	int nWorkers = 2;
	threadNames.insert(std::pair<std::thread::id, std::string>(std::this_thread::get_id(), std::string("main")));

	for (int i = 0; i < 10; i++) {
		std::string str = "im doing something important: " + std::to_string(i);
		PushWork(str);
		//std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}

	Stop();

	for (int i = 0; i < nWorkers; i++) {
		std::thread t(DoWork, i + 1);
		threadNames.insert(std::pair<std::thread::id, std::string>(t.get_id(), std::string("Worker #" + std::to_string(i + 1))));
		workers.push_back(std::move(t));
	}



	PushWork("Finishing work");
	PushWork("No more work to do");
	
	PushWork("No more work to do");
	for (auto& w : workers) {
		w.join();
	}

	std::cout << "Main: All threads finished" << std::endl;
	*/

	WorkQueue<Work> tasksQueue(2, false);

	for (int i = 0; i < 10; i++) {
		std::shared_ptr<Work> task(new PrinterTask(Printer, "some data: " + std::to_string(i)));
		std::shared_ptr<Work> t2(new AddTask(Adder, i, i + 5));
		tasksQueue.Submit(task);
		tasksQueue.Submit(t2);
	}

	tasksQueue.Start();
	tasksQueue.WaitToFinish();
	Printer("Main: All threads finished");
	//std::cout << "Main: All threads finished" << std::endl;

	system("pause");
	return 0;
}