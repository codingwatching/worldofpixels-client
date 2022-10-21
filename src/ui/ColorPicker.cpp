#include "ColorPicker.hpp"

#include <charconv>
#include <emscripten.h>

#include <util/byteswap.hpp>
#include <util/misc.hpp>

EM_JS(void, init_color_picker_on, (std::uint32_t boxId, std::uint32_t inputId), {
	var box = Module.EUI.elems[boxId];
	var input = Module.EUI.elems[inputId];
	window["initColorPicker"](box, input);
});

// "#89ABCDEF"
constexpr std::size_t bufSz = 9;

ColorPicker::ColorPicker(std::function<void(RGB_u)> cb)
: Object("span"),
  input("input"),
  cb(std::move(cb)),
  onColorChange(input.createHandler("change", std::bind(&ColorPicker::colorChanged, this))),
  color{{0, 0, 0, 0}} {
	input.appendTo(*this);
	init_color_picker_on(getId(), input.getId());
	addClass("owop-clr-picker");
}

ColorPicker::ColorPicker(ColorPicker&& o) noexcept
: eui::Object(std::move(o)),
  cb(std::move(o.cb)),
  onColorChange(std::move(o.onColorChange)),
  color(o.color) {
	onColorChange.setCb(std::bind(&ColorPicker::colorChanged, this));
}

const ColorPicker& ColorPicker::operator =(ColorPicker&& o) noexcept {
	eui::Object::operator=(std::move(o));
	cb = std::move(o.cb);
	onColorChange = std::move(o.onColorChange);
	color = o.color;
	onColorChange.setCb(std::bind(&ColorPicker::colorChanged, this));
	return *this;
}


void ColorPicker::setColor(RGB_u nclr) {
	if (nclr.rgb == color.rgb) {
		return;
	}

	u32 cssClr = bswap_32(nclr.rgb);
	auto hexClr = svprintf<bufSz>("#%08X", cssClr);
	input.setProperty("value", hexClr);
	setProperty("value", hexClr);
	setProperty("style.backgroundColor", hexClr);
	color = nclr;
}

RGB_u ColorPicker::getColor() const {
	return color;
}

bool ColorPicker::colorChanged() {
	color = readColor();
	if (cb) {
		cb(color);
	}

	return false;
}

RGB_u ColorPicker::readColor() const {
	auto s = input.getProperty("value");

	RGB_u newClr;

	// + 1 to skip '#'
	auto res = std::from_chars<u32>(s.data() + 1, s.data() + s.size(), newClr.rgb, 16);
	if (res.ec == std::errc::invalid_argument || res.ec == std::errc::result_out_of_range) {
		return {{0, 0, 0, 255}};
	}

	if (s.size() == 6 + 1) { // if format is #rrggbb, correct alpha value
		newClr.rgb <<= 8;
		newClr.rgb |= 0xFF;
	}

	newClr.rgb = bswap_32(newClr.rgb);

	return newClr;
}
