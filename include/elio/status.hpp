#pragma once

#include <string.h>
#include <string_view>

struct MessagedStatus {
	explicit MessagedStatus(bool success_, std::string_view message_) : success(success_), message(message_) {};
	inline operator bool() const
	{
		return success;
	}
	inline const char *what() const
	{
		return message.data();
	}

    private:
	bool success;
	std::string_view message;
};