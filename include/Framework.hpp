#pragma once

#include "Wrappers.hpp"

bool InitWrapperFW();
void CloseWrapperFW();

Wrappers::Context* GetContext();

Wrappers::Window* GetWindow();
