#include <cmath>
#include <format>
#include <string>
#include <thread>
#include <utility>

#include <hyprutils/utils/ScopeGuard.hpp>

#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"
#include "tests.hpp"

/// Parses an "X,Y" attribute (as returned by getAttribute for "at" and "size")
/// into a pair of ints. Returns {0, 0} on a malformed value.
static std::pair<int, int> parseXY(const std::string& value) {
    const auto COMMA = value.find(',');
    if (COMMA == std::string::npos)
        return {0, 0};

    try {
        return {std::stoi(value.substr(0, COMMA)), std::stoi(value.substr(COMMA + 1))};
    } catch (...) { return {0, 0}; }
}

/// Window geometry is subject to rounding, gaps and borders, so positions are
/// compared with a small tolerance.
static bool nearly(int a, int b, int tolerance = 4) {
    return std::abs(a - b) <= tolerance;
}

static std::pair<int, int> activeWindowAt() {
    return parseXY(Tests::getAttribute(getFromSocket("/activewindow"), "at"));
}

static std::pair<int, int> activeWindowSize() {
    return parseXY(Tests::getAttribute(getFromSocket("/activewindow"), "size"));
}

/// Tests that dragging a tiled window with binds:drag_center_window enabled
/// centers the window on the cursor once it becomes floating.
TEST_CASE(dragTiledCentered) {
    OK(getFromSocket("/eval hl.config({ binds = { drag_center_window = true } })"));
    OK(getFromSocket("r/eval hl.unbind('SUPER + mouse:272')"));
    OK(getFromSocket("r/eval hl.bind('mouse:272', hl.dsp.window.drag(), { mouse = true })"));

    SPAWN_KITTY("kitty");
    ASSERT(Tests::windowCount(), 1);
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty' })"));

    // grab well away from the center, so centering is observable
    const auto [AT_X, AT_Y]     = activeWindowAt();
    const auto [SIZE_X, SIZE_Y] = activeWindowSize();
    const int GRAB_X            = AT_X + (SIZE_X * 3) / 4;
    const int GRAB_Y            = AT_Y + (SIZE_Y * 3) / 4;

    OK(getFromSocket(std::format("/dispatch hl.dsp.cursor.move({{ x = {}, y = {} }})", GRAB_X, GRAB_Y)));
    OK(getFromSocket("/eval hl.plugin.test.click(272, 1)"));
    OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = 700, y = 500 })"));

    {
        // the grab point is ignored, the floating window is centered on the cursor
        const auto [NEW_X, NEW_Y]           = activeWindowAt();
        const auto [NEW_SIZE_X, NEW_SIZE_Y] = activeWindowSize();

        EXPECT(nearly(NEW_X + NEW_SIZE_X / 2, 700), true);
        EXPECT(nearly(NEW_Y + NEW_SIZE_Y / 2, 500), true);
    }

    OK(getFromSocket("/eval hl.plugin.test.click(272, 0)"));
    OK(getFromSocket("/reload"));
    Tests::killAllWindows();
}

/// Tests that dragging a tiled window with binds:drag_center_window disabled maps
/// the grab point proportionally onto the smaller floating window, instead of
/// centering it.
TEST_CASE(dragTiledProportional) {
    OK(getFromSocket("/eval hl.config({ binds = { drag_center_window = false } })"));
    OK(getFromSocket("r/eval hl.unbind('SUPER + mouse:272')"));
    OK(getFromSocket("r/eval hl.bind('mouse:272', hl.dsp.window.drag(), { mouse = true })"));

    SPAWN_KITTY("kitty");
    ASSERT(Tests::windowCount(), 1);
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty' })"));

    // grab three quarters across and down the tiled window
    const auto [AT_X, AT_Y]     = activeWindowAt();
    const auto [SIZE_X, SIZE_Y] = activeWindowSize();
    const int GRAB_X            = AT_X + (SIZE_X * 3) / 4;
    const int GRAB_Y            = AT_Y + (SIZE_Y * 3) / 4;

    OK(getFromSocket(std::format("/dispatch hl.dsp.cursor.move({{ x = {}, y = {} }})", GRAB_X, GRAB_Y)));
    OK(getFromSocket("/eval hl.plugin.test.click(272, 1)"));
    OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = 700, y = 500 })"));

    {
        // the window shrinks as it becomes floating, but the cursor should still
        // sit three quarters across and down
        const auto [NEW_X, NEW_Y]           = activeWindowAt();
        const auto [NEW_SIZE_X, NEW_SIZE_Y] = activeWindowSize();

        EXPECT(nearly(NEW_X + (NEW_SIZE_X * 3) / 4, 700), true);
        EXPECT(nearly(NEW_Y + (NEW_SIZE_Y * 3) / 4, 500), true);
    }

    OK(getFromSocket("/eval hl.plugin.test.click(272, 0)"));
    OK(getFromSocket("/reload"));
    Tests::killAllWindows();
}

/// Tests that dragging a fullscreen window with binds:drag_center_window enabled
/// centers it on the cursor once it is restored to its floating size.
TEST_CASE(dragFullscreenCentered) {
    OK(getFromSocket("/eval hl.config({ binds = { drag_center_window = true } })"));
    OK(getFromSocket("r/eval hl.unbind('SUPER + mouse:272')"));
    OK(getFromSocket("r/eval hl.bind('mouse:272', hl.dsp.window.drag(), { mouse = true })"));

    SPAWN_KITTY("kitty");
    ASSERT(Tests::windowCount(), 1);
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.float({ action = 'set' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.resize({ x = 600, y = 400, window = 'class:kitty' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.move({ x = 100, y = 100, window = 'class:kitty' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', action = 'set' })"));

    // grab well away from the center, so centering is observable
    const auto [AT_X, AT_Y]     = activeWindowAt();
    const auto [SIZE_X, SIZE_Y] = activeWindowSize();
    const int GRAB_X            = AT_X + (SIZE_X * 3) / 4;
    const int GRAB_Y            = AT_Y + (SIZE_Y * 3) / 4;

    OK(getFromSocket(std::format("/dispatch hl.dsp.cursor.move({{ x = {}, y = {} }})", GRAB_X, GRAB_Y)));
    OK(getFromSocket("/eval hl.plugin.test.click(272, 1)"));
    OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = 700, y = 500 })"));

    {
        // the grab point is ignored, the restored window is centered on the cursor
        const auto [NEW_X, NEW_Y]           = activeWindowAt();
        const auto [NEW_SIZE_X, NEW_SIZE_Y] = activeWindowSize();

        EXPECT_CONTAINS(getFromSocket("/activewindow"), "fullscreen: 0");
        EXPECT(nearly(NEW_X + NEW_SIZE_X / 2, 700), true);
        EXPECT(nearly(NEW_Y + NEW_SIZE_Y / 2, 500), true);
    }

    OK(getFromSocket("/eval hl.plugin.test.click(272, 0)"));
    OK(getFromSocket("/reload"));
    Tests::killAllWindows();
}

/// Tests that dragging a fullscreen window with binds:drag_center_window disabled
/// maps the grab point proportionally onto the restored floating window.
TEST_CASE(dragFullscreenProportional) {
    OK(getFromSocket("/eval hl.config({ binds = { drag_center_window = false } })"));
    OK(getFromSocket("r/eval hl.unbind('SUPER + mouse:272')"));
    OK(getFromSocket("r/eval hl.bind('mouse:272', hl.dsp.window.drag(), { mouse = true })"));

    SPAWN_KITTY("kitty");
    ASSERT(Tests::windowCount(), 1);
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.float({ action = 'set' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.resize({ x = 600, y = 400, window = 'class:kitty' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.move({ x = 100, y = 100, window = 'class:kitty' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', action = 'set' })"));

    // grab three quarters across and down the fullscreen surface, far away from
    // where the restored floating window would otherwise end up
    const auto [AT_X, AT_Y]     = activeWindowAt();
    const auto [SIZE_X, SIZE_Y] = activeWindowSize();
    const int GRAB_X            = AT_X + (SIZE_X * 3) / 4;
    const int GRAB_Y            = AT_Y + (SIZE_Y * 3) / 4;

    OK(getFromSocket(std::format("/dispatch hl.dsp.cursor.move({{ x = {}, y = {} }})", GRAB_X, GRAB_Y)));
    OK(getFromSocket("/eval hl.plugin.test.click(272, 1)"));
    OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = 700, y = 500 })"));

    {
        // the window shrinks back to its floating size, but the cursor should still
        // sit three quarters across and down
        const auto [NEW_X, NEW_Y]           = activeWindowAt();
        const auto [NEW_SIZE_X, NEW_SIZE_Y] = activeWindowSize();

        EXPECT_CONTAINS(getFromSocket("/activewindow"), "fullscreen: 0");
        EXPECT(nearly(NEW_X + (NEW_SIZE_X * 3) / 4, 700, 8), true);
        EXPECT(nearly(NEW_Y + (NEW_SIZE_Y * 3) / 4, 500, 8), true);
    }

    OK(getFromSocket("/eval hl.plugin.test.click(272, 0)"));
    OK(getFromSocket("/reload"));
    Tests::killAllWindows();
}

TEST_CASE(dragLifecycleSignals) {
    OK(getFromSocket("/eval hl.config({ binds = { drag_threshold = 0 } })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '5301' })"));
    SPAWN_KITTY("drag_lifecycle");
    OK(getFromSocket("/dispatch hl.dsp.window.float({ action = 'set', window = 'class:drag_lifecycle' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.resize({ x = 300, y = 200, window = 'class:drag_lifecycle' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.move({ x = 200, y = 200, window = 'class:drag_lifecycle' })"));

    OK(getFromSocket("/eval hl.plugin.test.test_drag_lifecycle('drag_lifecycle')"));

    Tests::killAllWindows();
    OK(getFromSocket("/reload"));
}

TEST_CASE(dragThresholdSurvivesScrollEvents) {
    OK(getFromSocket("/eval hl.config({ general = { snap = { enabled = false } } })"));
    OK(getFromSocket("/eval hl.config({ binds = { scroll_event_delay = 0 } })"));
    OK(getFromSocket("/eval hl.unbind('SUPER + mouse:272'); hl.bind('mouse:272', hl.dsp.window.drag(), { mouse = true })"));
    SPAWN_KITTY("drag_threshold");
    ASSERT(Tests::windowCount(), 1);
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:drag_threshold' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.float({ action = 'set' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.resize({ x = 300, y = 200, window = 'class:drag_threshold' })"));

    for (const int THRESHOLD : {0, 40}) {
        OK(getFromSocket(std::format("/eval hl.config({{ binds = {{ drag_threshold = {} }} }})", THRESHOLD)));
        OK(getFromSocket("/dispatch hl.dsp.window.move({ x = 200, y = 200, window = 'class:drag_threshold' })"));
        // Let the client configure and animated geometry settle before the hit-tested grab.
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = 350, y = 300 })"));
        bool                          buttonHeld = true;
        Hyprutils::Utils::CScopeGuard releaseButton([&] {
            if (buttonHeld)
                getFromSocket("/eval hl.plugin.test.click(272, 0)");
        });
        OK(getFromSocket("/eval hl.plugin.test.click(272, 1)"));
        EXPECT_OK(getFromSocket(std::format("/eval hl.plugin.test.expect_drag_state(true, {})", THRESHOLD == 0)));

        if (THRESHOLD > 0) {
            // Motion below the threshold must not pick up the window.
            OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = 360, y = 300 })"));
            EXPECT_OK(getFromSocket("/eval hl.plugin.test.expect_drag_state(true, false)"));
            EXPECT(nearly(activeWindowAt().first, 200), true);
        }

        OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = 410, y = 300 })"));
        EXPECT_OK(getFromSocket("/eval hl.plugin.test.expect_drag_state(true, true)"));
        const auto [BEFORE_X, BEFORE_Y] = activeWindowAt();

        // Scroll events run processEvent without ending the mouse drag.
        OK(getFromSocket("/eval hl.plugin.test.scroll(120)"));
        EXPECT_OK(getFromSocket("/eval hl.plugin.test.expect_drag_state(true, true)"));
        OK(getFromSocket("/eval hl.plugin.test.scroll(-120)"));
        EXPECT_OK(getFromSocket("/eval hl.plugin.test.expect_drag_state(true, true)"));

        // A small move must follow immediately, without requiring the threshold again.
        OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = 420, y = 300 })"));
        const auto [AFTER_X, AFTER_Y] = activeWindowAt();
        EXPECT(nearly(AFTER_X, BEFORE_X + 10), true);
        EXPECT(nearly(AFTER_Y, BEFORE_Y), true);

        OK(getFromSocket("/eval hl.plugin.test.click(272, 0)"));
        buttonHeld = false;
        EXPECT_OK(getFromSocket("/eval hl.plugin.test.expect_drag_state(false, false)"));

        // A new drag starts with its own threshold, not the previous drag's latch.
        buttonHeld = true;
        OK(getFromSocket("/eval hl.plugin.test.click(272, 1)"));
        EXPECT_OK(getFromSocket(std::format("/eval hl.plugin.test.expect_drag_state(true, {})", THRESHOLD == 0)));
        OK(getFromSocket("/eval hl.plugin.test.click(272, 0)"));
        buttonHeld = false;
        EXPECT_OK(getFromSocket("/eval hl.plugin.test.expect_drag_state(false, false)"));
    }

    Tests::killAllWindows();
    OK(getFromSocket("/reload"));
}

/// Tests that misc:float_force_onscreen applies while a floating window is being
/// dragged, not only once the drag is released.
TEST_CASE(dragFloatingForceOnscreen) {
    OK(getFromSocket("/eval hl.config({ general = { snap = { enabled = false } } })"));
    OK(getFromSocket("/eval hl.config({ binds = { drag_threshold = 0 } })"));
    OK(getFromSocket("r/eval hl.unbind('SUPER + mouse:272')"));
    OK(getFromSocket("r/eval hl.bind('mouse:272', hl.dsp.window.drag(), { mouse = true })"));

    SPAWN_KITTY("drag_onscreen");
    ASSERT(Tests::windowCount(), 1);
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:drag_onscreen' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.float({ action = 'set' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.resize({ x = 300, y = 200, relative = false, window = 'class:drag_onscreen' })"));

    bool                          buttonHeld = false;
    Hyprutils::Utils::CScopeGuard releaseButton([&] {
        if (buttonHeld)
            getFromSocket("/eval hl.plugin.test.click(272, 0)");
    });

    const auto                    grabAt = [&](int x, int y) {
        OK(getFromSocket("/dispatch hl.dsp.window.move({ x = 200, y = 200, relative = false, window = 'class:drag_onscreen' })"));
        // Let the client configure and animated geometry settle before the hit-tested grab.
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        OK(getFromSocket(std::format("/dispatch hl.dsp.cursor.move({{ x = {}, y = {} }})", x, y)));
        OK(getFromSocket("/eval hl.plugin.test.click(272, 1)"));
        buttonHeld = true;
    };

    const auto release = [&] {
        OK(getFromSocket("/eval hl.plugin.test.click(272, 0)"));
        buttonHeld = false;
    };

    // No constraints: the window follows the pointer past the edge
    OK(getFromSocket("/eval hl.config({ misc = { float_force_onscreen = 0 } })"));
    grabAt(350, 300);
    OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = 50, y = 300 })"));
    EXPECT(nearly(activeWindowAt().first, -100), true);
    release();

    // Fully onscreen: the window stops at the edge while the button is still held
    OK(getFromSocket("/eval hl.config({ misc = { float_force_onscreen = 2 } })"));
    grabAt(350, 300);
    OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = 50, y = 300 })"));
    EXPECT(nearly(activeWindowAt().first, 2), true);
    OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = 350, y = 50 })"));
    EXPECT(nearly(activeWindowAt().second, 2), true);

    // and picks up from the same grab point when the pointer comes back
    OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = 350, y = 300 })"));
    {
        const auto [X, Y] = activeWindowAt();
        EXPECT(nearly(X, 200), true);
        EXPECT(nearly(Y, 200), true);
    }

    // the right edge stops it too: 1920 wide monitor, 300 wide window, 2px border
    OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = 1900, y = 300 })"));
    EXPECT(nearly(activeWindowAt().first, 1618), true);
    release();

    Tests::killAllWindows();
    OK(getFromSocket("/reload"));
}

/// Tests that misc:float_force_onscreen limits a drag to the monitor under the
/// pointer, so a floating window can still be dragged onto another monitor.
TEST_CASE(dragFloatingForceOnscreenAcrossMonitors) {
    OK(getFromSocket("/eval hl.config({ general = { snap = { enabled = false } } })"));
    OK(getFromSocket("/eval hl.config({ binds = { drag_threshold = 0 } })"));
    OK(getFromSocket("/eval hl.config({ misc = { float_force_onscreen = 2 } })"));
    OK(getFromSocket("r/eval hl.unbind('SUPER + mouse:272')"));
    OK(getFromSocket("r/eval hl.bind('mouse:272', hl.dsp.window.drag(), { mouse = true })"));
    OK(getFromSocket("/eval hl.monitor({ output = 'HYPRTEST-DRAG', mode = '1920x1080@60', position = 'auto-right', scale = '1' })"));
    OK(getFromSocket("/output create headless HYPRTEST-DRAG"));

    bool                          buttonHeld = false;
    Hyprutils::Utils::CScopeGuard cleanup([&] {
        if (buttonHeld)
            getFromSocket("/eval hl.plugin.test.click(272, 0)");
        getFromSocket("/output remove HYPRTEST-DRAG");
    });

    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-2' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '5302' })"));
    SPAWN_KITTY("drag_across");
    ASSERT(Tests::windowCount(), 1);
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:drag_across' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.float({ action = 'set' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.resize({ x = 300, y = 200, relative = false, window = 'class:drag_across' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.move({ x = 1500, y = 200, relative = false, window = 'class:drag_across' })"));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = 1650, y = 300 })"));
    OK(getFromSocket("/eval hl.plugin.test.click(272, 1)"));
    buttonHeld = true;

    // still on the first monitor: the window stops at its right edge
    OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = 1900, y = 300 })"));
    EXPECT(nearly(activeWindowAt().first, 1618), true);

    // once the pointer is on the second monitor, the window moves fully onto it
    OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = 2100, y = 300 })"));
    EXPECT(nearly(activeWindowAt().first, 1950), true);

    OK(getFromSocket("/eval hl.plugin.test.click(272, 0)"));
    buttonHeld = false;

    Tests::killAllWindows();
    OK(getFromSocket("/reload"));
}

/// Tests that a floating resize with misc:float_force_onscreen = 2 stops the
/// dragged edge at the monitor edge instead of moving the window.
TEST_CASE(resizeFloatingForceOnscreen) {
    OK(getFromSocket("/eval hl.config({ general = { snap = { enabled = false } } })"));
    OK(getFromSocket("/eval hl.config({ binds = { drag_threshold = 0 } })"));
    OK(getFromSocket("/eval hl.config({ misc = { float_force_onscreen = 2 } })"));
    OK(getFromSocket("r/eval hl.unbind('SUPER + mouse:273')"));
    OK(getFromSocket("r/eval hl.bind('mouse:273', hl.dsp.window.resize(), { mouse = true })"));

    SPAWN_KITTY("resize_onscreen");
    ASSERT(Tests::windowCount(), 1);
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:resize_onscreen' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.float({ action = 'set' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.resize({ x = 300, y = 200, relative = false, window = 'class:resize_onscreen' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.move({ x = 200, y = 200, relative = false, window = 'class:resize_onscreen' })"));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    bool                          buttonHeld = false;
    Hyprutils::Utils::CScopeGuard releaseButton([&] {
        if (buttonHeld)
            getFromSocket("/eval hl.plugin.test.click(273, 0)");
    });

    // grab near the top-left corner and pull it past the top-left of the monitor
    OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = 220, y = 220 })"));
    OK(getFromSocket("/eval hl.plugin.test.click(273, 1)"));
    buttonHeld = true;
    OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = -100, y = -100 })"));
    {
        // the top-left edges stop at the monitor, the bottom-right ones stay where they were
        const auto [X, Y] = activeWindowAt();
        const auto [W, H] = activeWindowSize();
        EXPECT(nearly(X, 2), true);
        EXPECT(nearly(Y, 2), true);
        EXPECT(nearly(X + W, 500), true);
        EXPECT(nearly(Y + H, 400), true);
    }
    OK(getFromSocket("/eval hl.plugin.test.click(273, 0)"));
    buttonHeld = false;

    Tests::killAllWindows();
    OK(getFromSocket("/reload"));
}
