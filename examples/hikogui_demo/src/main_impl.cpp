// Copyright Take Vos 2022.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt)

#include "hikogui/module.hpp"
#include "hikogui/crt.hpp"
#include <memory>

class my_preferences : public hi::preferences {
public:
    hi::observer<std::string> audio_output_device_id;
    hi::observer<bool> audio_output_exclusive;
    hi::observer<double> audio_output_sample_rate;
    hi::observer<hi::speaker_mapping> audio_output_speaker_mapping;

    hi::observer<std::string> audio_input_device_id;
    hi::observer<bool> audio_input_exclusive;
    hi::observer<double> audio_input_sample_rate;
    hi::observer<hi::speaker_mapping> audio_input_speaker_mapping;

    hi::observer<int> tab_index = 1;
    hi::observer<bool> toggle_value;
    hi::observer<int> radio_value = 0;
    hi::observer<std::vector<std::pair<std::string, hi::label>>> _audio_device_list;

    hi::observer<std::string> selected_theme;

    my_preferences(std::filesystem::path path) : hi::preferences(std::move(path))
    {
        add("audio_output_device_id", audio_output_device_id);
        add("audio_output_exclusive", audio_output_exclusive);
        add("audio_output_sample_rate", audio_output_sample_rate);
        add("audio_input_device_id", audio_input_device_id);
        add("audio_input_exclusive", audio_input_exclusive);
        add("audio_input_sample_rate", audio_input_sample_rate);
        add("tab_index", tab_index);
        add("toggle_value", toggle_value);
        add("radio_value", radio_value);
        add("selected_theme", selected_theme);
    }
};

hi::scoped_task<> init_audio_tab(hi::grid_widget& grid, my_preferences& preferences, hi::audio_system& audio_system) noexcept
{
    using namespace hi;

    grid.make_widget<label_widget>("A1", txt("Input audio device:"), alignment::top_right());
    auto& input_config = grid.make_widget<audio_device_widget>("B1", audio_system);
    input_config.direction = audio_direction::input;
    input_config.device_id = preferences.audio_input_device_id;

    grid.make_widget<label_widget>("A2", txt("Output audio device:"), alignment::top_right());
    auto& output_config = grid.make_widget<audio_device_widget>("B2", audio_system);
    output_config.direction = audio_direction::output;
    output_config.device_id = preferences.audio_output_device_id;

    co_await std::suspend_always{};
}

hi::scoped_task<> init_theme_tab(hi::grid_widget& grid, my_preferences& preferences, hi::theme_book& theme_book) noexcept
{
    using namespace hi;

    hi::observer<std::vector<std::pair<std::string, hi::label>>> theme_list;

    {
        auto proxy = theme_list.copy();
        for (hilet& name : theme_book.theme_names()) {
            proxy->emplace_back(name, txt(name));
        }
    }

    grid.make_widget<label_widget>("A1", txt("Theme:"), alignment::top_right());
    grid.make_widget<selection_widget>("B1", preferences.selected_theme, theme_list);

    co_await std::suspend_always{};
}

hi::scoped_task<> init_license_tab(hi::grid_widget& grid, my_preferences& preferences) noexcept
{
    using namespace hi;

    grid.make_widget<label_widget>(
        "A1",
        txt("This is a \xd7\x9c\xd6\xb0\xd7\x9e\xd6\xb7\xd7\xaa\xd6\xb5\xd7\x92.\nAnd another sentence. One more:"),
        alignment::top_right());
    grid.make_widget<toggle_widget>("B1", preferences.toggle_value, txt("true"), txt("false"), txt("other"));

    grid.make_widget<label_widget>("A2", txt("These is a disabled checkbox:"), alignment::top_right());
    auto& checkbox2 = grid.make_widget<checkbox_widget>(
        "B2", preferences.radio_value, 2, txt("Checkbox, with a pretty large label."), txt("off"), txt("other"));

    grid.make_widget<label_widget>("A3", txt("These are radio buttons:"), alignment::top_right());
    grid.make_widget<radio_button_widget>("B3", preferences.radio_value, 0, txt("Radio 1"));
    grid.make_widget<radio_button_widget>("B4", preferences.radio_value, 1, txt("Radio 2 (on)"), txt("Radio 2 (off)"));
    grid.make_widget<radio_button_widget>("B5", preferences.radio_value, 2, txt("Radio 3"));

    auto option_list = std::vector{
        std::pair{0, label{txt("first")}},
        std::pair{1, label{txt("second")}},
        std::pair{2, label{txt("third")}},
        std::pair{3, label{txt("four")}},
        std::pair{4, label{txt("five")}},
        std::pair{5, label{txt("six")}},
        std::pair{6, label{txt("seven")}}};

    grid.make_widget<label_widget>("A6", txt("This is a selection box at the bottom:"), alignment::top_right());
    auto& selection3 = grid.make_widget<selection_widget>("B6", preferences.radio_value, option_list);

    grid.make_widget<label_widget>("A7", txt("Sample Rate:"), alignment::top_right());
    grid.make_widget<text_field_widget>("B7", preferences.audio_output_sample_rate);

    auto toggle_value_cbt = preferences.toggle_value.subscribe(
        [&](bool value) {
            checkbox2.mode = value ? widget_mode::enabled : widget_mode::disabled;
            selection3.mode = value ? widget_mode::enabled : widget_mode::disabled;
        },
        callback_flags::main);

    grid.make_widget<label_widget>("A8:B8", txt("This is large number locale formatted: {:L}", 1234.56));


    co_await std::suspend_always{};
}

hi::task<> preferences_window(hi::gui_system& gui, my_preferences& preferences, hi::audio_system& audio_system)
{
    using namespace hi;

    auto window_label = label{png::load(URL{"resource:hikogui_demo.png"}), txt("Preferences")};
    auto [window, widget] = gui.make_window<window_widget>(window_label);

    widget.toolbar().make_widget<toolbar_tab_button_widget>(preferences.tab_index, 0, label{elusive_icon::Speaker, txt("Audio")});
    widget.toolbar().make_widget<toolbar_tab_button_widget>(preferences.tab_index, 1, label{elusive_icon::Key, txt("License")});
    widget.toolbar().make_widget<toolbar_tab_button_widget>(preferences.tab_index, 2, label{elusive_icon::Brush, txt("Theme")});

    auto& tabs = widget.content().make_widget<tab_widget>("A1", preferences.tab_index);
    auto& audio_tab_grid = tabs.make_widget<grid_widget>(0);
    auto& license_tab_grid = tabs.make_widget<scroll_widget<axis::both>>(1).make_widget<grid_widget>();
    auto& theme_tab_grid = tabs.make_widget<grid_widget>(2);

    auto audio_tab = init_audio_tab(audio_tab_grid, preferences, audio_system);
    auto license_tab = init_license_tab(license_tab_grid, preferences);
    auto theme_tab = init_theme_tab(theme_tab_grid, preferences, *gui.theme_book);

    co_await window->closing;
}

hi::task<> main_window(hi::gui_system& gui, my_preferences& preferences, hi::audio_system& audio_system)
{
    using namespace hi;

    auto window_label = label{png::load(URL{"resource:hikogui_demo.png"}), txt("HikoGUI demo")};
    auto [window, widget] = gui.make_window<window_widget>(window_label);

    auto preferences_label = label{elusive_icon::Wrench, txt("Preferences")};
    hilet& preferences_button = widget.toolbar().make_widget<hi::toolbar_button_widget>(preferences_label);

    auto& column = widget.content().make_widget<column_widget>("A1");
    column.make_widget<toggle_widget>(preferences.toggle_value);
    hilet& hello_world_button = column.make_widget<momentary_button_widget>(txt("Hello world AV"));

    hilet& vma_dump_button = column.make_widget<momentary_button_widget>(txt("vma\ncalculate stats"));

    while (true) {
        hilet result = co_await when_any(
            preferences_button.pressed,
            vma_dump_button.pressed,
            hello_world_button.pressed,
            preferences.toggle_value,
            window->closing);

        switch (result.index()) {
        case 0:
            preferences_window(gui, preferences, audio_system);
            break;
        case 1:
            gui.gfx->log_memory_usage();
            break;
        case 2:
            hi_log_info("Hello World");
            break;
        case 3:
            hi_log_info("Toggle value {}", get<bool>(result));
            break;
        case 4:
            co_return;
        default:
            hi_no_default();
        }
    }
}

int hi_main(int argc, char *argv[])
{
    using namespace hi;

    set_application_name("HikoGUI Demo");
    set_application_vendor("HikoGUI");
    set_application_version({1, 0, 0});

    // Start the logger system, so logging is done asynchronously.
    log::start_subsystem(global_state_type::log_level_info);
    auto render_doc = RenderDoc();

    auto preferences = my_preferences(get_path(path_location::preferences_file));

    auto gui = gui_system::make_unique();
    gui->selected_theme = preferences.selected_theme;

    auto audio_system = hi::audio_system::make_unique();

    main_window(*gui, preferences, *audio_system);
    return loop::main().resume();
}

// extern "C" const char *__asan_default_options() {
//    return "help=1:log_path=asan.log";
//}
