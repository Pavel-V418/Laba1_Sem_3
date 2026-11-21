#pragma once
#include <cmath>
#include <initializer_list>
#include "../LR2/mutableArraySequence.h"
#include "../common/comparators.h"

// Многочлен P_n(x) = a0 + a1*x + a2*x^2 + ... + an*x^n
// По ТЗ: "Многочлен является функцией, на множестве функций определена ассоциативная операция - композиция (f o g)(x) = f(g(x))"
class PolynomialFunction {
public:
    // f(x) = 0
    PolynomialFunction() {
        coeffs_.append(0.0);
    }

    // Из массива коэффициентов: coeffs[0] = a0, coeffs[1] = a1, ...
    PolynomialFunction(const double* coeffs, int count) {
        if (count <= 0 || coeffs == nullptr) {
            coeffs_.append(0.0);
            return;
        }
        for (int i = 0; i < count; i++) {
            coeffs_.append(coeffs[i]);
        }
        normalize();
    }

    PolynomialFunction(std::initializer_list<double> coeffs) {
        if (coeffs.size() == 0) {
            coeffs_.append(0.0);
            return;
        }
        for (double c : coeffs) {
            coeffs_.append(c);
        }
        normalize();
    }

    int get_degree() const {
        return coeffs_.get_length() - 1;
    }

    double get_coeff(int i) const {
        if (i < 0 || i >= coeffs_.get_length())
            return 0.0;
        return coeffs_.get(i);
    }

    // Вычисление значения f(x) по схеме Горнера
    double evaluate(double x) const {
        int n = get_degree();
        double res = coeffs_.get(n);
        for (int i = n - 1; i >= 0; i--) {
            res = res * x + coeffs_.get(i);
        }
        return res;
    }

    // Вызов как функции: f(x)
    double operator()(double x) const {
        return evaluate(x);
    }

    // Сложение полиномов
    PolynomialFunction operator+(const PolynomialFunction& other) const {
        int len_a = coeffs_.get_length();
        int len_b = other.coeffs_.get_length();
        int max_len = len_a < len_b ? len_b : len_a;
        MutableArraySequence<double> res_coeffs;
        for (int i = 0; i < max_len; i++) {
            double a = get_coeff(i);
            double b = other.get_coeff(i);
            res_coeffs.append(a + b);
        }
        PolynomialFunction res;
        res.coeffs_ = res_coeffs;
        res.normalize();
        return res;
    }

    // Умножение полиномов
    PolynomialFunction operator*(const PolynomialFunction& other) const {
        int deg_a = get_degree();
        int deg_b = other.get_degree();
        int res_len = deg_a + deg_b + 1;

        MutableArraySequence<double> res_coeffs;
        for (int i = 0; i < res_len; i++)
            res_coeffs.append(0.0);

        for (int i = 0; i <= deg_a; i++) {
            for (int j = 0; j <= deg_b; j++) {
                res_coeffs.set(res_coeffs.get(i + j) + get_coeff(i) * other.get_coeff(j), i + j);
            }
        }

        PolynomialFunction res;
        res.coeffs_ = res_coeffs;
        res.normalize();
        return res;
    }

    // Композиция функций: (f o g)(x) = f(g(x))
    // Если f(x) = a0 + a1*x + a2*x^2 + ..., то f(g(x)) вычисляется по схеме Горнера над полиномами
    PolynomialFunction compose(const PolynomialFunction& g) const {
        int n = get_degree();
        PolynomialFunction res{coeffs_.get(n)};
        for (int i = n - 1; i >= 0; i--) {
            res = res * g + PolynomialFunction{coeffs_.get(i)};
        }
        return res;
    }

    bool operator==(const PolynomialFunction& other) const {
        if (get_degree() != other.get_degree())
            return false;
        for (int i = 0; i <= get_degree(); i++) {
            if (compare_double(get_coeff(i), other.get_coeff(i)) != 0)
                return false;
        }
        return true;
    }

    bool is_approximately_equal(const PolynomialFunction& other, double tolerance = 1e-9) const {
        int degree_a = get_degree();
        int degree_b = other.get_degree();
        int degree = degree_a < degree_b ? degree_b : degree_a;
        for (int i = 0; i <= degree; i++) {
            double a = get_coeff(i);
            double b = other.get_coeff(i);
            if (compare_double(a, b) != 0 && !(std::abs(a - b) <= tolerance))
                return false;
        }
        return true;
    }

    bool operator!=(const PolynomialFunction& other) const {
        return !(*this == other);
    }

    // Сравнение: сначала по степени, затем по коэффициентам от старшего к младшему
    bool operator<(const PolynomialFunction& other) const {
        if (get_degree() != other.get_degree())
            return get_degree() < other.get_degree();
        for (int i = get_degree(); i >= 0; i--) {
            int coeff_order = compare_double(get_coeff(i), other.get_coeff(i));
            if (coeff_order != 0)
                return coeff_order < 0;
        }
        return false;
    }

    bool operator>(const PolynomialFunction& other) const {
        return other < *this;
    }

private:
    MutableArraySequence<double> coeffs_;

    void normalize() {
        while (coeffs_.get_length() > 1 && coeffs_.get_last() == 0.0) {
            coeffs_.remove_at(coeffs_.get_length() - 1);
        }
    }
};

