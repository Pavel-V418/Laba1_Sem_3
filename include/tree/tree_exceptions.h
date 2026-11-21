#pragma once

#include <stdexcept>
#include <string>

// Иерархия исключений. Все ошибки деревьев ловятся одним catch (const TreeException&).
class TreeException : public std::runtime_error {
public:
    explicit TreeException(const std::string& message)
        : std::runtime_error(message) {}
};

// find / remove / subtree по элементу, которого нет в дереве
class ElementNotFoundException : public TreeException {
public:
    explicit ElementNotFoundException(const std::string& message = "Element not found")
        : TreeException(message) {}
};

// insert элемента, эквивалентного уже существующему (по компаратору)
class DuplicateElementException : public TreeException {
public:
    explicit DuplicateElementException(const std::string& message = "Element already exists")
        : TreeException(message) {}
};

// min / max и подобные операции над пустым деревом
class EmptyTreeException : public TreeException {
public:
    explicit EmptyTreeException(const std::string& message = "Tree is empty")
        : TreeException(message) {}
};

// Некорректная формат-строка обхода или строка с сериализованным деревом
class ParseException : public TreeException {
public:
    explicit ParseException(const std::string& message)
        : TreeException("Parse error: " + message), detail_(message) {}

    // Сообщение без префикса "Parse error: " - чтобы дополнять его (например, позицией)
    const std::string& detail() const { return detail_; }

private:
    std::string detail_;
};
