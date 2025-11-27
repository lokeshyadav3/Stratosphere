#include <Engine/Application.h>
#include <iostream>

class MySampleApp : public Engine::Application
{
public:
    MySampleApp() {}
    ~MySampleApp() {}

    void OnUpdate(Engine::TimeStep ts) override
    {
        // game logic / ECS tick
    }

    void OnRender() override
    {
        // submit rendering work (or call renderer API)
    }
};

Engine::Application *Engine::CreateApplication()
{
    return new MySampleApp();
}

int main(int argc, char **argv)
{
    auto app = std::unique_ptr<Engine::Application>(Engine::CreateApplication());
    app->Run();
    return 0;
}