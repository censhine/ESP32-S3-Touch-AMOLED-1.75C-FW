// Deterministic dependencies for the production lifecycle method bodies.
#include <cstdlib>
#include <cstdint>
#include <deque>
#include <functional>
#include <iostream>
#include <unordered_map>

using namespace std;
#define ESP_UTILS_LOGD(...) ((void)0)
#define ESP_UTILS_LOGE(...) ((void)0)
#define ESP_UTILS_LOGW(...) ((void)0)
#define ESP_UTILS_CHECK_FALSE_RETURN(condition, result, ...) do { if (!(condition)) return result; } while (0)
#define ESP_UTILS_CHECK_NULL_RETURN(condition, result, ...) ESP_UTILS_CHECK_FALSE_RETURN(condition, result)
#define ESP_UTILS_CHECK_FALSE_GOTO(condition, target, ...) do { if (!(condition)) goto target; } while (0)

int checks = 0;
void require(bool value, const char *message)
{
    ++checks;
    if (!value) { cerr << "FAIL: " << message << '\n'; exit(1); }
}
bool next_result(deque<bool> &results)
{
    if (results.empty()) return true;
    const bool result = results.front(); results.pop_front(); return result;
}

struct App {
    // Enum and lifecycle flag declarations are inserted from the real header.
    APP_LIFECYCLE_DECLARATIONS
    int _id;
    struct { struct { bool enable_default_screen = true; bool enable_recycle_resource = true; } flags; } _active_config;
    bool run_ok = true, setup_ok = true, restore_theme_ok = true, runtime_alive = false;
    bool theme_saved = false, owns_current_screen = false;
    int run_calls = 0, resume_calls = 0, close_calls = 0, load_calls = 0, end_record_calls = 0;
    deque<bool> close_results;
    function<void()> on_run;
    explicit App(int id) : _id(id) {}
    const char *getName() { return "test"; }
    bool checkInitialized() { return true; }
    bool saveRecentScreen(bool) { return true; }
    bool resetRecordResource() { _flags.is_resource_recording = false; return true; }
    bool startRecordResource() { _flags.is_resource_recording = true; return true; }
    bool endRecordResource() { ++end_record_calls; _flags.is_resource_recording = false; return true; }
    bool initDefaultScreen() { owns_current_screen = setup_ok; return setup_ok; }
    bool saveDisplayTheme() { theme_saved = true; return true; }
    bool loadDisplayTheme() { return theme_saved && restore_theme_ok; }
    bool run() { ++run_calls; runtime_alive = true; if (on_run) on_run(); return run_ok; }
    bool close() { ++close_calls; if (!next_result(close_results)) return false; runtime_alive = false; return true; }
    bool cleanResource() { require(!runtime_alive, "UI cleanup waits for runtime shutdown"); return true; }
    bool cleanRecordResource() { return true; }
    bool cleanDefaultScreen() { return true; }
    bool enableAutoClean() { require(owns_current_screen, "auto-clean never adopts an unrelated screen"); return true; }
    bool loadRecentScreen() { ++load_calls; return true; }
    bool processRun();
    bool processClose(bool);
};

struct Display {
    bool run_ok = true;
    int close_calls = 0, main_loads = 0, resume_calls = 0;
    deque<bool> close_results;
    function<void()> on_close;
    bool processAppRun(App *) { return run_ok; }
    bool processAppClose(App *) { ++close_calls; if (on_close) on_close(); return next_result(close_results); }
    bool processMainScreenLoad() { ++main_loads; return true; }
    bool processAppResume(App *) { ++resume_calls; return true; }
};
struct Context { Display display; Display &getDisplay() { return display; } };
struct Manager {
    Context &_system_context;
    struct { struct { int max_running_num = 0; } app;
        struct { bool enable_app_save_snapshot = false; } flags; } _core_data;
    App *_active_app = nullptr;
    unordered_map<int, App *> _id_installed_app_map, _id_running_app_map;
    bool run_extra_ok = true;
    int close_extra_calls = 0;
    deque<bool> close_extra_results;
    explicit Manager(Context &context) : _system_context(context) {}
    void install(App &app) { _id_installed_app_map[app._id] = &app; }
    bool owns(App &app) { return _id_running_app_map.count(app._id) != 0; }
    bool processAppResume(App *app) { ++app->resume_calls; return true; }
    bool processAppRunExtra(App *) { return run_extra_ok; }
    bool processAppResumeExtra(App *) { return true; }
    bool processAppCloseExtra(App *app) {
        ++close_extra_calls;
        if (!next_result(close_extra_results)) return false;
        if (_active_app == app) return _system_context.display.processMainScreenLoad();
        return true;
    }
    bool releaseAppSnapshot(App *) { return true; }
    bool startApp(int);
    bool processAppRun(App *);
    bool processAppClose(App *);
};

PRODUCTION_LIFECYCLE_METHODS

int main()
{
    {
        Context c; Manager m(c); App app(1); m.install(app);
        app.on_run = [&] { require(m.owns(app), "manager owns runtime before run allocates"); };
        require(m.startApp(1) && m.owns(app), "successful launch remains owned");
        require(app._status == App::Status::RUNNING && m._active_app == &app, "successful launch is active");
        require(m.startApp(1) && app.resume_calls == 1 && app.run_calls == 1, "healthy START resumes");
        require(m.processAppClose(&app) && !m.owns(app), "ordinary STOP releases ownership");
    }
    {
        Context c; Manager m(c); App app(2); m.install(app); app.run_ok = false;
        require(!m.startApp(2) && !m.owns(app), "failed launch with completed cleanup is erased");
        require(app.close_calls == 1, "successful processRun self-cleanup is not repeated by manager");
        require(!app.runtime_alive && m._active_app == nullptr, "cleaned failure leaves no active runtime");
    }
    {
        Context c; Manager m(c); App app(3); m.install(app); app.run_ok = false; app.close_results = {false, true};
        require(!m.startApp(3) && m.owns(app), "failed launch with blocked shutdown stays owned");
        require(app.runtime_alive && app._status == App::Status::CLOSING, "failed shutdown retains live runtime");
        require(app.close_calls == 1, "failed startup does not immediately repeat blocking close");
        require(m.processAppClose(&app) && !m.owns(app), "STOP retry finishes retained startup cleanup");
        require(!app.runtime_alive && app.close_calls == 2, "retry actually releases runtime");
    }
    {
        Context c; Manager m(c); App app(4); m.install(app); app.run_ok = false; app.close_results = {false, false, true};
        require(!m.startApp(4), "retained START setup");
        app.run_ok = true;
        app._status = App::Status::PAUSED;
        require(!m.startApp(4) && app.run_calls == 1 && app.resume_calls == 0, "START cannot resume or duplicate retained runtime");
        require(m.startApp(4) && app.run_calls == 2 && app.resume_calls == 0, "START cleans then launches a fresh runtime");
        require(m.processAppClose(&app), "fresh runtime closes independently");
    }
    {
        Context c; Manager m(c); App app(5); m.install(app); require(m.startApp(5), "display retry setup");
        c.display.close_results = {false, true}; m.close_extra_results = {false, true};
        require(!m.processAppClose(&app) && m.owns(app), "failed display cleanup retains ownership");
        require(!m.processAppClose(&app) && m.owns(app), "failed extra cleanup retains ownership");
        require(app.close_calls == 1, "app close is not repeated after runtime cleanup succeeds");
        require(m.processAppClose(&app) && !m.owns(app), "third STOP finishes remaining stage");
        require(c.display.close_calls == 2, "successful display close is not repeated for extra-stage retry");
    }
    {
        Context c; Manager m(c); App app(6); m.install(app); require(m.startApp(6), "theme retry setup");
        app.restore_theme_ok = false;
        require(!m.processAppClose(&app) && m.owns(app), "app UI cleanup failure retains ownership");
        app.restore_theme_ok = true;
        require(m.processAppClose(&app) && app.close_calls == 1, "app-level cleanup retry skips completed runtime close");
    }
    {
        Context c; Manager m(c); App previous(7), failed(8); m.install(previous); m.install(failed);
        require(m.startApp(7), "previous active app setup");
        failed.run_ok = false; failed.close_results = {false, true};
        require(!m.startApp(8), "new launch fails while an app is active");
        require(m._active_app == &previous && previous.runtime_alive && m.owns(previous), "previous active runtime remains owned and active");
        require(previous.close_calls == 0 && previous.resume_calls == 0 && previous.load_calls == 1, "previous view restored without lifecycle duplication");
        require(m.processAppClose(&failed) && m._active_app == &previous, "background cleanup retry does not disturb previous app");
    }
    {
        Context c; Manager m(c); App app(9); m.install(app); app.setup_ok = false;
        require(!m.startApp(9) && !m.owns(app), "partial setup failure cleans ownership");
        require(app.run_calls == 0 && app.end_record_calls == 0, "pre-run setup failure preserves existing return behavior");
        require(app.close_calls == 0, "setup failure does not close a never-started runtime");
    }
    {
        Context c; Manager m(c); App app(10); m.install(app); c.display.run_ok = false;
        require(!m.startApp(10) && !m.owns(app), "display startup failure cleans ownership");
        require(app.run_calls == 0 && app.close_calls == 0, "runtime never started is not destructively closed");
    }
    {
        Context c; Manager m(c); App app(11); m.install(app); m.run_extra_ok = false; app.close_results = {false, true};
        require(!m.startApp(11) && m.owns(app), "extra startup failure with failed close stays owned");
        require(m.processAppClose(&app) && !m.owns(app), "extra startup failure can be retried normally");
    }
    {
        Context c; Manager m(c); App app(12); m.install(app); require(m.startApp(12), "reentrant close setup");
        c.display.on_close = [&] { require(!m.processAppClose(&app), "recursive STOP cannot restart active close stages"); };
        require(m.processAppClose(&app) && !m.owns(app), "outer close finishes after rejected recursive STOP");
        require(app.close_calls == 1 && c.display.close_calls == 1, "recursive STOP never double-closes resources");
    }
    cout << "PASS: " << checks << " production app/manager lifecycle checks\n";
}
