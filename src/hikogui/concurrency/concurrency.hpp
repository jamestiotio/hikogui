// Copyright Take Vos 2023.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "atomic.hpp" // export
#include "callback_flags.hpp" // export
#include "global_state.hpp" // export
#include "notifier.hpp" // export
#include "rcu.hpp" // export
#include "subsystem.hpp" // export
#include "thread.hpp" // export
#include "unfair_mutex.hpp" // export
#include "unfair_recursive_mutex.hpp" // export
#include "wfree_idle_count.hpp" // export

hi_export_module(hikogui.concurrency);

namespace hi {
inline namespace v1 {

/** @defgroup concurrency Concurrency.

Types and functions for handling multiple threads of execution.
*/

}}
