#pragma once
#include <cmath>
#include "../common/comparators.h"

// Комплексное число z = re + im*i
class Complex {
public:
    Complex() : re_(0.0), im_(0.0) {}
    Complex(double re, double im) : re_(re), im_(im) {}
    explicit Complex(double re) : re_(re), im_(0.0) {}

    double get_re() const { return re_; }
    double get_im() const { return im_; }

    double abs() const {
        return std::hypot(re_, im_);
    }

    Complex operator+(const Complex& other) const {
        return Complex(re_ + other.re_, im_ + other.im_);
    }

    Complex operator-(const Complex& other) const {
        return Complex(re_ - other.re_, im_ - other.im_);
    }

    Complex operator*(const Complex& other) const {
        return Complex(re_ * other.re_ - im_ * other.im_,
                       re_ * other.im_ + im_ * other.re_);
    }

    bool operator==(const Complex& other) const {
        return compare_double(re_, other.re_) == 0 && compare_double(im_, other.im_) == 0;
    }

    bool is_approximately_equal(const Complex& other, double tolerance = 1e-9) const {
        return *this == other || (std::abs(re_ - other.re_) <= tolerance
                              && std::abs(im_ - other.im_) <= tolerance);
    }

    bool operator!=(const Complex& other) const {
        return !(*this == other);
    }

    // Лексикографическое сравнение по умолчанию: сначала re, затем im
    bool operator<(const Complex& other) const {
        int real_order = compare_double(re_, other.re_);
        if (real_order != 0)
            return real_order < 0;
        return compare_double(im_, other.im_) < 0;
    }

    bool operator>(const Complex& other) const {
        return other < *this;
    }

private:
    double re_;
    double im_;
};

// Компаратор по модулю комплексного числа
struct ComplexAbsLess {
    bool operator()(const Complex& a, const Complex& b) const {
        int abs_order = compare_double(a.abs(), b.abs());
        if (abs_order != 0)
            return abs_order < 0;
        return a < b; // при равных модулях сравниваем компоненты
    }
};

