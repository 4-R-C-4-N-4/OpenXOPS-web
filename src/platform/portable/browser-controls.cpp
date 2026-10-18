// Browser-only input presets. Codes are the game's original XOPS codes, not VKs.
#include "config.h"
#include <emscripten/emscripten.h>

// Keycodes here are verified against Config::SetDefaultConfig; browser preset
// changes only key assignments, not the user's native config files.

extern Config GameConfig;

extern "C" EMSCRIPTEN_KEEPALIVE void openxops_apply_controls(int preset)
{
	if( preset != 1 ){ return; } // Classic leaves config.dat and its defaults untouched.

	GameConfig.SetKeycode(KEY_TURNUP, 0);
	GameConfig.SetKeycode(KEY_TURNDOWN, 1);
	GameConfig.SetKeycode(KEY_TURNLEFT, 2);
	GameConfig.SetKeycode(KEY_TURNRIGHT, 3);
	GameConfig.SetKeycode(KEY_MOVEFORWARD, 54);  // W
	GameConfig.SetKeycode(KEY_MOVEBACKWARD, 50); // S
	GameConfig.SetKeycode(KEY_MOVELEFT, 32);     // A
	GameConfig.SetKeycode(KEY_MOVERIGHT, 35);    // D
	GameConfig.SetKeycode(KEY_WALK, 20);         // Shift
	GameConfig.SetKeycode(KEY_JUMP, 17);         // Space
	GameConfig.SetKeycode(KEY_RELOAD, 49);       // R
	GameConfig.SetKeycode(KEY_DROPWEAPON, 38);    // G
	GameConfig.SetKeycode(KEY_ZOOMSCOPE, 19);     // Right mouse; independent of Shift walk
	GameConfig.SetKeycode(KEY_SHOTMODE, 55);      // X
	GameConfig.SetKeycode(KEY_SWITCHWEAPON, 48);  // Q
	GameConfig.SetKeycode(KEY_WEAPON1, 23);       // 1
	GameConfig.SetKeycode(KEY_WEAPON2, 24);       // 2
	GameConfig.SetKeycode(KEY_SHOT, 18);          // left mouse
}