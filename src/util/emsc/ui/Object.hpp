#pragma once

#include <util/NonCopyable.hpp>
#include <string>
#include <string_view>
#include <cstdint>
#include <functional>

#include <util/emsc/ui/EventHandle.hpp>

namespace eui {

class Object : public NonCopyable {
	std::uint32_t id;

public:
	Object(); // creates a div by default
	Object(std::string_view tag);

	Object(Object&&);
	Object& operator=(Object&&);

	~Object();

	std::uint32_t getId() const;
	std::string_view getSelector() const;
	void getOffsetSize(int * ow, int * oh) const;

	void addClass(std::string_view);
	bool tglClass(std::string_view);
	void delClass(std::string_view);

	std::string getProperty(std::string_view name) const;
	void setProperty(std::string_view name);
	void setProperty(std::string_view name, std::string_view value);
	void setPropertyBool(std::string_view name, bool value);
	std::string getAttribute(std::string_view name) const;
	void setAttribute(std::string_view name);
	void setAttribute(std::string_view name, std::string_view value);
	EventHandle createHandler(std::string_view name, std::function<bool(void)> cb, bool passive = true);
	/* "window" binds event to the js window object */
	static EventHandle createWindowHandler(std::string_view name, std::function<bool(void)> cb, bool passive = true);

	void appendTo(std::string_view selector);
	void appendTo(std::uint32_t id);
	void appendTo(const Object&);
	void appendToMainContainer();

	void remove();

private:
	void destroy();
};

} // namespace eui
