#include <iostream>
#include <queue>
#include <functional>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <chrono>

class TaskScheduler {
public:
  TaskScheduler() : stop_flag(false) {
    worker_thread = std::thread(&TaskScheduler::Worker, this);
  }

  ~TaskScheduler() {
    {
      std::unique_lock<std::mutex> lock(queue_mutex);
      stop_flag = true;
    }
    cv.notify_one();
    worker_thread.join();
  }

  void Add(std::function<void()> task, std::time_t timestamp) {
    {
      std::unique_lock<std::mutex> lock(queue_mutex);
      task_queue.emplace(timestamp, task);
    }
    cv.notify_one();
  }

private:
  struct Task {
    Task(std::time_t ts, std::function<void()> f) : timestamp(ts), func(f) {}

    bool operator>(const Task& other) const {
      return timestamp > other.timestamp;
    }

    std::time_t timestamp;
    std::function<void()> func;
  };

  void Worker() {
    while (true) {
      std::function<void()> current_task;
      std::time_t current_time = 0;

      {
        std::unique_lock<std::mutex> lock(queue_mutex);

        while (!stop_flag && task_queue.empty()) {
          cv.wait(lock);
        }

        if (stop_flag && task_queue.empty())
          break;

        auto now = std::time(nullptr);

        if (!task_queue.empty() && task_queue.top().timestamp <= now) {
          current_task = task_queue.top().func;
          task_queue.pop();
        } else if (!task_queue.empty()) {
          current_time = task_queue.top().timestamp;
          auto wait_time = std::chrono::system_clock::from_time_t(current_time);
          cv.wait_until(lock, wait_time);
          continue;
        }
      }

      if (current_task) {
        current_task();
      }
    }
  }

  std::priority_queue<Task, std::vector<Task>, std::greater<Task>> task_queue;
  std::mutex queue_mutex;
  std::condition_variable cv;
  std::thread worker_thread;
  bool stop_flag;
};


int main() {
  TaskScheduler scheduler;

  scheduler.Add([]() {
    std::cout << "Задача выполнена через 3 секунды\n";
  }, std::time(nullptr) + 3);

  scheduler.Add([]() {
    std::cout << "Задача выполнена через 5 секунд\n";
  }, std::time(nullptr) + 5);

  scheduler.Add([]() {
    std::cout << "Задача выполнена при запуске\n";
  }, std::time(nullptr));

  std::this_thread::sleep_for(std::chrono::seconds(6));

  return 0;
}

