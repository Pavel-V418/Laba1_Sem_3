#pragma once

#include "i_enumerable.h"

// Базовый интерфейс коллекции (контейнера элементов)
template<class T>
class ICollection : public IEnumerable<T> {
public:
    virtual ~ICollection() = default;

    virtual int get_count() const = 0;
    virtual bool is_empty() const = 0;
    virtual bool contains(const T& item) const = 0;
    virtual void clear() = 0;
};
