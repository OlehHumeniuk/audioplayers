// Copyright 2024 The audioplayers authors. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file.

#ifndef PACKAGES_AUDIOPLAYERS_WINDOWS_WINDOWS_PLATFORM_THREAD_HANDLER_H_
#define PACKAGES_AUDIOPLAYERS_WINDOWS_WINDOWS_PLATFORM_THREAD_HANDLER_H_

#include <windows.h>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>

namespace audioplayers {

#define WM_AUDIOPLAYERS_CALLBACK (WM_USER + 100)

// Ensures callbacks are executed on the Flutter platform thread.
// Uses a hidden message-only window to dispatch callbacks from MTA threads.
class PlatformThreadHandler {
 public:
  using Callback = std::function<void()>;

  PlatformThreadHandler()
      : m_hwnd(nullptr), m_platformThreadId(GetCurrentThreadId()) {}

  ~PlatformThreadHandler() { DestroyMessageWindow(); }

  // Non-copyable
  PlatformThreadHandler(const PlatformThreadHandler&) = delete;
  PlatformThreadHandler& operator=(const PlatformThreadHandler&) = delete;

  static bool Initialize() {
    if (s_instance) {
      return true;
    }
    s_instance =
        std::unique_ptr<PlatformThreadHandler>(new PlatformThreadHandler());
    return s_instance->CreateMessageWindow();
  }

  static void Shutdown() {
    if (s_instance) {
      s_instance->DestroyMessageWindow();
      s_instance.reset();
    }
  }

  static bool IsInitialized() {
    return s_instance != nullptr && s_instance->m_hwnd != nullptr;
  }

  static void RunOnPlatformThread(Callback callback,
                                  bool synchronous = false) {
    if (!s_instance || !s_instance->m_hwnd) {
#ifdef _DEBUG
      OutputDebugStringA(
          "[AudioPlayers] Warning: PlatformThreadHandler not initialized\n");
#endif
      callback();
      return;
    }

    if (GetCurrentThreadId() == s_instance->m_platformThreadId) {
      callback();
      return;
    }

    s_instance->PostCallback(std::move(callback), synchronous);
  }

  static DWORD GetPlatformThreadId() {
    return s_instance ? s_instance->m_platformThreadId : 0;
  }

 private:
  bool CreateMessageWindow() {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"AudioPlayersMessageWindow";

    if (!RegisterClassExW(&wc)) {
      DWORD error = GetLastError();
      if (error != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
      }
    }

    m_hwnd = CreateWindowExW(0, L"AudioPlayersMessageWindow", L"", 0, 0, 0, 0,
                             0, HWND_MESSAGE, nullptr,
                             GetModuleHandle(nullptr), this);

    if (!m_hwnd) {
      return false;
    }

    SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    return true;
  }

  void DestroyMessageWindow() {
    if (m_hwnd) {
      DestroyWindow(m_hwnd);
      m_hwnd = nullptr;
    }
  }

  void PostCallback(Callback callback, bool synchronous) {
    auto* callbackPtr = new CallbackWrapper{std::move(callback), synchronous};

    if (synchronous) {
      SendMessageW(m_hwnd, WM_AUDIOPLAYERS_CALLBACK,
                   reinterpret_cast<WPARAM>(callbackPtr), 0);
    } else {
      if (!PostMessageW(m_hwnd, WM_AUDIOPLAYERS_CALLBACK,
                        reinterpret_cast<WPARAM>(callbackPtr), 0)) {
        delete callbackPtr;
#ifdef _DEBUG
        OutputDebugStringA("[AudioPlayers] Warning: PostMessage failed\n");
#endif
      }
    }
  }

  static LRESULT CALLBACK WindowProc(HWND hwnd,
                                     UINT msg,
                                     WPARAM wParam,
                                     LPARAM lParam) {
    if (msg == WM_AUDIOPLAYERS_CALLBACK) {
      auto* wrapper = reinterpret_cast<CallbackWrapper*>(wParam);
      if (wrapper) {
        try {
          wrapper->callback();
        } catch (...) {
#ifdef _DEBUG
          OutputDebugStringA("[AudioPlayers] Exception in callback\n");
#endif
        }
        delete wrapper;
      }
      return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
  }

  struct CallbackWrapper {
    Callback callback;
    bool synchronous;
  };

  HWND m_hwnd;
  DWORD m_platformThreadId;

  static inline std::unique_ptr<PlatformThreadHandler> s_instance;
};

}  // namespace audioplayers

#endif  // PACKAGES_AUDIOPLAYERS_WINDOWS_WINDOWS_PLATFORM_THREAD_HANDLER_H_
