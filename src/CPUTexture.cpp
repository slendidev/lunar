#include "CPUTexture.h"

#include <format>
#include <stdexcept>

#if defined(__clang__)
#	pragma clang diagnostic push
#	pragma clang diagnostic ignored "-Wreserved-identifier"
#	pragma clang diagnostic ignored "-Wcast-qual"
#	pragma clang diagnostic ignored "-Wimplicit-fallthrough"
#	pragma clang diagnostic ignored "-Wmissing-field-initializers"
#	pragma clang diagnostic ignored "-Wused-but-marked-unused"
#	pragma clang diagnostic ignored "-Wmissing-prototypes"
#	pragma clang diagnostic ignored "-Wextra-semi-stmt"
#	pragma clang diagnostic ignored "-Wimplicit-int-conversion"
#	pragma clang diagnostic ignored "-Wsign-conversion"
#	pragma clang diagnostic ignored "-Wshorten-64-to-32"
#	pragma clang diagnostic ignored "-Wconversion"
#	pragma clang diagnostic ignored "-Wcomma"
#	pragma clang diagnostic ignored "-Wdouble-promotion"
#	pragma clang diagnostic ignored "-Wimplicit-float-conversion"
#	pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#	pragma clang diagnostic ignored "-Wdeprecated-declarations"
#	pragma clang diagnostic ignored "-Wdisabled-macro-expansion"
#	pragma clang diagnostic ignored "-Wsign-compare"
#	pragma clang diagnostic ignored "-Wfloat-equal"
#	pragma clang diagnostic ignored "-Wpacked"
#	pragma clang diagnostic ignored "-Wold-style-cast"
#	pragma clang diagnostic ignored "-Wexit-time-destructors"
#	pragma clang diagnostic ignored "-Wglobal-constructors"
#	pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"
#	pragma clang diagnostic ignored "-Wcast-align"
#	pragma clang diagnostic ignored "-Wcast-qual"
#	pragma clang diagnostic ignored "-Wshadow"
#	pragma clang diagnostic ignored "-Wnewline-eof"
#	pragma clang diagnostic ignored "-Wformat-nonliteral"
#	pragma clang diagnostic ignored "-Wswitch-default"
#	pragma clang diagnostic ignored "-Wswitch-enum"
#	pragma clang diagnostic ignored "-Wcovered-switch-default"
#	pragma clang diagnostic ignored "-Wdocumentation"
#	pragma clang diagnostic ignored "-Wdocumentation-unknown-command"
#	pragma clang diagnostic ignored "-Wextra-semi"
#	pragma clang diagnostic ignored "-Wundef"
#	pragma clang diagnostic ignored "-Wreserved-macro-identifier"
#	pragma clang diagnostic ignored "-Wc++98-compat"
#	pragma clang diagnostic ignored "-Wc++98-compat-pedantic"
#	pragma clang diagnostic ignored "-Wweak-vtables"
#	pragma clang diagnostic ignored "-Wswitch"
#	pragma clang diagnostic ignored "-Wunused-macros"
#	pragma clang diagnostic ignored "-Wextra"
#endif
#define STB_IMAGE_IMPLEMENTATION
#include "../thirdparty/stb/stb_image.h"
#if defined(__clang__)
#	pragma clang diagnostic pop
#endif

namespace Lunar {

CPUTexture::CPUTexture(std::filesystem::path const &path)
{
	int width_out { 0 };
	int height_out { 0 };
	int channels_out { 0 };
	stbi_uc *data { stbi_load(path.string().c_str(), &width_out, &height_out,
		&channels_out, STBI_rgb_alpha) };
	if (!data) {
		throw std::runtime_error(
		    std::format("Failed to load texture: {}", path.string()));
	}

	width = static_cast<uint32_t>(width_out);
	height = static_cast<uint32_t>(height_out);
	format = vk::Format::eR8G8B8A8Unorm;
	pixels.assign(data, data + (width * height * 4));
	stbi_image_free(data);
}

} // namespace Lunar
