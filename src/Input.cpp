#include "Input.hpp"

namespace Input
{
    /*! \brief Polls all input received by the window and updates the global InputMap.
    * loops through all SDL_Event(s), processing important inputs and ignoring unimportant inputs. returns false when the quit signal has been received.
    */
    bool PollInputs()
    {
        SDL_Event FrameEvent;
        while(SDL_PollEvent(&FrameEvent))
        {
            if(FrameEvent.type == SDL_EVENT_QUIT) // App close signal received
            {
                return false;
            }
            else if(FrameEvent.type == SDL_EVENT_KEY_DOWN)
            {
                if(FrameEvent.key.key == SDLK_W)
                {
                    Input::InputMap.Forward = true;
                }
                else if(FrameEvent.key.key == SDLK_S)
                {
                    Input::InputMap.Back = true;
                }
                else if(FrameEvent.key.key == SDLK_A)
                {
                    Input::InputMap.Left = true;
                }
                else if(FrameEvent.key.key == SDLK_D)
                {
                    Input::InputMap.Right = true;
                }
            }
            else if(FrameEvent.type == SDL_EVENT_KEY_UP)
            {
                if(FrameEvent.key.key == SDLK_ESCAPE)
                {
                    return false;
                }
                else if(FrameEvent.key.key == SDLK_W)
                {
                    Input::InputMap.Forward = false;
                }
                else if(FrameEvent.key.key == SDLK_S)
                {
                    Input::InputMap.Back = false;
                }
                else if(FrameEvent.key.key == SDLK_A)
                {
                    Input::InputMap.Left = false;
                }
                else if(FrameEvent.key.key == SDLK_D)
                {
                    Input::InputMap.Right = false;
                }
            }
            else if(FrameEvent.type == SDL_EVENT_MOUSE_MOTION)
            {
                Input::InputMap.MouseX = FrameEvent.motion.x;
                Input::InputMap.MouseY = FrameEvent.motion.y;
            }
        }

        return true;
    }
}
