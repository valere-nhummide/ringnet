#pragma once

#include <string.h>
#include <string_view>

template <auto StrError = ::strerror, bool USE_ERRNO = false>
struct FileDescriptorStatus {
	static constexpr int SUCCESS = 0;
	explicit FileDescriptorStatus() : return_code(SUCCESS) {};

	explicit FileDescriptorStatus(int return_code_) : return_code(USE_ERRNO ? errno : return_code_) {};

	inline operator bool() const
	{
		return return_code == SUCCESS;
	}
	inline const char *what() const
	{
		return StrError(return_code);
	}
	inline int code() const
	{
		return return_code;
	}

    private:
	int return_code;
};

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