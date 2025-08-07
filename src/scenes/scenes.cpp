#include "scenes.hpp"
#include "default.hpp"

Scenes::Scenes() {
    scenes.emplace(DEFAULT, std::make_unique<Default>());
}

void Scenes::render() {
    /*
     * Main render loop
     * */
    scenes[DEFAULT]->render();
}
