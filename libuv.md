
# How does uvw associate a stream handle to an event loop

# How does libuv call the callback associated to an event ?

A read callback has the following type:
```cpp
void read_callback(uv_stream_t *, ssize_t, const uv_buf_t *)
```
An `uv_stream_t` object type has several members, including:
* `void* data` : user specific data. `uvw` uses this to hold the handle (e.g. `tcp_handle`)
* `uv_read_cb read_cb`: the callback to call
* `uv_alloc_cb alloc_cb` : the allocation callback

Basically, for each active file descriptor, the libuv event loop calls `uv__read`:
1. Allocates the reception buffer using the allocation callback:
```cpp
buf = uv_buf_init(NULL, 0);
    stream->alloc_cb((uv_handle_t*)stream, 64 * 1024, &buf);
```
2. Reads the socket
```cpp
nread = read(uv__stream_fd(stream), buf.base, buf.len);
```
3. Calls the associated read callback
```cpp
stream->read_cb(stream, nread, &buf);
```

Hence the proper callback is called. It is provided the `stream` (a pointer to `uv_stream_t`), so the client code can access its custom data and call other callbacks if needed.  

Hence everything is performed on the main event loop's thread.

# Adapt to io_uring

Using io_uring, then we cannot poll per socket. Instead, each completion should indicate what socket it is associated to. WE can store it in a `CompletionEvent`, that holds the file descriptor, alongside the different required callbacks. 

But then, we cannot use the persistent memory allocated alongside a socket (for `uvw`, this is the `tcp_handle`) to ensure the lifetime of the `CompletionEvent` outlives the asynchronous nature of io_uring. Unless the `CompletionEvent` is part of the " 