#pragma once

#include <string>

/*
Format of ANSI Escape Codes
  - The core syntax for a standard 16-color ANSI code follows this structure: `\033[Style;ColorCode m`.
  - Styles: 0 (Normal/Reset), 1 (Bold/Bright), 4 (Underline).
  - Foreground (30-37): 31 (Red), 32 (Green), 33 (Yellow), 34 (Blue).
  - Background (40-47): 41 (Red), 42 (Green), 43 (Yellow), 44 (Blue).
  - Example: a Bold Red Text on a Yellow Background: `\033[1;31;43m`. 

Advanced Colors (256-Color & True Color RGB)
  - Modern terminals allow leveraging a larger spectrum using specific extended formatting structures.
  - 256 Colors: Format via `\033[38;5;[0-255]m` (Foreground) or `\033[48;5;[0-255]m` (Background).
  - True Color (RGB): Format via `\033[38;2;R;G;Bm` (where R, G, B are integers from 0 to 255).
*/

#define RESET        "\033[0m"
#define BLACK        "\033[30m"
#define RED          "\033[31m"
#define GREEN        "\033[32m"
#define YELLOW       "\033[33m"
#define BLUE         "\033[34m"
#define MAGENTA      "\033[35m"
#define CYAN         "\033[36m"
#define WHITE        "\033[37m"
#define BOLD         "\033[1m"
#define BOLD_BLACK   "\033[1m\033[30m"
#define BOLD_RED     "\033[1m\033[31m"
#define BOLD_GREEN   "\033[1m\033[32m"
#define BOLD_YELLOW  "\033[1m\033[33m"
#define BOLD_BLUE    "\033[1m\033[34m"
#define BOLD_MAGENTA "\033[1m\033[35m"
#define BOLD_CYAN    "\033[1m\033[36m"
#define BOLD_WHITE   "\033[1m\033[37m"

constexpr inline std::string rgb(const std::string &s, unsigned char r, unsigned g, unsigned b) {
  auto red = std::to_string(r);
  auto green = std::to_string(g);
  auto blue = std::to_string(b);
  
  return "\033[38;2;" + red + ";" + green + ";" + blue + "m" + s + RESET;
}

constexpr inline std::string rgb(const std::string &s, unsigned int color) {
  unsigned char r = (color >> 16) & 0xFF;
  unsigned char g = (color >> 8) & 0xFF;
  unsigned char b = (color) & 0xFF;

  return rgb(s, r, g, b);
}

inline std::string bold(const std::string &s) {
  return BOLD + s + RESET;
}