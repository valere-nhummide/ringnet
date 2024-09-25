#pragma once

#include <string.h>

template <auto StrError = ::strerror, bool USE_ERRNO = false>
struct Status {
	static constexpr int SUCCESS = 0;
	explicit Status() : return_code(SUCCESS) {};

	explicit Status(int return_code_) : return_code(USE_ERRNO ? errno : return_code_) {};

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
