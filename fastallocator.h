#pragma once

#include <vector>
#include <iostream>

/*
 *
 *      FixedAllocator
 *
 *      Этот аллокатор только аллоцирует память. То есть при деаллокации он
 * ничего не делает Сначала мы аллоцируем памяти на 32 блока размера chunkSize
 *      потом на 64
 *      потом на 128 и тд
 *      при этом просто отдаем память, а когда нам ее возвращают - ничего не
 * делаем Удалим эту память, когда удалится FixedAllocator :)
 *
 *      Также FixedAllocator - синглтон
 *      Это такой паттерн проектирования, чтобы такой объект создавался один на
 * всю прогу
 *
 */

template <size_t chunkSize>
struct FixedAllocator {
private:
    size_t capacity_ = 32;
    size_t size_ = 0;

    std::vector<void*> chunks_;
    std::vector<void*> returned_;

    void allocate_memory_();

    static FixedAllocator<chunkSize> *allocator_;

    FixedAllocator();

public:
    static FixedAllocator<chunkSize> *getFixedAllocator();

    ~FixedAllocator();

    void *allocate();
    void deallocate(void* ptr);
};

template <size_t chunkSize>
FixedAllocator<chunkSize> *FixedAllocator<chunkSize>::allocator_ = nullptr;

template <size_t chunkSize>
FixedAllocator<chunkSize>::FixedAllocator() {
    void *chunk = ::operator new(capacity_ * chunkSize);
    chunks_.push_back(chunk);
}

/*
 *  Аллоцирование новой памяти
 */
template <size_t chunkSize>
void FixedAllocator<chunkSize>::allocate_memory_() {
    capacity_ *= 2;
    void *new_chunk = ::operator new(capacity_ * chunkSize);
    chunks_.push_back(new_chunk);
    size_ = 0;
}

/*
 *  Часть синглтона. Только через него можно будет обращаться к аллокатору
 */
template <size_t chunkSize>
FixedAllocator<chunkSize> *FixedAllocator<chunkSize>::getFixedAllocator() {
    if (allocator_ == nullptr) {
        allocator_ = new FixedAllocator<chunkSize>();
    }
    return allocator_;
}

/*
 *  Посмотрим есть ли у нас еще свободные блоки, для того, чтобы отдать
 *  Если нет, то сначала аллоцируем новый блок размеров в два раза больше
 * предыдущего
 *
 *  Отдадим память
 */
template <size_t chunkSize>
void *FixedAllocator<chunkSize>::allocate() {
    if (!returned_.empty()) {
        void* memory = returned_.back();
        returned_.pop_back();
        return memory;
    }

    if (size_ == capacity_) {
        allocate_memory_();
    }

    void *memory = reinterpret_cast<char *>(chunks_.back()) + size_ * chunkSize;
    size_++;

    return memory;
}

/*
 *  Ничего не делаем
 */
template <size_t chunkSize>
void FixedAllocator<chunkSize>::deallocate(void* ptr) {
    returned_.push_back(ptr);
}

/*
 *  Просто пройдемся и удалим все блоки памяти, которые мы аллоцировали
 */
template <size_t chunkSize>
FixedAllocator<chunkSize>::~FixedAllocator() {
    for (size_t i = 0; i < chunks_.size(); i++) {
        ::operator delete(chunks_[i]);
    }
}

/*
 *
 *      FastAllocator
 *
 *      FastAllocator - уже полноценный аллокатор.
 *      - Если мы пытаемся выделить
 *      небольшой кусок памяти (до maxSize), то используется FixedAllocator
 *      - Иначе обычный ::operator new()
 */

template <typename T>
struct FastAllocator {
private:
    static const size_t maxSize = 32;

public:
    FastAllocator() = default;
    template <typename U>
    FastAllocator(const FastAllocator<U>);

    T *allocate(size_t);
    void deallocate(T *, size_t);

    using value_type = T;
    using ptr = T *;
    using const_pointer = const T *;
    using reference = T &;
    using const_reference = const T &;

    template <typename U>
    struct rebind;
};

template <typename T>
template <typename U>
FastAllocator<T>::FastAllocator(const FastAllocator<U>) {}

template <typename T>
T *FastAllocator<T>::allocate(size_t n) {
    if (sizeof(T) <= maxSize && n <= 1) {
        return reinterpret_cast<T *>(
            FixedAllocator<sizeof(T)>::getFixedAllocator()->allocate());
    } else {
        return reinterpret_cast<T *>(::operator new(n * sizeof(T)));
    }
}

template <typename T>
void FastAllocator<T>::deallocate(T *point, size_t n) {
    if (sizeof(T) <= maxSize && n <= 1) {
        FixedAllocator<sizeof(T)>::getFixedAllocator()->deallocate(point);
    } else {
        ::operator delete(point);
    }
}

template <typename T>
template <typename U>
struct FastAllocator<T>::rebind {
    typedef FastAllocator<U> other;
};

/*
 *
 *      List<T, Allocator>
 *
 *      Написан обычный лист на указателях
 *      Только с указанием, что используем кастомный аллокатор
 *
 *
 */

template <typename T, typename Allocator = std::allocator<T> >
struct List {
public:
    explicit List(const Allocator &alloc = Allocator());
    List(size_t count, const T &value,
        const Allocator &alloc = Allocator());
    List(size_t count);
    List(const List &rhs);
    List &operator=(const List &rhs);
    ~List();

    size_t size() const;
    void pop_front();
    void pop_back();

    void push_front(const T &value);
    void push_back(const T &value);

    Allocator& get_allocator();

    template <typename U>
    class list_iterator;

    typedef list_iterator<T> iterator;
    typedef list_iterator<T const> const_iterator;
    typedef std::reverse_iterator<list_iterator<T>> reverse_iterator;
    typedef std::reverse_iterator<list_iterator<T const>> const_reverse_iterator;

    iterator begin() const;
    const_iterator cbegin() const;
    iterator end() const;
    const_iterator cend() const;

    reverse_iterator rbegin() const;
    const_reverse_iterator crbegin() const;
    reverse_iterator rend() const;
    const_reverse_iterator crend() const;

    iterator insert(const_iterator, const T&);
    iterator erase(const_iterator);


private:
    struct Node {
        T elem_;
        Node *next;
        Node *prev;
        Node() { next = prev = nullptr; }
        Node(const T &value) : elem_(value) { next = prev = nullptr; }
    };

    void insert_before_(Node *, const T &value);

    void emplace_before_(Node*);

    void erase_(Node*);

    void copy_(const List<T, Allocator> &);

    using node_allocator_type_ =
        typename std::allocator_traits<Allocator>::template rebind_alloc<Node>;
    using node_allocator_traits_ = std::allocator_traits<node_allocator_type_>;

    Allocator allocator_;
    node_allocator_type_ node_allocator_;
    size_t size_ = 0;
    Node *begin_ = nullptr;
    Node *end_ = nullptr;
};

template <typename T, typename Allocator>
List<T, Allocator>::List(const Allocator &alloc) : allocator_(std::allocator_traits<Allocator>::select_on_container_copy_construction(alloc)) {
    Node* begin = node_allocator_traits_::allocate(node_allocator_, 1);
    Node* end = node_allocator_traits_::allocate(node_allocator_, 1);

    begin->next = end;
    begin->prev = nullptr;
    end->prev = begin;
    end->next = nullptr;

    begin_ = begin;
    end_ = end;
}

template <typename T, typename Allocator>
List<T, Allocator> &List<T, Allocator>::operator=(
    const List<T, Allocator> &rhs) {
    if (this == &rhs) {
        return *this;
    }

    while (size_ > 0) {
        pop_back();
    }
    
    if (std::allocator_traits<Allocator>::propagate_on_container_copy_assignment::value) {
        allocator_ = rhs.allocator_;
    }

    copy_(rhs);

    return *this;
}

template <typename T, typename Allocator>
List<T, Allocator>::List(const List<T, Allocator> &rhs) : List(rhs.allocator_) {
    copy_(rhs);
}

template <typename T, typename Allocator>
List<T, Allocator>::List(size_t count, const T &value, const Allocator &alloc)
        : List(alloc) {
    
    for (size_t i = 0; i < count; i++) {
        push_back(value);
    }
}

template <typename T, typename Allocator>
List<T, Allocator>::List(size_t count)
        : List(Allocator()) {
    
    for (size_t i = 0; i < count; i++) {
        emplace_before_(end_);
    }
}

template <typename T, typename Allocator>
List<T, Allocator>::~List() {
    while (size_ > 0) {
        pop_back();
    }

    node_allocator_traits_::deallocate(node_allocator_, begin_, 1);
    node_allocator_traits_::deallocate(node_allocator_, end_, 1);
}

template <typename T, typename Allocator>
void List<T, Allocator>::copy_(const List<T, Allocator> &rhs) {
    for (auto it = rhs.cbegin(); it != rhs.cend(); it++) {
        push_back(*it);
    }
}

template <typename T, typename Allocator>
size_t List<T, Allocator>::size() const {
    return size_;
}

template <typename T, typename Allocator>
void List<T, Allocator>::pop_front() {
    if (size_) {
        erase(begin());
    }
}

template <typename T, typename Allocator>
void List<T, Allocator>::pop_back() {
    if (size_) {
        erase(--end());
    }
}

template <typename T, typename Allocator>
void List<T, Allocator>::push_front(const T &value) {
    insert(begin(), value);
}

template <typename T, typename Allocator>
void List<T, Allocator>::push_back(const T &value) {
    insert(end(), value);
}

template <typename T, typename Allocator>
void List<T, Allocator>::erase_(Node *ptr) {
    if (ptr->next) {
        ptr->next->prev = ptr->prev;
    } else {
        end_ = ptr->prev;
    }

    if (ptr->prev) {
        ptr->prev->next = ptr->next;
    } else {
        begin_ = ptr->next;
    }

    node_allocator_traits_::destroy(node_allocator_, ptr);
    node_allocator_traits_::deallocate(node_allocator_, ptr, 1);

    --size_;
}

template <typename T, typename Allocator>
void List<T, Allocator>::insert_before_(Node *ptr, const T &value) {
    Node *newbie = node_allocator_traits_::allocate(node_allocator_, 1);
    node_allocator_traits_::construct(node_allocator_, newbie, value);

    newbie->next = ptr;
    newbie->prev = ptr->prev;

    if (ptr->prev) {
        ptr->prev->next = newbie;
    } else {
        begin_ = newbie;
    }

    ptr->prev = newbie;
    ++size_;
}

template <typename T, typename Allocator>
void List<T, Allocator>::emplace_before_(Node *ptr) {
    Node *newbie = node_allocator_traits_::allocate(node_allocator_, 1);
    node_allocator_traits_::construct(node_allocator_, newbie);

    newbie->next = ptr;
    newbie->prev = ptr->prev;

    if (ptr->prev) {
        ptr->prev->next = newbie;
    } else {
        begin_ = newbie;
    }

    ptr->prev = newbie;
    ++size_;
}

template <typename T, typename Allocator>
Allocator& List<T, Allocator>::get_allocator() {
    return allocator_;
}

template <typename T, typename Allocator>
template <typename U>
class List<T, Allocator>::list_iterator {
public:
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = U;
    using pointer = U*;
    using reference = U&;

    list_iterator();
    list_iterator(const list_iterator<typename std::remove_const<T>::type>& rhs);

    U& operator*();
    U* operator->();

    list_iterator& operator++();
    list_iterator operator++(int);
    list_iterator& operator--();
    list_iterator operator--(int);

    bool operator==(const list_iterator<U>& rhs);
    bool operator!=(const list_iterator<U>& rhs);
    friend class List<T, Allocator>;

private:
    list_iterator(Node* ptr);

    Node* ptr_;
};

template <typename T, typename Allocator>
template <typename U>
List<T, Allocator>::list_iterator<U>::list_iterator(Node* ptr) : ptr_(ptr) {}


template <typename T, typename Allocator>
template <typename U>
List<T, Allocator>::list_iterator<U>::list_iterator(const list_iterator<typename std::remove_const<T>::type>& rhs) : ptr_(rhs.ptr_) {}


template <typename T, typename Allocator>
template <typename U>
U& List<T, Allocator>::list_iterator<U>::operator*() {
    return ptr_->elem_;
}

template <typename T, typename Allocator>
template <typename U>
U* List<T, Allocator>::list_iterator<U>::operator->() {
    return &(ptr_->elem);
}

template <typename T, typename Allocator>
template <typename U>
typename List<T, Allocator>::template list_iterator<U>& List<T, Allocator>::list_iterator<U>::operator++() {
    ptr_ = ptr_->next;
    return *this;
}

template <typename T, typename Allocator>
template <typename U>
typename List<T, Allocator>::template list_iterator<U> List<T, Allocator>::list_iterator<U>::operator++(int) {
    list_iterator other = *this;
    ++(*this);
    return other;
}

template <typename T, typename Allocator>
template <typename U>
typename List<T, Allocator>::template list_iterator<U>& List<T, Allocator>::list_iterator<U>::operator--() {
    ptr_ = ptr_->prev;
    return *this;
}

template <typename T, typename Allocator>
template <typename U>
typename List<T, Allocator>::template list_iterator<U> List<T, Allocator>::list_iterator<U>::operator--(int) {
    list_iterator other = *this;
    --(*this);
    return other;
}


template <typename T, typename Allocator>
template <typename U>
bool List<T, Allocator>::list_iterator<U>::operator==(const typename List<T, Allocator>::template list_iterator<U>& rhs) {
    return ptr_ == rhs.ptr_;
}

template <typename T, typename Allocator>
template <typename U>
bool List<T, Allocator>::list_iterator<U>::operator!=(const typename List<T, Allocator>::template list_iterator<U>& rhs) {
    return ptr_ != rhs.ptr_;
}

template <typename T, typename Allocator>
typename List<T, Allocator>::iterator List<T, Allocator>::begin() const {
    return iterator(begin_->next);
}
template <typename T, typename Allocator>
typename List<T, Allocator>::const_iterator List<T, Allocator>::cbegin() const {
    return const_iterator(begin_->next);
}

template <typename T, typename Allocator>
typename List<T, Allocator>::iterator List<T, Allocator>::end() const {
    return iterator(end_);
}
template <typename T, typename Allocator>
typename List<T, Allocator>::const_iterator List<T, Allocator>::cend() const {
    return const_iterator(end_);
}

template <typename T, typename Allocator>
typename List<T, Allocator>::reverse_iterator List<T, Allocator>::rbegin() const {
    return reverse_iterator(end_);
}
template <typename T, typename Allocator>
typename List<T, Allocator>::const_reverse_iterator List<T, Allocator>::crbegin() const {
    return const_reverse_iterator(end_);
}

template <typename T, typename Allocator>
typename List<T, Allocator>::reverse_iterator List<T, Allocator>::rend() const {
    return reverse_iterator(begin_->next);
}

template <typename T, typename Allocator>
typename List<T, Allocator>::const_reverse_iterator List<T, Allocator>::crend() const {
    return const_reverse_iterator(begin_->next);
}

template <typename T, typename Allocator>
typename List<T, Allocator>::iterator List<T, Allocator>::insert(const_iterator iter, const T& value) {
    insert_before_(iter.ptr_, value);
    return iterator(iter.ptr_->next);
}

template <typename T, typename Allocator>
typename List<T, Allocator>::iterator List<T, Allocator>::erase(const_iterator iter) {
    iterator ret(iter.ptr_->next);
    erase_(iter.ptr_);
    return ret;
}
