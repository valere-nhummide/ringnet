#pragma once

#include <functional>
#include <tuple>
#include <vector>

namespace elio::uring
{

template <typename... Ts>
class RefContainer {
	template <class T>
	using References = std::vector<std::reference_wrapper<const T>>;

	std::tuple<References<Ts>...> references{};

    public:
	template <class T>
	void push(const T &item)
	{
		std::get<References<T>>(references).push_back(std::cref(item));
	}

	void clear()
	{
		std::apply([](auto &&...elems) { ((elems.clear()), ...); }, references);
	}

	template <class UnaryFunc>
	void for_each(UnaryFunc &&function)
	{
		std::apply([this,
			    &function](auto &&...vecs) { ((std::for_each(vecs.begin(), vecs.end(), function)), ...); },
			   references);
	}
};

} // namespace elio::uring