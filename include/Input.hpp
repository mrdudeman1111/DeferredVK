#include "Wrappers.hpp"

namespace Input
{
    struct InputMap_t
    {
        bool Forward;
        bool Back;
        bool Left;
        bool Right;
        float MouseX;
        float MouseY;
    };

    InputMap_t* GetInputMap();

    bool PollInputs();
}