#ifndef VECTOR_VECTOR_H
#define VECTOR_VECTOR_H

#include <string>
#include <variant>

template<typename T>
struct dynamic {
    struct info {
        size_t capacity, count;
        T elems[];
    };

    void data_alloc(size_t len) {
        data = static_cast<info *>(operator new(sizeof(info) + len * sizeof(T)));
        data->count = 0;
        data->capacity = len;
    }

    void refs_inc() {
        data->count++;
    }

    void refs_dec(size_t size_) {
        data->count--;
        if (data->count == 0) {
            for (size_t i = 0; i < size_; ++i)
                data->elems[i].~T();
            operator delete(data);
        }
    }

    info *data;
};

template<typename T>
struct vector {
    const static size_t SIZE = 1;

    template<typename U>
    struct ra_vector_iterator {
        typedef std::random_access_iterator_tag iterator_category;
        typedef U value_type;
        typedef ptrdiff_t difference_type;
        typedef U *pointer;
        typedef U &reference;

        friend struct vector;

        ra_vector_iterator() : p(nullptr) {};

        ra_vector_iterator(pointer p_) : p(p_) {};

        template<typename V>
        ra_vector_iterator(ra_vector_iterator<V> const &other,
                           typename std::enable_if<std::is_same<V const, V>::value &&
                                                   std::is_const<V>::value>::type * = nullptr) {
            p = other.p;
        }

        ra_vector_iterator &operator++() {
            p++;
            return *this;
        }

        const ra_vector_iterator operator++(int) {
            auto res = *this;
            ++(*this);
            return res;
        }

        ra_vector_iterator &operator--() {
            p--;
            return *this;
        }

        const ra_vector_iterator operator--(int) {
            auto res = *this;
            --(*this);
            return res;
        }

        reference operator*() {
            return *p;
        }

        pointer operator->() {
            return p;
        }

        ra_vector_iterator &operator+=(difference_type diff) {
            p += diff;
            return *this;
        }

        ra_vector_iterator &operator-=(difference_type diff) {
            p -= diff;
            return *this;
        }

        ra_vector_iterator &operator=(ra_vector_iterator const &other) = default;

        reference operator[](difference_type diff) const {
            return *ra_vector_iterator(*this + diff);
        }

        friend ra_vector_iterator operator+(ra_vector_iterator it, difference_type diff) {
            return it += diff;
        }

        friend ra_vector_iterator operator-(ra_vector_iterator it, difference_type diff) {
            return it -= diff;
        }

        friend difference_type operator-(ra_vector_iterator const &a, ra_vector_iterator const &b) {
            return difference_type(a.p - b.p);
        }

        friend bool operator<(ra_vector_iterator const &a, ra_vector_iterator const &b) {
            return a.p < b.p;
        }

        friend bool operator<=(ra_vector_iterator const &a, ra_vector_iterator const &b) {
            return a.p <= b.p;
        }

        friend bool operator>(ra_vector_iterator const &a, ra_vector_iterator const &b) {
            return a.p > b.p;
        }

        friend bool operator>=(ra_vector_iterator const &a, ra_vector_iterator const &b) {
            return a.p >= b.p;
        }

        friend bool operator==(ra_vector_iterator const &a, ra_vector_iterator const &b) {
            return a.p == b.p;
        }

        friend bool operator!=(ra_vector_iterator const &a, ra_vector_iterator const &b) {
            return !(a == b);
        }

    private:
        pointer p;
    };

    typedef T value_type;

    typedef ra_vector_iterator<T> iterator;
    typedef ra_vector_iterator<T const> const_iterator;
    typedef std::reverse_iterator<iterator> reverse_iterator;
    typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

    vector() noexcept {
        size_ = 0;
    }

    vector(vector const &other) {
        size_ = other.size();
        data_ = other.data_;
        if (!small())
            std::get<dynamic<T>>(data_).refs_inc();
    }

    vector &operator=(vector const &other) {
        vector<T> safe(*this);
        if (&other == this)
            return *this;
        if (!small()) {
            std::get<dynamic<T>>(data_).refs_dec(size_);
        }

        size_ = other.size();
        data_ = other.data_;
        if (!small())
            std::get<dynamic<T>>(data_).refs_inc();
        return *this;
    }

    ~vector() {
        if (!small())
            std::get<dynamic<T>>(data_).refs_dec(size_);
    }

    T const &operator[](size_t i) const {
        if (data_.index() == 1) {
            return std::get<1>(data_);
        }
        return std::get<2>(data_).data->elems[i];
    }

    T &operator[](size_t i) {
        if (data_.index() == 1) {
            return std::get<1>(data_);
        }
        copy_on_write();
        return std::get<2>(data_).data->elems[i];
    }

    size_t capacity() const {
        return small() ? 1 : std::get<dynamic<T>>(data_).data->capacity;
    }

    T *data() {
        if (std::holds_alternative<std::monostate>(data_))
            return nullptr;
        if (!small()) {
            return std::get<dynamic<T>>(data_).data->elems;
        } else {
            return &std::get<T>(data_);
        }
    }

    T const *data() const {
        if (std::holds_alternative<std::monostate>(data_))
            return nullptr;
        if (!small()) {
            return std::get<dynamic<T>>(data_).data->elems;
        } else {
            return &std::get<T>(data_);
        }
    }

    size_t size() const {
        return size_;
    }

    bool empty() const {
        return size_ == 0;
    }

    void resize(size_t nsize) {
        vector <T> buf(*this);
        buf.copy_on_write();
        if (nsize > buf.capacity()) {
            buf.reserve(2 * nsize);
        } else if (nsize < buf.size_ && !buf.small()) {
            for (size_t i = nsize; i < buf.size_; ++i)
                std::get<dynamic<T>>(buf.data_).data->elems[i].~T();
        }
        buf.size_ = nsize;

        *this = buf;
    }

    void resize(size_t nsize, T const& value) {
        if (nsize <= size_) {
            resize(nsize);
        } else {
            vector <T> buf(*this);
            buf.copy_on_write();
            while (buf.size_ < nsize)
                buf.push_back(value);
            *this = buf;
        }

    }

    void push_back(T const &value) {
        vector<T> buf(*this);
        T val = value;
        if (buf.size_ + 1 > SIZE) {
            if (buf.size_ >= buf.capacity()) {
                buf.copy_on_write();
                buf.reserve(2 * buf.capacity());
            }
        }
        if (buf.small()) {
            try {
                buf.data_ = T(val);
            } catch (...) {
                // std::cerr<< "pb small\n";
                throw;
            }
        } else {
            try {
                new(std::get<dynamic<T>>(buf.data_).data->elems + size_) T(val);
            } catch (...) {
                // std::cerr << "pb big\n";
                throw;
            }
        }
        buf.size_++;
        *this = buf;
    }

    void pop_back() {
        vector <T> buf(*this);
        buf.copy_on_write();
        buf.size_--;
        if (!buf.small()) {
            std::get<dynamic<T>>(buf.data_).data->elems[buf.size_].~T();
        }

        *this = buf;
    }

    T &front() {
        return *begin();
    }

    T &back() {
        return *(end() - 1);
    }

    T const &front() const {
        return *begin();
    }

    T const &back() const {
        return *(end() - 1);
    }

    void shrink_to_fit() {
        vector <T> buf(*this);
        reserve(size_);
        *this = buf;
    }

    void clear() {
        vector<T> buf(*this);
        buf.resize(0);
        *this = buf;
    }

    void reserve(size_t nlen) {
        vector<T> buf(*this);

        buf.copy_on_write();
        if (nlen == buf.capacity()) {
            return;
        }

        if (std::holds_alternative<std::monostate>(buf.data_)) {
            if (nlen > SIZE) {
                dynamic<T> ndata;
                ndata.data_alloc(nlen);
                ndata.refs_inc();
                buf.data_ = ndata;
            }

            *this = buf;
            return;
        }

        size_t nsize = std::min(buf.size(), nlen);

        T *cur_data = buf.data();
        if (nlen <= SIZE) {
            T ndata(cur_data[0]);
            if (!buf.small()) {
                std::get<dynamic<T>>(buf.data_).refs_dec(buf.size_);
            }
            buf.data_ = T(ndata);
        } else {
            dynamic<T> ndata;
            ndata.data_alloc(nlen);
            ndata.refs_inc();
            for (size_t i = 0; i < nsize; ++i) {
                try {
                    new(ndata.data->elems + i) T(cur_data[i]);
                } catch (...) {
                    // std::cerr << "reserve\n";
                    for (size_t j = 0; j < i; ++j)
                        ndata.data->elems[j].~T();
                    operator delete(ndata.data);
                    throw;
                }
            }
            if (!buf.small()) {
                std::get<dynamic<T>>(buf.data_).refs_dec(buf.size_);
            }
            buf.data_ = ndata;
        }

        buf.size_ = nsize;
        *this = buf;
    }

    template<typename U>
    friend void swap(vector<U> &a, vector<U> &b);

    template<typename InputIterator>
    vector(InputIterator first, InputIterator last) {
        vector <T> buf;
        size_ = 0;
        for (; first != last; first++)
            buf.push_back(*first);
        *this = buf;
    }

    template<typename InputIterator>
    void assign(InputIterator first, InputIterator last) {
        vector <T> buf(first, last);
        swap(buf, *this);
    }

    iterator begin() {
        return data();
    }

    iterator end() {
        return data() + size_;
    }

    reverse_iterator rbegin() {
        return reverse_iterator(data() + size_);
    }

    reverse_iterator rend() {
        return reverse_iterator(data());
    }

    const_iterator begin() const {
        return data();
    }

    const_iterator end() const {
        return data() + size_;
    }

    const_reverse_iterator rbegin() const {
        return const_reverse_iterator(data() + size_);
    }

    const_reverse_iterator rend() const {
        return const_reverse_iterator(data());
    }

    iterator insert(iterator pos, T const &value) {
        vector<T> buf;
        iterator r = begin();
        while (r != pos) {
            buf.push_back(*r);
            ++r;
        }
        size_t offset = buf.size();
        buf.push_back(value);
        while (r != end()) {
            buf.push_back(*r);
            ++r;
        }
        *this = buf;
        return begin() + offset;
    }

    iterator erase(iterator pos) { return erase(pos, pos + 1); }

    iterator erase(iterator first, iterator last) {
        vector <T> buf;
        iterator t = begin();
        while (t != first) {
            buf.push_back(*t);
            t++;
        }
        while (t != last)
            t++;
        size_t res = buf.size();
        while (t != end()) {
            buf.push_back(*t);
            t++;
        }
        *this = buf;
        return begin() + res;
    }


    template<typename U>
    friend bool operator==(vector<T> const &a, vector<T> const &b);

    template<typename U>
    friend bool operator<(vector<T> const &a, vector<T> const &b);

    template<typename U>
    friend bool operator>(vector<T> const &a, vector<T> const &b);

    template<typename U>
    friend bool operator!=(vector<T> const &a, vector<T> const &b);

    template<typename U>
    friend bool operator<=(vector<T> const &a, vector<T> const &b);

    template<typename U>
    friend bool operator>=(vector<T> const &a, vector<T> const &b);

private:
    size_t size_;
    std::variant<std::monostate, T, dynamic<T>> data_;

    bool small() const {
        return !std::holds_alternative<dynamic<T>>(data_);
    }

    size_t count() const {
        return std::get<dynamic<T>>(data_).data->count;
    }

    void copy_on_write() {
        if (small() || count() <= 1)
            return;

        T *cur_data = data();
        dynamic<T> ndata;
        ndata.data_alloc(capacity());

        for (size_t i = 0; i < size_; ++i) {
            try {
                new(ndata.data->elems + i) T(cur_data[i]);
            } catch (...) {
                //std::cerr << "cow\n";
                for (size_t j = 0; j < i; ++j)
                    ndata.data->elems[j].~T();
                operator delete(ndata.data);
                throw;
            }
        }

        std::get<dynamic<T>>(data_).refs_dec(size_);
        ndata.refs_inc();
        data_ = ndata;
    }

    void swap_sm(vector<T> &other) {
        if (small() || !other.small()) {
            return;
        }
        T cur = other[0];
        other.pop_back();
        for (size_t i = 0; i < size(); ++i) {
            other.push_back((*this)[i]);
        }
        while (!empty())
            pop_back();
        push_back(cur);
    }
};


template<typename U>
bool operator==(vector<U> const &a, vector<U> const &b) {
    if (a.size() != b.size())
        return false;

    for (size_t i = 0; i < a.size(); ++i)
        if (a[i] != b[i])
            return false;

    return true;
}

template<typename U>
bool operator<(vector<U> const &a, vector<U> const &b) {
    for (size_t i = 0; i < std::min(a.size(), b.size()); ++i) {
        if (a[i] > b[i])
            return false;
        if (a[i] < b[i])
            return true;
    }

    return a.size() < b.size();
}

template<typename U>
bool operator>(vector<U> const &a, vector<U> const &b) {
    return !(a == b || a < b);
}

template<typename U>
bool operator!=(vector<U> const &a, vector<U> const &b) {
    return !(a == b);
}

template<typename U>
bool operator<=(vector<U> const &a, vector<U> const &b) {
    return !(a > b);
}

template<typename U>
bool operator>=(vector<U> const &a, vector<U> const &b) {
    return !(a < b);
}

template<typename U>
void swap(vector<U> &a, vector<U> &b) {
    if (a.data_.index() == 2 && b.data_.index() == 1)
        a.swap_sm(b);
    else if (b.data_.index() == 2 && a.data_.index() == 1)
        b.swap_sm(a);
    else {
        std::swap(a.data_, b.data_);
        std::swap(a.size_, b.size_);
    }
}


#endif //VECTOR_VECTOR_H
