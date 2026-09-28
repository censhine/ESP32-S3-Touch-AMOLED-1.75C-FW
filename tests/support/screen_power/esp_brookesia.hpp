#pragma once
namespace esp_brookesia::systems::phone {
class App {
public:
    const char *name = "Clock";
    const char *getName() const { return name; }
};
class Manager {
public:
    App *active = nullptr;
    const App *getActiveApp() const { return active; }
};
class Phone {
public:
    Manager manager;
    Manager &getManager() { return manager; }
};
}
