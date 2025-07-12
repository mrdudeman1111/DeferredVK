#include "Input.hpp"

namespace Input
{
    InputMap_t gInput;

    InputMap_t* GetInputMap()
    {
        return& gInput;
    }

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
                    gInput.Forward = true;
                }
                else if(FrameEvent.key.key == SDLK_S)
                {
                    gInput.Back = true;
                }
                else if(FrameEvent.key.key == SDLK_A)
                {
                    gInput.Left = true;
                }
                else if(FrameEvent.key.key == SDLK_D)
                {
                    gInput.Right = true;
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
                    gInput.Forward = false;
                }
                else if(FrameEvent.key.key == SDLK_S)
                {
                    gInput.Back = false;
                }
                else if(FrameEvent.key.key == SDLK_A)
                {
                    gInput.Left = false;
                }
                else if(FrameEvent.key.key == SDLK_D)
                {
                    gInput.Right = false;
                }
            }
            else if(FrameEvent.type == SDL_EVENT_MOUSE_MOTION)
            {
                gInput.MouseX += FrameEvent.motion.xrel;
                gInput.MouseY += FrameEvent.motion.yrel;
            }
        }

        return true;
    }
}
