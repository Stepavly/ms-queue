//
// Created by stepavly on 28.01.2022.
//

#ifndef MS_QUEUE__MS_QUEUE_H_
#define MS_QUEUE__MS_QUEUE_H_

#include <atomic>
#include <memory>
#include <optional>
#include <type_traits>

template<typename T>
struct ms_queue {
 public:

  static_assert(std::atomic<T*>::is_always_lock_free, "Lock-free atomic<T*> is required for algorithm lock-freedom");

  ms_queue() {
    head = tail = new node(nullptr);
  }

  void push(const T &value) {
    T *value_holder = new T(value);
    try {
      auto new_node = new node(value_holder);
      auto cur_tail = tail.load();
      while (true) {
        auto cur_tail_next = cur_tail->next_.load();
        if (cur_tail_next != nullptr) {
          tail.compare_exchange_strong(cur_tail, cur_tail_next);
        } else if (cur_tail->cas_next(new_node)) {
          tail.compare_exchange_strong(cur_tail, new_node);
          break;
        }
      }
    } catch (...) {
      delete value_holder;
      throw;
    }
  }

  std::optional<T> pop() {
    while (true) {
      auto cur_head = head.load();
      auto cur_tail = tail.load();
      auto cur_head_next = cur_head->next_.load();
      if (cur_head == cur_tail) {
        if (cur_head_next) {
          tail.compare_exchange_strong(cur_tail, cur_head_next);
        }
        return {};
      }
      if (head.compare_exchange_strong(cur_head, cur_head_next)) {
        T* res_ptr = cur_head_next->value_.exchange(nullptr);
        T res = *res_ptr;
        delete cur_head;
        delete res_ptr;
        return res;
      }
    }
  }

  ~ms_queue() {
    while (pop()) {}
    delete head.load();
  }

 private:

  struct node {
    std::atomic<T *> value_ = nullptr;
    std::atomic<node *> next_ = nullptr;

    explicit node(T *value)
      : value_(value) {}

    bool cas_next(node *next) {
      node *null = nullptr;
      return next_.compare_exchange_strong(null, next);
    }

    ~node() {}
  };

  std::atomic<node *> head, tail;
};

#endif //MS_QUEUE__MS_QUEUE_H_
