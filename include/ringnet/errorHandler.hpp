#pragma once

#include <iostream>

#include "ringnet/eventHandler.hpp"
#include "ringnet/events.hpp"

namespace ringnet
{

/// @brief Default error type: a thin wrapper around a std::string_view, representing an error message.
/// It has a what() method that returns a string-like object, and a constructor that takes a std::string_view.
/// It also has a constructor that takes an error event.
class Error {
    public:
	explicit Error(std::string_view message_) : message(message_){};

	template <class ErrorEvent>
	explicit Error(ErrorEvent error_event) : message(error_event.what()){};

	std::string_view what() const
	{
		return message;
	}

	std::string_view message;
};

/// @brief A class to handle errors:
/// - set the appropriate callback to call when an error occurs
/// - calls the callback on error (using an event handler)
/// @note The default constructor initializes the callback to print the error message to std::cerr.
class ErrorHandler {
    public:
	/// @brief The default constructor initializes the callback to print the error message to std::cerr.
	ErrorHandler()
	{
		onError([](Error error) { std::cerr << error.what(); });
	}

	/// @brief Handle an error event -> calls the current callback.
	/// @param error Error event.
	void handle(Error &&error)
	{
		handler.handle(std::move(error));
	}

	/// @brief Override to directly take an error message instead of an error object. The object is constructed from
	/// the message.
	/// @param message Error message.
	void handle(std::string_view message)
	{
		handler.handle(Error(message));
	}

	/// @brief Set a new callback to handle the error event, replacing the current one.
	/// @tparam Func The type of the callback.
	/// @param callback The callback to handle the error event. It should take an ErrorEvent and return void.
	/// @note Use concepts to ensure the callback is a valid function object.
	template <class Func>
	void onError(Func &&callback)
	{
		handler.on<Error>(std::move(callback));
	}

    private:
	EventHandler<Error> handler{};
};

} // namespace ringnet