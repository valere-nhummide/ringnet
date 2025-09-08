#pragma once

#include <memory_resource>

namespace ringnet::uring
{

class RequestPool {
	std::pmr::unsynchronized_pool_resource resource{};

    public:
	template <class Request>
	Request *allocate()
	{
		return static_cast<Request *>(resource.allocate(sizeof(Request), alignof(Request)));
	}

	template <class Request>
	void deallocate(Request *ptr)
	{
		resource.deallocate(static_cast<void *>(ptr), sizeof(Request), alignof(Request));
	}
};

} // namespace ringnet::uring