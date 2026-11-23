#pragma once

#include <ostream>
#include "function.h"

inline std::ostream& operator<<(std::ostream& os, const PolynomialFunction& f) {
    if (f.get_degree() == 0 && f.get_coeff(0) == 0.0) {
        os << "0";
        return os;
    }
    bool first = true;
    for (int i = 0; i <= f.get_degree(); i++) {
        double c = f.get_coeff(i);
        if (c == 0.0)
            continue;
        if (!first) {
            if (c < 0) {
                os << " - ";
                c = -c;
            } else {
                os << " + ";
            }
        }
        if (i == 0) {
            os << c;
        } else if (i == 1) {
            if (c == 1.0)
                os << "x";
            else if (c == -1.0)
                os << "-x";
            else
                os << c << "x";
        } else {
            if (c == 1.0)
                os << "x^" << i;
            else if (c == -1.0)
                os << "-x^" << i;
            else
                os << c << "x^" << i;
        }
        first = false;
    }
    return os;
}
