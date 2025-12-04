// Copyright 2024 The audioplayers authors. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file.

#ifndef PACKAGES_AUDIOPLAYERS_WINDOWS_WINDOWS_EVENT_STREAM_HANDLER_H_
#define PACKAGES_AUDIOPLAYERS_WINDOWS_WINDOWS_EVENT_STREAM_HANDLER_H_

#include <flutter/encodable_value.h>
#include <flutter/event_channel.h>

#include <memory>
#include <mutex>

#include "platform_thread_handler.h"

using namespace flutter;

// Thread-safe EventStreamHandler that marshals calls to the platform thread.
template <typename T = EncodableValue>
class EventStreamHandler : public StreamHandler<T> {
 public:
  EventStreamHandler() = default;

  virtual ~EventStreamHandler() = default;

  void Success(std::unique_ptr<T> _data) {
    auto sharedData = std::make_shared<T>(std::move(*_data));

    audioplayers::PlatformThreadHandler::RunOnPlatformThread(
        [this, sharedData]() {
          std::unique_lock<std::mutex> _ul(m_mtx);
          if (m_sink.get()) {
            m_sink.get()->Success(*sharedData);
          }
        });
  }

  void Error(const std::string& error_code,
             const std::string& error_message,
             const T& error_details) {
    auto code = error_code;
    auto message = error_message;
    auto details = error_details;

    audioplayers::PlatformThreadHandler::RunOnPlatformThread(
        [this, code, message, details]() {
          std::unique_lock<std::mutex> _ul(m_mtx);
          if (m_sink.get()) {
            m_sink.get()->Error(code, message, details);
          }
        });
  }

 protected:
  std::unique_ptr<StreamHandlerError<T>> OnListenInternal(
      const T* arguments,
      std::unique_ptr<EventSink<T>>&& events) override {
    std::unique_lock<std::mutex> _ul(m_mtx);
    m_sink = std::move(events);
    return nullptr;
  }

  std::unique_ptr<StreamHandlerError<T>> OnCancelInternal(
      const T* arguments) override {
    std::unique_lock<std::mutex> _ul(m_mtx);
    m_sink.reset();
    return nullptr;
  }

 private:
  std::mutex m_mtx;
  std::unique_ptr<EventSink<T>> m_sink;
};

#endif  // PACKAGES_AUDIOPLAYERS_WINDOWS_WINDOWS_EVENT_STREAM_HANDLER_H_
