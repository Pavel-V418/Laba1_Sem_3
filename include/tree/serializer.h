#pragma once

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "tree_exceptions.h"

// Преобразование элемента дерева в текст и обратно (для to_string / from_string).
//
// Для нового типа элементов достаточно написать специализацию:
//   template<> struct Serializer<Complex> {
//       static std::string to_string(const Complex& value);
//       static Complex from_string(const std::string& text); // ошибка => ParseException
//   };
// Экранирование скобок формата делает само дерево - здесь о нём думать не нужно.
template<class T>
struct Serializer {
    static_assert(sizeof(T) == 0, "Serializer<T> is not specialized for this element type");
};

template<>
struct Serializer<int> {
    static std::string to_string(int value) {
        return std::to_string(value);
    }

    static int from_string(const std::string& text) {
        if (text.empty())
            throw ParseException("int: empty value");
        errno = 0;
        char* end = nullptr;
        long value = std::strtol(text.c_str(), &end, 10);
        if (*end != '\0' || end == text.c_str())
            throw ParseException("int: not a number: \"" + text + "\"");
        if (errno == ERANGE || value < INT_MIN || value > INT_MAX)
            throw ParseException("int: out of range: \"" + text + "\"");
        return static_cast<int>(value);
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
        if (*end != '\0' || end == text.c_str())
            throw ParseException("double: not a number: \"" + text + "\"");
        if (errno == ERANGE)
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
