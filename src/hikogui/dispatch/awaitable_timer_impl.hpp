// Copyright Take Vos 2022.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "awaitable_timer_intf.hpp"
#include "loop.hpp"
#include "../macros.hpp"

hi_export_module(hikogui.dispatch.awaitable_timer : impl);

namespace hi::inline v1 {

inline void awaitable_timer::await_suspend(std::coroutine_handle<> handle) noexcept
{
    _token = loop::local().delay_function(_deadline, [handle = std::move(handle)]() {
        handle.resume();
    });
}

}
