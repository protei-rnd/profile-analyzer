#include <string>
#include <sstream>
#include <fstream>
#include <array>
#include <stdio.h>

#include <psc/process.h>

#include <QFileInfo>
#include <QDir>
#include <QDebug>

#include <db_reader.h>

namespace {
    typedef std::map<ptr_t, fn_desc> fn_descs_t;
    static fn_descs_t g_fn_desc;

    memory_pool<fn_desc_node> g_mem_pool;

    template<typename T>
    bool in_range(T const & point, T const & left, size_t length) {
        return left <= point && point < left + length;
    }

    fn_desc * get_descriptor(ptr_t rip) {
        fn_desc * res = nullptr;

        fn_descs_t::iterator where = g_fn_desc.lower_bound(rip);

        if (g_fn_desc.end() != where) {

            if (!in_range(rip, where->first, where->second._size)) {
                if (where != g_fn_desc.begin()) {
                    --where;
                }
            }

            if (in_range(rip, where->first, where->second._size)) {
                res = &(where->second);
            } else {
                std::cout << "not found (not in range): " << std::hex << (uint64_t) rip <<  ", " << std::dec  << "\n";
            }
        }  else {
            std::cout << "not found: " << std::hex << (uint64_t) rip << std::dec << "\n";
        }

        return res;
    }

    fn_desc_node * add_new_child(fn_desc_node * node_, std::string && name, ptr_t rip, uint64_t ts) {
        fn_desc_node * child = nullptr;
        fn_tree_node_t::iterator where = node_->_children.find(rip);
        if (node_->_children.end() == where) {

            child = g_mem_pool.next_element();
            *child = fn_desc_node();
            child->_size = 0;
            child->_name = std::move(name);
            child->_children.clear();

            node_->_children[rip] = child;
        } else {
            child = where->second;
        }
        ++child->_size;
        child->upd_ts(ts);

        return child;
    }

    template<typename IteratorT>
    bool add_children(int level, fn_desc_node * node_, IteratorT begin, IteratorT const end, uint64_t ts)
    {
        if (begin != end) {
            ptr_t rip = *begin;
            std::string name;

            if (0 == level) {
                name = "thread " + std::to_string(rip);
            } else {
                fn_desc * desc = get_descriptor(rip);

                if (desc) {
                    rip = desc->_start_addr;
                    name = desc->_name;
                } else {
                    std::stringstream ss;
                    ss << "unknown [0x" << std::hex << (uint64_t) rip << "]";

                    name = ss.str();
                }
            }

            fn_desc_node * child = add_new_child(node_, std::move(name), rip, ts);

            // if current child is a leaf in the path
            if (add_children(++level, child, ++begin, end, ts)) {
                child->_tss.push_back(ts);
            }

            return false;
        }

        return true;
    }

    void remove_unnecessary_children(fn_desc_node & node, size_t less_than = 3) {
        if (!node._children.empty()) {
            node._size = node._tss.size();
            node._lts = std::numeric_limits<uint64_t>::max();
            node._uts = 0;

            for (auto it = node._children.begin(); it != node._children.end();) {
                fn_desc_node & child_node = *it->second;

                remove_unnecessary_children(child_node);

                if (child_node._size < less_than) {
                    it = node._children.erase(it);
                } else {
                    ++it;

                    node._size += child_node._size;
                    node.upd_ts(child_node._lts);
                    node.upd_ts(child_node._uts);
                }
            }

            for (uint64_t ts: node._tss) {
                node.upd_ts(ts);
            }
        }

        node._clipped_size = node._size;
    }

    std::vector<char> read_whole_file(std::ifstream & data_stream) {
        data_stream.seekg(0, std::ios::end);
        int length = data_stream.tellg();
        data_stream.seekg(0, std::ios::beg);

        std::vector<char> buf(length);
        data_stream.read(buf.data(), length);

        return buf;
    }

    template<typename T>
    void parse_num(char const *& buf, T& num) {
        num = 0;

        for (; *buf != ' '; ++buf) {
            num *= 10;
            num += *buf - '0';
        }

        ++buf;
    }

    void parse_maps(uint8_t const *& first, uint8_t const * last) {
        fn_desc desc;
        char const * chr_first = reinterpret_cast<char const *>(first);
        char const * chr_last = reinterpret_cast<char const *>(last);

        while (true) {
            parse_num(chr_first, desc._start_addr);
            parse_num(chr_first, desc._size);

            char const * chr_prev = chr_first;
            while (chr_first < chr_last && *chr_first != '\n') {
                ++chr_first;
            }

            desc._name = std::string(chr_prev, chr_first);
            chr_first++;

            if (desc._size > 0) {
                g_fn_desc[desc._start_addr] = desc;
            }

            if (chr_first >= chr_last) {
                break;
            }
        }

        first = reinterpret_cast<uint8_t const *>(chr_first);

        if (!g_fn_desc.empty()) {
            auto it_prev = g_fn_desc.begin();
            auto it_end = g_fn_desc.end();

            for (auto it = std::next(it_prev); it != it_end; ++it) {
                it_prev->second._size = it->second._start_addr - it_prev->second._start_addr;
                it_prev = it;
            }
        }
    }

    void init_root(fn_desc_node *& r) {
        r = g_mem_pool.next_element();
        *r = fn_desc_node();
    }
} // anonymous namespace

fn_desc_node * g_cur_root = nullptr;
fn_desc_node * g_root = nullptr;
fn_desc_node * g_root_reversed = nullptr;

void read_db_file (char const * filename)
{
    g_fn_desc.clear();
    g_mem_pool.clear();
    init_root(g_root);
    init_root(g_root_reversed);

    g_cur_root = g_root;

    std::ifstream data_stream(filename);
    if (data_stream) {

        std::vector<char> buf = read_whole_file(data_stream);

        uint8_t const * first = (uint8_t*) buf.data();
        uint8_t const * last = first + buf.size();

        struct stack_important {
            std::vector<ptr_t> _stack;
            uint64_t _ts;
        };

        bool spliter_exist = false;
        std::vector<stack_important> stacks;
        for ( ; first < last; ) {
            thread_stack const * cur_thread_stack = reinterpret_cast<thread_stack const *>(first);

            if (cur_thread_stack->_tid != pid_t(-1)) {
                stack_important imp_stack;
                imp_stack._stack.insert(imp_stack._stack.end(), cur_thread_stack->_stack, cur_thread_stack->_stack + cur_thread_stack->_length);
                imp_stack._stack.push_back(cur_thread_stack->_tid);
                imp_stack._ts = cur_thread_stack->_ts;

                stacks.push_back(std::move(imp_stack));

                first += sizeof(thread_stack) + sizeof(char *) * cur_thread_stack->_length;
            } else {
                spliter_exist = true;
                first += sizeof(cur_thread_stack->_tid);

                if (first < last) {
                    parse_maps(first, last);

                    if (first < last) {
                        std::cerr << "not parsed: " << std::string(first, last) << std::endl;
                        break;
                    }
                }
            }
        }

        assert(spliter_exist);

        for (size_t i = 0; i < stacks.size(); ++i) {
            stack_important& imp = stacks[i];

            add_children(0, g_root, imp._stack.rbegin(), imp._stack.rend(), imp._ts);

            auto & rev_stack = imp._stack;
            rev_stack.insert(rev_stack.begin(), rev_stack.back());
            rev_stack.pop_back();
            add_children(0, g_root_reversed, rev_stack.begin(), rev_stack.end(), imp._ts);
        }

        remove_unnecessary_children(*g_root);
        remove_unnecessary_children(*g_root_reversed);
    }
}
