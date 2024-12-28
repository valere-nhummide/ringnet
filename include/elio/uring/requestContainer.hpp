#pragma once

#include <functional>
#include <memory>
#include <tuple>
#include <vector>

namespace elio::uring
{

template <typename... Requests>
class RequestContainer {
	std::tuple<std::vector<Requests *>...> requests{};

    public:
	template <class T>
	void push(T *request)
	{
		std::get<std::vector<T *>>(requests).push_back(request);
	}

	void clear()
	{
		std::apply([](auto &&...vector) { ((vector.clear()), ...); }, requests);
	}

	template <class UnaryFunc>
	void for_each(UnaryFunc &&function)
	{
		std::apply(
			[&function](auto &&...vector) {
				((std::for_each(vector.begin(), vector.end(), function)), ...);
			},
			requests);
	}
};

} // namespace elio::uring