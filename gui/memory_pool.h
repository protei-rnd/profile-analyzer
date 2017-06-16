#pragma once

#include <list>
#include <vector>
#include <cassert>
#include <cmath>

template<typename T, size_t chunk_size = 128>
class memory_pool {
public:
    memory_pool(size_t cnt_elements = chunk_size)
        : _sz(ceil(cnt_elements * 1. / chunk_size))
        , _pool(_sz)
        , _pos_in_chunk(0)
        , _cur_chunk(_pool.begin())
    {}

    T* next_element() {
        if (_pos_in_chunk == chunk_size) {
            if (_sz == _pool.size()) {
                _pool.emplace_back();
            }
            _sz++;

            _pos_in_chunk = 0;
            _cur_chunk++;
        }

        assert(_pos_in_chunk < chunk_size);
        T* res = &((*_cur_chunk)[_pos_in_chunk]);
        _pos_in_chunk++;

        return res;
    }

    size_t cnt_pools() const {
        return _sz;
    }

    void clear() {
        _sz = 1;
        _pos_in_chunk = 0;
        _cur_chunk = _pool.begin();
    }

    size_t size() const {
        return _pool.size() * chunk_size;
    }

private:
    typedef std::list<std::array<T, chunk_size>> pool_t;

private:
    size_t _sz;
    pool_t _pool;
    size_t _pos_in_chunk;
    typename pool_t::iterator _cur_chunk;
};
