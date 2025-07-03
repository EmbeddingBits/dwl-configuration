// somebar - dwl bar
// See LICENSE file for copyright and license details.

#pragma once
#include "common.hpp"

constexpr bool topbar = true;

constexpr int paddingX = 10;
constexpr int paddingY = 3;

// See https://docs.gtk.org/Pango/type_func.FontDescription.from_string.html
constexpr const char* font = "JetBrainsMono Nerd Font Bold 12";

//constexpr ColorScheme colorInactive = {Color(0xec, 0xef, 0xf4), Color(0x2e, 0x34, 0x40)};
//constexpr ColorScheme colorActive = {Color(0xd8, 0xde, 0xe9), Color(0x5e, 0x81, 0xac)};
constexpr ColorScheme colorInactive = {Color(0xcd, 0xd6, 0xf4), Color(0x18, 0x18, 0x25)};
constexpr ColorScheme colorActive = {Color(0x18, 0x18, 0x25), Color(0x89, 0xb4, 0xfa)};
constexpr const char* termcmd[] = {"kitty", nullptr};

static std::vector<std::string> tagNames = {
	"  ", " 󰈹 ", "  ",
	"  ", "  ",
};

constexpr Button buttons[] = {
	{ ClkStatusText,   BTN_RIGHT,  spawn,      {.v = termcmd} },
};
