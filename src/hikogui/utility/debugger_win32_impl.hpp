// Copyright Take Vos 2021-2022.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "../win32_headers.hpp"
#include "../macros.hpp"
#include "debugger_intf.hpp"
#include <exception>

hi_export_module(hikogui.utility.debugger : impl);

hi_warning_push();
// C6320: Exception-filter expression is the constant EXCEPTION_EXECUTE_HANDLER.
// This might mask exceptions that were not intended to be handled.
hi_warning_ignore_msvc(6320);

hi_export namespace hi { inline namespace v1 {

inline hi_no_inline bool prepare_debug_break() noexcept
{
    if (IsDebuggerPresent()) {
        // When running under the debugger, __debugbreak() after returning.
        return true;

    } else {
        __try {
            __try {
                // Attempt to break, causing an exception.
                DebugBreak();

                // The UnhandledExceptionFilter() will be called to attempt to attach a debugger.
                //  * If the jit-debugger is not configured the user gets a error dialogue-box that
                //    with "Abort", "Retry (Debug)", "Ignore". The "Retry" option will only work
                //    when the application is already being debugged.
                //  * When the jit-debugger is configured the user gets a dialogue window which allows
                //    a selection of debuggers and a "OK (Debug)", "Cancel (aborts application)".

            } __except (UnhandledExceptionFilter(GetExceptionInformation())) {
                // The jit-debugger is not configured and the user pressed any of the buttons.
                return false;
            }

        } __except (EXCEPTION_EXECUTE_HANDLER) {
            // User pressed "OK", debugger has been attached, __debugbreak() after return.
            return true;
        }

        // The jit-debugger was configured, but the use pressed Cancel.
        return false;
    }
}


}} // namespace hi::inline v1

hi_warning_pop();
