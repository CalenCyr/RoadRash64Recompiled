#pragma once
#include <SDL.h>
#include <string>

/*
Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>
  
This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:
  
1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required. 
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.


*/
// Exact-device mappings from SDL_GameControllerDB, commit 5a12daa568d19344f9b6e9286ef5929833b25c7c.
// C buttons are exposed by this device as right-stick axes. Do not guess
// mappings for other 8BitDo products or connection modes from their names.
inline constexpr const char* rr64_controller_mappings[] = {
    "03000000c82d00001930000000000000,8BitDo 64,a:b0,b:b1,back:b10,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b12,leftshoulder:b6,leftstick:b13,lefttrigger:b8,leftx:a0,lefty:a1,rightshoulder:b7,righttrigger:b9,rightx:a3,righty:a4,start:b11,platform:Windows,",
    "03000000c82d00001930000000000000,8BitDo 64,a:b0,b:b1,back:b10,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b12,leftshoulder:b6,leftstick:b13,lefttrigger:b8,leftx:a0,lefty:a1,rightshoulder:b7,righttrigger:b9,rightx:a2,righty:a3,start:b11,platform:Mac OS X,",
    "03000000c82d00001930000000020000,8BitDo 64,a:b0,b:b1,back:b10,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b12,leftshoulder:b6,leftstick:b13,lefttrigger:b8,leftx:a0,lefty:a1,rightshoulder:b7,righttrigger:b9,rightx:a2,righty:a3,start:b11,platform:Mac OS X,",
    "03000000c82d00001930000001000000,8BitDo 64,a:b0,b:b1,back:b10,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b12,leftshoulder:b6,leftstick:b13,lefttrigger:b8,leftx:a0,lefty:a1,rightshoulder:b7,righttrigger:b9,rightx:a2,righty:a3,start:b11,platform:Mac OS X,",
    "03000000c82d00001930000011010000,8BitDo 64,a:b0,b:b1,back:b10,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b12,leftshoulder:b6,leftstick:b13,lefttrigger:b8,leftx:a0,lefty:a1,rightshoulder:b7,righttrigger:b9,rightx:a2,righty:a3,start:b11,platform:Linux,",
    "05000000c82d00001930000001000000,8BitDo 64,a:b0,b:b1,back:b10,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b12,leftshoulder:b6,leftstick:b13,lefttrigger:b8,leftx:a0,lefty:a1,rightshoulder:b7,righttrigger:b9,rightx:a2,righty:a3,start:b11,platform:Linux,",
};

inline bool rr64_register_controller_mappings() {
    bool success = true;
    const std::string platform = std::string("platform:") + SDL_GetPlatform() + ",";
    for (const char* mapping : rr64_controller_mappings) {
        // AddMapping (unlike the database-file loader) does not filter platform.
        // USB GUIDs may be identical even though OS axis numbering differs.
        if (std::string(mapping).find(platform) == std::string::npos) continue;
        if (SDL_GameControllerAddMapping(mapping) < 0) success = false;
    }
    return success;
}
