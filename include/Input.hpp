#include "Wrappers.hpp"

namespace Input
{
    struct
    {
        bool Forward;
        bool Back;
        bool Left;
        bool Right;
        float MouseX;
        float MouseY;
    } InputMap;

    bool PollInputs();
}