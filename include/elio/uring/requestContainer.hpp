#pragma once

#include <functional>
#include <memory>
#include <tuple>
#include <vector>

namespace elio::uring
{

template <typename... Requests>
class RequestContainer {
	template <typename T>
	using Vector = std::vector<std::weak_ptr<T>>;

	std::tuple<Vector<Requests>...> requests{};

    public:
	template <class T>
	void push(const std::shared_ptr<T> &item)
	{
		std::weak_ptr weak = item;
		std::get<Vector<T>>(requests).push_back(weak);
	}

	void clear()
	{
		std::apply([](auto &&...elems) { ((elems.clear()), ...); }, requests);
	}

	void cleanup()
	{
		std::apply(
			[](auto &&...vector) {
				((std::erase_if(vector, [](auto request) { return request.expired(); })), ...);
			},
			requests);
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