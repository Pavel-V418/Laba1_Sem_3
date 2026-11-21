#pragma once

#include <cmath>

// Точный порядок чисел для составных ключей (для комплексных чисел и коэф. полинома). Все NaN эквивалентны и идут после чисел.
inline int compare_double(double a, double b) {
    if (a < b)
        return -1;
    if (b < a)
        return 1;
    return static_cast<int>(std::isnan(a)) - static_cast<int>(std::isnan(b));
}

// Компаратор по умолчанию для деревьев поиска
template<class T>
struct KeyLess {
    bool operator()(const T& a, const T& b) const {
        return a < b;
    }
};

// Обратный порядок (например, индекс "сначала самые молодые") - дерево обходится по убыванию
template<class T>
struct KeyGreater {
    bool operator()(const T& a, const T& b) const {
        return b < a;
    }
};
