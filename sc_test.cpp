//
// Created by stepavly on 29.01.2022.
//

#include <gtest/gtest.h>
#include <random>
#include <utility>
#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include "ms_queue.h"

static std::mt19937 rnd;
static const uint32_t MAX_SLEEP_MS = 5;
static const size_t ACTIONS_PER_THREAD = 4;
static const size_t THREADS_COUNT = 3;
static const size_t SCENARIO_INVOCATIONS = 100;

template<typename T>
struct abstract_queue {
  virtual void push(const T &value) = 0;
  virtual std::optional<T> pop() = 0;
};

template<typename T>
struct seq_queue : public abstract_queue<T> {
  void push(const T &value) override {
    q.push(value);
  }

  std::optional<T> pop() override {
    if (q.empty()) {
      return {};
    }
    auto res = q.front();
    q.pop();
    return res;
  }

 private:
  std::queue<T> q;
};

template<typename T>
struct conq_queue : public abstract_queue<T> {
  void push(const T &value) override {
    q.push(value);
  }

  std::optional<T> pop() override {
    return q.pop();
  }

 private:
  ms_queue<T> q;
};

template<typename T>
using thread_action_r = std::variant<std::monostate, std::optional<T>>;

template<typename T>
struct thread_action {
  std::function<thread_action_r<T>(abstract_queue<T> &)> action;
  thread_action_r<T> result;
  T value;

  thread_action(std::function<thread_action_r<T>(abstract_queue<T> &)> action, T val)
    : action(std::move(action))
    , result(std::monostate())
    , value(val) {}

  void set_result(abstract_queue<T> &res) {
    result = action(res);
  }

  std::string to_string() const {
    if (result.index() == 1) {
      auto res = std::get<1>(result);
      if (res) {
        return "pop(): " + std::to_string(*res);
      } else {
        return "pop(): null";
      }
    } else {
      return std::string("push(") + std::to_string(value) + std::string(")");
    }
  }
};

std::vector<std::vector<thread_action<int>>> generate_scenario() {
  std::vector<std::vector<thread_action<int>>> scenario;
  for (size_t tid = 0; tid < THREADS_COUNT; tid++) {
    std::vector<thread_action<int>> thread_scenario;
    thread_scenario.reserve(ACTIONS_PER_THREAD);
    for (size_t i = 0; i < ACTIONS_PER_THREAD; i++) {
      int val = rnd() % 10;
      switch (rnd() % 2) {
        case 0:
          thread_scenario.emplace_back([val](abstract_queue<int> &q) {
            q.push(val);
            return std::monostate();
          }, val);
          break;
        case 1:
          thread_scenario.emplace_back([](abstract_queue<int> &q) {
            return q.pop();
          }, val);
          break;
        default:break;
      }
    }
    scenario.push_back(thread_scenario);
  }
  return scenario;
}

bool validate(const std::vector<std::vector<thread_action<int>>> &scenario) {
  std::vector<size_t> thread_action(scenario.size());
  std::function<bool(seq_queue<int>)> validate = [&thread_action, &scenario, &validate](const seq_queue<int> &q) {
    bool is_final_state = true;
    for (size_t t = 0; t < scenario.size(); t++) {
      if (thread_action[t] < scenario[t].size()) {
        is_final_state = false;
        auto new_q = q;
        size_t &i = thread_action[t];
        if (scenario[t][i].action(new_q) == scenario[t][i].result) {
          i++;
          if (validate(new_q)) {
            return true;
          }
          i--;
        }
      }
    }
    return is_final_state;
  };
  return validate(seq_queue<int>());
}

std::string to_string(const std::vector<std::vector<thread_action<int>>> &scenario) {
  const size_t WIDTH = 15;
  std::string res;
  for (size_t i = 0; i < scenario[0].size(); i++) {
    std::string line(scenario.size() * WIDTH, ' ');
    for (size_t t = 0; t < scenario.size(); t++) {
      auto s = scenario[t][i].to_string();
      for (size_t j = 0; j < s.size(); j++) {
        line[WIDTH * t + j] = s[j];
      }
      line[WIDTH * t + WIDTH - 2] = '|';
    }
    res += line;
    res += '\n';
  }
  return res;
}

TEST(SEQUENTIAL_CONSISTENCY, CHECK) {
  auto scenario = generate_scenario();
  std::vector<std::thread> threads(THREADS_COUNT);
  std::vector<std::mt19937> thread_rands(THREADS_COUNT);
  for (size_t i = 0; i < THREADS_COUNT; i++) {
    thread_rands[i].seed(rnd());
  }

  for (size_t invocation = 0; invocation < SCENARIO_INVOCATIONS; invocation++) {
    conq_queue<int> conq;
    std::atomic<bool> can_start = false;
    for (size_t id = 0; id < THREADS_COUNT; id++) {
      threads[id] = std::thread([&scenario, &conq, &can_start, &thread_rands, id]() {
        while (!can_start.load()) continue;
        for (auto &action: scenario[id]) {
          action.set_result(conq);
          std::this_thread::sleep_for(std::chrono::milliseconds(thread_rands[id]() % MAX_SLEEP_MS));
        }
      });
    }
    can_start = true;
    for (auto &t: threads) {
      t.join();
    }

    ASSERT_TRUE(validate(scenario)) << "\nFailed scenario:\n" << to_string(scenario);
    if (invocation == 0 || (invocation + 1) % 10 == 0) {
      GTEST_LOG_(INFO) << "Running invocation #" << (invocation + 1) << std::endl << to_string(scenario);
    }
  }
}

extern "C" {
void __ubsan_on_report() {
  FAIL() << "Encountered an undefined behavior sanitizer error";
}
void __asan_on_error() {
  FAIL() << "Encountered an address sanitizer error";
}
void __tsan_on_report() {
  FAIL() << "Encountered a thread sanitizer error";
}
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  GTEST_FLAG_SET(repeat, 100);
  GTEST_FLAG_SET(break_on_failure, true);
  return RUN_ALL_TESTS();
}
