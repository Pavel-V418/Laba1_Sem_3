#pragma once

#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "tree_exceptions.h"

// Преобразование элемента дерева в текст и обратно (для to_string / from_string).

// Для нового типа элементов достаточно написать специализацию:
//   template<> struct Serializer<Complex> ....
// Экранирование скобок формата делает само дерево - здесь о нём думать не нужно.
template<class T>
struct Serializer {
    static_assert(sizeof(T) == 0, "Serializer<T> is not specialized for this element type"); // проверка на этапе компиляции
};

template<>
struct Serializer<int> {
    static std::string to_string(int value) {
        return std::to_string(value);
    }

    static int from_string(const std::string& text) {
        if (text.empty())
            throw ParseException("int: empty value");
        errno = 0; // место, куда std::strtol записывает код ошибки
        char* end = nullptr;
        long value = std::strtol(text.c_str(), &end, 10); // std::strtol - преобразует строку в число long
        if (*end != '\0' || end == text.c_str())
            throw ParseException("int: not a number: \"" + text + "\"");
        // ERANGE - число вне допустимого диапозона
        if (errno == ERANGE || value < INT_MIN || value > INT_MAX) // число помещается в int
            throw ParseException("int: out of range: \"" + text + "\"");
        return static_cast<int>(value); // явное приведение long к int
    }
};

template<>
struct Serializer<double> {
    // %.17g - достаточно знаков, чтобы прочитанное значение совпало с исходным бит в бит
    static std::string to_string(double value) {
        char buffer[64];
        std::snprintf(buffer, sizeof(buffer), "%.17g", value);
        return buffer;
    }

    static double from_string(const std::string& text) {
        if (text.empty())
            throw ParseException("double: empty value");
        errno = 0;
        char* end = nullptr;
        double value = std::strtod(text.c_str(), &end);
        if (end == text.c_str() || end != text.c_str() + text.size())
            throw ParseException("double: not a number: \"" + text + "\"");
        // strtod может выставить ERANGE для представимого субнормального числа.
        if (errno == ERANGE && (value == 0.0 || !std::isfinite(value)))
            throw ParseException("double: out of range: \"" + text + "\"");
        return value;
    }
};

template<>
struct Serializer<std::string> {
    static std::string to_string(const std::string& value) {
        return value;
    }

    static std::string from_string(const std::string& text) {
        return text;
    }
};

// Сериализация для Complex
#include "../model/complex.h"

template<>
struct Serializer<Complex> {
    static std::string to_string(const Complex& value) {
        double imaginary = value.get_im();
        return Serializer<double>::to_string(value.get_re()) +
               (std::signbit(imaginary) ? "-" : "+") +
               Serializer<double>::to_string(std::abs(imaginary)) + "i";
    }

    static Complex from_string(const std::string& text) {
        if (text.empty())
            throw ParseException("Complex: empty value");

        if (text.back() != 'i')
            throw ParseException("Complex: missing 'i' at end: \"" + text + "\"");

        // strtod прочитает вещественную часть вместе с её показателем степени.
        char* separator = nullptr;
        std::strtod(text.c_str(), &separator);
        if (separator == text.c_str() || (*separator != '+' && *separator != '-'))
            throw ParseException("Complex: invalid format, expected <re>+<im>i: \"" + text + "\"");

        std::size_t sign_pos = separator - text.c_str();
        double re = Serializer<double>::from_string(text.substr(0, sign_pos));
        double im = Serializer<double>::from_string(text.substr(sign_pos, text.size() - sign_pos - 1));
        return Complex(re, im);
    }
};

// Формат v2: экранируем ':' и '\'. Префикс отличает его от старых записей.
#include "../model/person.h"

template<>
struct Serializer<Person> {
    static std::string to_string(const Person& p) {
        const std::string* names[] = {&p.get_last_name(), &p.get_first_name(), &p.get_middle_name()};
        bool escaped = false;
        for (const std::string* name : names)
            escaped = escaped || name->find(':') != std::string::npos;

        std::string result = escaped ? "v2:" : "";
        result += std::to_string(p.get_id());
        for (const std::string* name : names) {
            result += ':';
            for (char c : *name) {
                if (escaped && (c == ':' || c == '\\'))
                    result += '\\';
                result += c;
            }
        }
        result += ':';
        result += std::to_string(p.get_birth_year());
        return result;
    }

    static Person from_string(const std::string& text) {
        if (text.empty())
            throw ParseException("Person: empty value");

        std::string parts[5];
        int part_idx = 0;
        bool escaped = text.compare(0, 3, "v2:") == 0;

        for (std::size_t i = escaped ? 3 : 0; i < text.size(); i++) {
            char c = text[i];
            if (escaped && c == '\\') {
                if (++i == text.size() || (text[i] != ':' && text[i] != '\\'))
                    throw ParseException("Person: invalid escape in \"" + text + "\"");
                parts[part_idx] += text[i];
            } else if (c == ':') {
                if (part_idx >= 4)
                    throw ParseException("Person: too many colons in \"" + text + "\"");
                part_idx++;
            } else {
                parts[part_idx] += c;
            }
        }
        if (part_idx != 4)
            throw ParseException("Person: invalid format (expected id:last:first:middle:year): \"" + text + "\"");

        int id = Serializer<int>::from_string(parts[0]);
        int year = Serializer<int>::from_string(parts[4]);
        return Person(id, parts[1], parts[2], parts[3], year);
    }
};

// Сериализация для PolynomialFunction: список коэффициентов через запятую, например "1,2,3" для 1 + 2x + 3x^2
#include "../model/function.h"

template<>
struct Serializer<PolynomialFunction> {
    static std::string to_string(const PolynomialFunction& f) {
        std::string res;
        for (int i = 0; i <= f.get_degree(); i++) {
            if (i > 0)
                res += ",";
            res += Serializer<double>::to_string(f.get_coeff(i));
        }
        return res;
    }

    static PolynomialFunction from_string(const std::string& text) {
        if (text.empty())
            throw ParseException("PolynomialFunction: empty value");

        MutableArraySequence<double> coeffs;
        std::size_t start = 0;
        for (std::size_t i = 0; i < text.size(); i++) {
            if (text[i] == ',') {
                coeffs.append(Serializer<double>::from_string(text.substr(start, i - start)));
                start = i + 1;
            }
        }
        coeffs.append(Serializer<double>::from_string(text.substr(start)));

        double* arr = new double[coeffs.get_length()];
        for (int i = 0; i < coeffs.get_length(); i++)
            arr[i] = coeffs.get(i);
        PolynomialFunction res(arr, coeffs.get_length());
        delete[] arr;
        return res;
    }
};
