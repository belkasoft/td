//
// Copyright Aliaksei Levin (levlam@telegram.org), Arseny Smirnov (arseny30@gmail.com) 2014-2023
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "td/telegram/MessageId.h"

#include "td/utils/common.h"

#include <functional>

namespace td {

class OrderedMessage {
 public:
  MessageId get_message_id() const {
    return message_id_;
  }

  bool have_previous_ = false;
  bool have_next_ = false;

 private:
  int32 random_y_ = 0;

  MessageId message_id_;

  unique_ptr<OrderedMessage> left_;
  unique_ptr<OrderedMessage> right_;

  friend class OrderedMessages;
};

class OrderedMessages {
 public:
  class IteratorBase {
    vector<const OrderedMessage *> stack_;

   protected:
    IteratorBase() = default;

    // points iterator to message with greatest identifier which is less or equal than message_id
    IteratorBase(const OrderedMessage *root, MessageId message_id) {
      CHECK(!message_id.is_scheduled());

      size_t last_right_pos = 0;
      while (root != nullptr) {
        //        LOG(DEBUG) << "Have root->message_id_ = " << root->message_id_;
        stack_.push_back(root);
        if (root->message_id_ <= message_id) {
          //          LOG(DEBUG) << "Go right";
          last_right_pos = stack_.size();
          root = root->right_.get();
        } else {
          //          LOG(DEBUG) << "Go left";
          root = root->left_.get();
        }
      }
      stack_.resize(last_right_pos);
    }

    const OrderedMessage *operator*() const {
      return stack_.empty() ? nullptr : stack_.back();
    }

    ~IteratorBase() = default;

   public:
    IteratorBase(const IteratorBase &) = default;
    IteratorBase &operator=(const IteratorBase &) = default;
    IteratorBase(IteratorBase &&other) = default;
    IteratorBase &operator=(IteratorBase &&other) = default;

    void operator++() {
      if (stack_.empty()) {
        return;
      }

      const OrderedMessage *cur = stack_.back();
      if (!cur->have_next_) {
        stack_.clear();
        return;
      }
      if (cur->right_ == nullptr) {
        while (true) {
          stack_.pop_back();
          if (stack_.empty()) {
            return;
          }
          const OrderedMessage *new_cur = stack_.back();
          if (new_cur->left_.get() == cur) {
            return;
          }
          cur = new_cur;
        }
      }

      cur = cur->right_.get();
      while (cur != nullptr) {
        stack_.push_back(cur);
        cur = cur->left_.get();
      }
    }

    void operator--() {
      if (stack_.empty()) {
        return;
      }

      const OrderedMessage *cur = stack_.back();
      if (!cur->have_previous_) {
        stack_.clear();
        return;
      }
      if (cur->left_ == nullptr) {
        while (true) {
          stack_.pop_back();
          if (stack_.empty()) {
            return;
          }
          const OrderedMessage *new_cur = stack_.back();
          if (new_cur->right_.get() == cur) {
            return;
          }
          cur = new_cur;
        }
      }

      cur = cur->left_.get();
      while (cur != nullptr) {
        stack_.push_back(cur);
        cur = cur->right_.get();
      }
    }
  };

  class ConstIterator final : public IteratorBase {
   public:
    ConstIterator() = default;

    ConstIterator(const OrderedMessage *root, MessageId message_id) : IteratorBase(root, message_id) {
    }

    const OrderedMessage *operator*() const {
      return IteratorBase::operator*();
    }
  };

  ConstIterator get_const_iterator(MessageId message_id) const {
    return ConstIterator(messages_.get(), message_id);
  }

  void insert(MessageId message_id, bool was_auto_attached, bool have_previous, bool have_next);

  void erase(MessageId message_id, bool only_from_memory);

  struct AttachInfo {
    bool have_previous_ = false;
    bool have_next_ = false;

    AttachInfo(bool have_previous, bool have_next) : have_previous_(have_previous), have_next_(have_next) {
    }
  };
  AttachInfo auto_attach_message(MessageId message_id, MessageId last_message_id, const char *source);

  void attach_message_to_previous(MessageId message_id, const char *source);

  void attach_message_to_next(MessageId message_id, const char *source);

  vector<MessageId> find_older_messages(MessageId max_message_id) const;

  vector<MessageId> find_newer_messages(MessageId min_message_id) const;

  MessageId find_message_by_date(int32 date, const std::function<int32(MessageId)> &get_message_date) const;

  vector<MessageId> find_messages_by_date(int32 min_date, int32 max_date,
                                          const std::function<int32(MessageId)> &get_message_date) const;

  void traverse_messages(const std::function<bool(MessageId)> &need_scan_older,
                         const std::function<bool(MessageId)> &need_scan_newer) const;

 private:
  class Iterator final : public IteratorBase {
   public:
    Iterator() = default;

    Iterator(OrderedMessage *root, MessageId message_id) : IteratorBase(root, message_id) {
    }

    OrderedMessage *operator*() const {
      return const_cast<OrderedMessage *>(IteratorBase::operator*());
    }
  };

  Iterator get_iterator(MessageId message_id) {
    return Iterator(messages_.get(), message_id);
  }

  static void do_find_older_messages(const OrderedMessage *ordered_message, MessageId max_message_id,
                                     vector<MessageId> &message_ids);

  static void do_find_newer_messages(const OrderedMessage *ordered_message, MessageId min_message_id,
                                     vector<MessageId> &message_ids);

  static MessageId do_find_message_by_date(const OrderedMessage *ordered_message, int32 date,
                                           const std::function<int32(MessageId)> &get_message_date);

  static void do_find_messages_by_date(const OrderedMessage *ordered_message, int32 min_date, int32 max_date,
                                       const std::function<int32(MessageId)> &get_message_date,
                                       vector<MessageId> &message_ids);

  static void do_traverse_messages(const OrderedMessage *ordered_message,
                                   const std::function<bool(MessageId)> &need_scan_older,
                                   const std::function<bool(MessageId)> &need_scan_newer);

  unique_ptr<OrderedMessage> messages_;
};

}  // namespace td
