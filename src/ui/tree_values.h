#pragma once

#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include <type_traits>

#include "console_input.h"
#include "model/function_print.h"
#include "model/person_print.h"
#include "tree/serializer.h"

namespace tree_ui {

template<class T>
T ReadValue(const char* prompt) {
    std::cout << prompt;
    if constexpr (std::is_same_v<T, int>) {
        return ReadInt();
    } else if constexpr (std::is_same_v<T, std::string>) {
        return ReadLine();
    } else if constexpr (std::is_same_v<T, Person>) {
        std::cout << "\nID: ";
        int id = ReadInt();
        std::cout << "Last name: ";
        std::string last = ReadNonEmptyLine();
        std::cout << "First name: ";
        std::string first = ReadNonEmptyLine();
        std::cout << "Middle name (optional): ";
        std::string middle = Trim(ReadLine());
        std::cout << "Birth year: ";
        int year = ReadIntInRange(Person::MIN_BIRTH_YEAR, Person::MAX_BIRTH_YEAR);
        return Person(id, last, first, middle, year);
    } else {
        if constexpr (std::is_same_v<T, Complex>)
            std::cout << "(e.g. 1+2i) ";
        if constexpr (std::is_same_v<T, PolynomialFunction>)
            std::cout << "(coefficients from constant up, e.g. 1,2 for 1+2x) ";

        T value = Serializer<T>::from_string(Trim(ReadLine()));
        if constexpr (std::is_same_v<T, double>) {
            if (!std::isfinite(value))
                throw std::invalid_argument("A finite real number is required");
        }
        return value;
    }
}

template<class T>
T ReadKey(const char* prompt) {
    if constexpr (std::is_same_v<T, Person>) {
        std::cout << prompt << "(ID) ";
        return Person(ReadInt(), "-", "-", "", Person::MIN_BIRTH_YEAR);
    } else {
        return ReadValue<T>(prompt);
    }
}

template<class T>
void PrintValue(const T& value) {
    if constexpr (std::is_same_v<T, std::string>) {
        std::cout << std::quoted(value);
    } else if constexpr (std::is_same_v<T, Person> || std::is_same_v<T, PolynomialFunction>) {
        std::cout << value;
    } else {
        std::cout << Serializer<T>::to_string(value);
    }
}

// Номера дают разные значения в возрастающем порядке для каждого типа.
template<class T>
T GeneratedValue(int key) {
    if constexpr (std::is_same_v<T, int>) {
        return key;
    } else if constexpr (std::is_same_v<T, double>) {
        return key / 4.0;
    } else if constexpr (std::is_same_v<T, std::string>) {
        return "Item " + std::to_string(10000 + key);
    } else if constexpr (std::is_same_v<T, Complex>) {
        return Complex(key, key % 11 - 5);
    } else if constexpr (std::is_same_v<T, Person>) {
        return Person(key + 1, "Person " + std::to_string(key + 1), "Sample", "", 1980 + key % 30);
    } else {
        static_assert(std::is_same_v<T, PolynomialFunction>);
        return PolynomialFunction{static_cast<double>(key), 1.0};
    }
}

} // namespace tree_ui
