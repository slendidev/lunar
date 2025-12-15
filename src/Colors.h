#pragma once

#include <smath.hpp>

namespace Lunar::Colors {

constexpr auto hex_to_vec3(uint32_t hex) -> smath::Vec3
{
	return smath::Vec3 { static_cast<float>((hex >> 16) & 0xFF) / 255.0f,
		static_cast<float>((hex >> 8) & 0xFF) / 255.0f,
		static_cast<float>(hex & 0xFF) / 255.0f };
}

inline constexpr smath::Vec3 WHITE { hex_to_vec3(0xFFFFFF) };
inline constexpr smath::Vec3 SILVER { hex_to_vec3(0xC0C0C0) };
inline constexpr smath::Vec3 GRAY { hex_to_vec3(0x808080) };
inline constexpr smath::Vec3 BLACK { hex_to_vec3(0x000000) };
inline constexpr smath::Vec3 RED { hex_to_vec3(0xFF0000) };
inline constexpr smath::Vec3 MAROON { hex_to_vec3(0x800000) };
inline constexpr smath::Vec3 YELLOW { hex_to_vec3(0xFFFF00) };
inline constexpr smath::Vec3 OLIVE { hex_to_vec3(0x808000) };
inline constexpr smath::Vec3 LIME { hex_to_vec3(0x00FF00) };
inline constexpr smath::Vec3 GREEN { hex_to_vec3(0x008000) };
inline constexpr smath::Vec3 AQUA { hex_to_vec3(0x00FFFF) };
inline constexpr smath::Vec3 TEAL { hex_to_vec3(0x008080) };
inline constexpr smath::Vec3 BLUE { hex_to_vec3(0x0000FF) };
inline constexpr smath::Vec3 NAVY { hex_to_vec3(0x000080) };
inline constexpr smath::Vec3 FUCHSIA { hex_to_vec3(0xFF00FF) };
inline constexpr smath::Vec3 PURPLE { hex_to_vec3(0x800080) };

// Pink colors
inline constexpr smath::Vec3 MEDIUM_VIOLET_RED { hex_to_vec3(0xC71585) };
inline constexpr smath::Vec3 DEEP_PINK { hex_to_vec3(0xFF1493) };
inline constexpr smath::Vec3 PALE_VIOLET_RED { hex_to_vec3(0xDB7093) };
inline constexpr smath::Vec3 HOT_PINK { hex_to_vec3(0xFF69B4) };
inline constexpr smath::Vec3 LIGHT_PINK { hex_to_vec3(0xFFB6C1) };
inline constexpr smath::Vec3 PINK { hex_to_vec3(0xFFC0CB) };

// Red colors
inline constexpr smath::Vec3 DARK_RED { hex_to_vec3(0x8B0000) };
inline constexpr smath::Vec3 FIREBRICK { hex_to_vec3(0xB22222) };
inline constexpr smath::Vec3 CRIMSON { hex_to_vec3(0xDC143C) };
inline constexpr smath::Vec3 INDIAN_RED { hex_to_vec3(0xCD5C5C) };
inline constexpr smath::Vec3 LIGHT_CORAL { hex_to_vec3(0xF08080) };
inline constexpr smath::Vec3 SALMON { hex_to_vec3(0xFA8072) };
inline constexpr smath::Vec3 DARK_SALMON { hex_to_vec3(0xE9967A) };
inline constexpr smath::Vec3 LIGHT_SALMON { hex_to_vec3(0xFFA07A) };

// Orange colors
inline constexpr smath::Vec3 ORANGE_RED { hex_to_vec3(0xFF4500) };
inline constexpr smath::Vec3 TOMATO { hex_to_vec3(0xFF6347) };
inline constexpr smath::Vec3 DARK_ORANGE { hex_to_vec3(0xFF8C00) };
inline constexpr smath::Vec3 CORAL { hex_to_vec3(0xFF7F50) };
inline constexpr smath::Vec3 ORANGE { hex_to_vec3(0xFFA500) };

// Yellow colors
inline constexpr smath::Vec3 DARK_KHAKI { hex_to_vec3(0xBDB76B) };
inline constexpr smath::Vec3 GOLD { hex_to_vec3(0xFFD700) };
inline constexpr smath::Vec3 KHAKI { hex_to_vec3(0xF0E68C) };
inline constexpr smath::Vec3 PEACH_PUFF { hex_to_vec3(0xFFDAB9) };
inline constexpr smath::Vec3 PALE_GOLDENROD { hex_to_vec3(0xEEE8AA) };
inline constexpr smath::Vec3 MOCCASIN { hex_to_vec3(0xFFE4B5) };
inline constexpr smath::Vec3 PAPAYA_WHIP { hex_to_vec3(0xFFEFD5) };
inline constexpr smath::Vec3 LIGHT_GOLDENROD_YELLOW { hex_to_vec3(0xFAFAD2) };
inline constexpr smath::Vec3 LEMON_CHIFFON { hex_to_vec3(0xFFFACD) };
inline constexpr smath::Vec3 LIGHT_YELLOW { hex_to_vec3(0xFFFFE0) };

// Brown colors
inline constexpr smath::Vec3 BROWN { hex_to_vec3(0xA52A2A) };
inline constexpr smath::Vec3 SADDLE_BROWN { hex_to_vec3(0x8B4513) };
inline constexpr smath::Vec3 SIENNA { hex_to_vec3(0xA0522D) };
inline constexpr smath::Vec3 CHOCOLATE { hex_to_vec3(0xD2691E) };
inline constexpr smath::Vec3 DARK_GOLDENROD { hex_to_vec3(0xB8860B) };
inline constexpr smath::Vec3 PERU { hex_to_vec3(0xCD853F) };
inline constexpr smath::Vec3 ROSY_BROWN { hex_to_vec3(0xBC8F8F) };
inline constexpr smath::Vec3 GOLDENROD { hex_to_vec3(0xDAA520) };
inline constexpr smath::Vec3 SANDY_BROWN { hex_to_vec3(0xF4A460) };
inline constexpr smath::Vec3 TAN { hex_to_vec3(0xD2B48C) };
inline constexpr smath::Vec3 BURLYWOOD { hex_to_vec3(0xDEB887) };
inline constexpr smath::Vec3 WHEAT { hex_to_vec3(0xF5DEB3) };
inline constexpr smath::Vec3 NAVAJO_WHITE { hex_to_vec3(0xFFDEAD) };
inline constexpr smath::Vec3 BISQUE { hex_to_vec3(0xFFE4C4) };
inline constexpr smath::Vec3 BLANCHED_ALMOND { hex_to_vec3(0xFFEBCD) };
inline constexpr smath::Vec3 CORNSILK { hex_to_vec3(0xFFF8DC) };

// Purple, violet, magenta colors
inline constexpr smath::Vec3 INDIGO { hex_to_vec3(0x4B0082) };
inline constexpr smath::Vec3 DARK_MAGENTA { hex_to_vec3(0x8B008B) };
inline constexpr smath::Vec3 DARK_VIOLET { hex_to_vec3(0x9400D3) };
inline constexpr smath::Vec3 DARK_SLATE_BLUE { hex_to_vec3(0x483D8B) };
inline constexpr smath::Vec3 BLUE_VIOLET { hex_to_vec3(0x8A2BE2) };
inline constexpr smath::Vec3 DARK_ORCHID { hex_to_vec3(0x9932CC) };
inline constexpr smath::Vec3 MAGENTA { hex_to_vec3(0xFF00FF) };
inline constexpr smath::Vec3 SLATE_BLUE { hex_to_vec3(0x6A5ACD) };
inline constexpr smath::Vec3 MEDIUM_SLATE_BLUE { hex_to_vec3(0x7B68EE) };
inline constexpr smath::Vec3 MEDIUM_ORCHID { hex_to_vec3(0xBA55D3) };
inline constexpr smath::Vec3 MEDIUM_PURPLE { hex_to_vec3(0x9370DB) };
inline constexpr smath::Vec3 ORCHID { hex_to_vec3(0xDA70D6) };
inline constexpr smath::Vec3 VIOLET { hex_to_vec3(0xEE82EE) };
inline constexpr smath::Vec3 PLUM { hex_to_vec3(0xDDA0DD) };
inline constexpr smath::Vec3 THISTLE { hex_to_vec3(0xD8BFD8) };
inline constexpr smath::Vec3 LAVENDER { hex_to_vec3(0xE6E6FA) };

// Blue colors
inline constexpr smath::Vec3 MIDNIGHT_BLUE { hex_to_vec3(0x191970) };
inline constexpr smath::Vec3 DARK_BLUE { hex_to_vec3(0x00008B) };
inline constexpr smath::Vec3 MEDIUM_BLUE { hex_to_vec3(0x0000CD) };
inline constexpr smath::Vec3 ROYAL_BLUE { hex_to_vec3(0x4169E1) };
inline constexpr smath::Vec3 STEEL_BLUE { hex_to_vec3(0x4682B4) };
inline constexpr smath::Vec3 DODGER_BLUE { hex_to_vec3(0x1E90FF) };
inline constexpr smath::Vec3 DEEP_SKY_BLUE { hex_to_vec3(0x00BFFF) };
inline constexpr smath::Vec3 CORNFLOWER_BLUE { hex_to_vec3(0x6495ED) };
inline constexpr smath::Vec3 SKY_BLUE { hex_to_vec3(0x87CEEB) };
inline constexpr smath::Vec3 LIGHT_SKY_BLUE { hex_to_vec3(0x87CEFA) };
inline constexpr smath::Vec3 LIGHT_STEEL_BLUE { hex_to_vec3(0xB0C4DE) };
inline constexpr smath::Vec3 LIGHT_BLUE { hex_to_vec3(0xADD8E6) };
inline constexpr smath::Vec3 POWDER_BLUE { hex_to_vec3(0xB0E0E6) };

// Cyan colors
inline constexpr smath::Vec3 DARK_CYAN { hex_to_vec3(0x008B8B) };
inline constexpr smath::Vec3 LIGHT_SEA_GREEN { hex_to_vec3(0x20B2AA) };
inline constexpr smath::Vec3 CADET_BLUE { hex_to_vec3(0x5F9EA0) };
inline constexpr smath::Vec3 DARK_TURQUOISE { hex_to_vec3(0x00CED1) };
inline constexpr smath::Vec3 MEDIUM_TURQUOISE { hex_to_vec3(0x48D1CC) };
inline constexpr smath::Vec3 TURQUOISE { hex_to_vec3(0x40E0D0) };
inline constexpr smath::Vec3 CYAN { hex_to_vec3(0x00FFFF) };
inline constexpr smath::Vec3 AQUAMARINE { hex_to_vec3(0x7FFFD4) };
inline constexpr smath::Vec3 PALE_TURQUOISE { hex_to_vec3(0xAFEEEE) };
inline constexpr smath::Vec3 LIGHT_CYAN { hex_to_vec3(0xE0FFFF) };

// Green colors
inline constexpr smath::Vec3 DARK_GREEN { hex_to_vec3(0x006400) };
inline constexpr smath::Vec3 DARK_OLIVE_GREEN { hex_to_vec3(0x556B2F) };
inline constexpr smath::Vec3 FOREST_GREEN { hex_to_vec3(0x228B22) };
inline constexpr smath::Vec3 SEA_GREEN { hex_to_vec3(0x2E8B57) };
inline constexpr smath::Vec3 OLIVE_DRAB { hex_to_vec3(0x6B8E23) };
inline constexpr smath::Vec3 MEDIUM_SEA_GREEN { hex_to_vec3(0x3CB371) };
inline constexpr smath::Vec3 LIME_GREEN { hex_to_vec3(0x32CD32) };
inline constexpr smath::Vec3 SPRING_GREEN { hex_to_vec3(0x00FF7F) };
inline constexpr smath::Vec3 MEDIUM_SPRING_GREEN { hex_to_vec3(0x00FA9A) };
inline constexpr smath::Vec3 DARK_SEA_GREEN { hex_to_vec3(0x8FBC8F) };
inline constexpr smath::Vec3 MEDIUM_AQUAMARINE { hex_to_vec3(0x66CDAA) };
inline constexpr smath::Vec3 YELLOW_GREEN { hex_to_vec3(0x9ACD32) };
inline constexpr smath::Vec3 LAWN_GREEN { hex_to_vec3(0x7CFC00) };
inline constexpr smath::Vec3 CHARTREUSE { hex_to_vec3(0x7FFF00) };
inline constexpr smath::Vec3 LIGHT_GREEN { hex_to_vec3(0x90EE90) };
inline constexpr smath::Vec3 GREEN_YELLOW { hex_to_vec3(0xADFF2F) };
inline constexpr smath::Vec3 PALE_GREEN { hex_to_vec3(0x98FB98) };

// White colors
inline constexpr smath::Vec3 MISTY_ROSE { hex_to_vec3(0xFFE4E1) };
inline constexpr smath::Vec3 ANTIQUE_WHITE { hex_to_vec3(0xFAEBD7) };
inline constexpr smath::Vec3 LINEN { hex_to_vec3(0xFAF0E6) };
inline constexpr smath::Vec3 BEIGE { hex_to_vec3(0xF5F5DC) };
inline constexpr smath::Vec3 WHITE_SMOKE { hex_to_vec3(0xF5F5F5) };
inline constexpr smath::Vec3 LAVENDER_BLUSH { hex_to_vec3(0xFFF0F5) };
inline constexpr smath::Vec3 OLD_LACE { hex_to_vec3(0xFDF5E6) };
inline constexpr smath::Vec3 ALICE_BLUE { hex_to_vec3(0xF0F8FF) };
inline constexpr smath::Vec3 SEASHELL { hex_to_vec3(0xFFF5EE) };
inline constexpr smath::Vec3 GHOST_WHITE { hex_to_vec3(0xF8F8FF) };
inline constexpr smath::Vec3 HONEYDEW { hex_to_vec3(0xF0FFF0) };
inline constexpr smath::Vec3 FLORAL_WHITE { hex_to_vec3(0xFFFAF0) };
inline constexpr smath::Vec3 AZURE { hex_to_vec3(0xF0FFFF) };
inline constexpr smath::Vec3 MINT_CREAM { hex_to_vec3(0xF5FFFA) };
inline constexpr smath::Vec3 SNOW { hex_to_vec3(0xFFFAFA) };
inline constexpr smath::Vec3 IVORY { hex_to_vec3(0xFFFFF0) };

// Gray and black colors
inline constexpr smath::Vec3 DARK_SLATE_GRAY { hex_to_vec3(0x2F4F4F) };
inline constexpr smath::Vec3 DIM_GRAY { hex_to_vec3(0x696969) };
inline constexpr smath::Vec3 SLATE_GRAY { hex_to_vec3(0x708090) };
inline constexpr smath::Vec3 LIGHT_SLATE_GRAY { hex_to_vec3(0x778899) };
inline constexpr smath::Vec3 DARK_GRAY { hex_to_vec3(0xA9A9A9) };
inline constexpr smath::Vec3 LIGHT_GRAY { hex_to_vec3(0xD3D3D3) };
inline constexpr smath::Vec3 GAINSBORO { hex_to_vec3(0xDCDCDC) };

} // namespace Lunar::Colors
