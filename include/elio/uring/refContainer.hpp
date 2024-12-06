#pragma once

#include <functional>
#include <tuple>
#include <vector>

namespace elio::uring
{

template <typename... Ts>
class PointersTuple {
	template <typename T>
	using Pointers = std::vector<const T *>;

	std::tuple<Pointers<Ts>...> tuple{};

    public:
	template <class T>
	void push(const T *item)
	{
		std::get<Pointers<T>>(tuple).push_back(item);
	}

	void clear()
	{
		std::apply([](auto &&...elems) { ((elems.clear()), ...); }, tuple);
	}

	template <class UnaryFunc>
	void for_each(UnaryFunc &&function)
	{
		std::apply(
			[&function](auto &&...pointers) {
				((std::for_each(pointers.begin(), pointers.end(), function)), ...);
			},
			tuple);
	}
};

} // namespace elio::uring