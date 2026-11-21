#pragma once

#include <ostream>

#include "complex.h"

inline std::ostream& operator<<(std::ostream& os, const Complex& c) {
    if (c.get_im() >= 0) {
        os << c.get_re() << "+" << c.get_im() << "i";
    } else {
        os << c.get_re() << "-" << -c.get_im() << "i";
    }
    return os;
}
