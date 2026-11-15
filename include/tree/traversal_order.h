#pragma once

#include <string>

#include "tree_exceptions.h"

// Порядок обхода бинарного дерева + скобки для строкового представления.
//
// Формат-строка состоит из трёх частей - по одной на К (корень), Л (левое поддерево),
// П (правое поддерево), в порядке обхода:
//     "{К}(Л)[П]"   - КЛП: корень в {}, левое поддерево в (), правое в []
//     "(Л)К[П]"     - ЛКП: корень без скобок (как в пояснении ТЗ "{1]К[2][3}")
//     "{K}(L)[R]"   - то же латиницей (K - root, L - left, R - right), удобно в консоли Windows
//
// Правила (нарушение => ParseException):
//   - каждая из букв К/Л/П ровно один раз;
//   - части Л и П обязательно в скобках: пустое поддерево записывается как пустые скобки "()";
//   - К может быть без скобок, но не первой частью (иначе пустое поддерево
//     неотличимо от узла с пустым значением);
//   - скобка - один печатный ASCII-символ, не буква/цифра, не пробел и не '\\';
//     открывающая и закрывающая скобки могут быть любыми ("{1]" из ТЗ тоже допустимо);
//   - закрывающая скобка Л/П не совпадает с открывающей скобкой первой части
//     (иначе нельзя понять, пусто поддерево или начинается новый узел).
//
// Значения в строке экранируются: '\\' и все символы скобок записываются как "\\<символ>".

enum class TraversalPart {
    Root,
    Left,
    Right
};

class TraversalOrder {
public:
    static constexpr int PARTS = 3;

    // Разбор формат-строки вида "{К}(Л)[П]"
    static TraversalOrder parse(const std::string& format) {
        TraversalOrder order;
        std::size_t pos = 0;

        for (int i = 0; i < PARTS; i++) {
            if (pos >= format.size())
                throw ParseException("format: expected 3 parts (К, Л, П), got " + std::to_string(i));

            TraversalPart part;
            if (read_letter(format, pos, part)) {
                // часть без скобок - допустима только для корня
                if (part != TraversalPart::Root)
                    throw ParseException("format: subtree part must be enclosed in brackets");
                order.parts_[i] = part;
                order.bracketed_[i] = false;
                order.open_[i] = order.close_[i] = '\0';
                continue;
            }

            char open = format[pos++];
            check_bracket(open);
            if (!read_letter(format, pos, part))
                throw ParseException("format: expected letter К/Л/П after '" + std::string(1, open) + "'");
            if (pos >= format.size())
                throw ParseException("format: missing closing bracket");
            char close = format[pos++];
            check_bracket(close);

            order.parts_[i] = part;
            order.bracketed_[i] = true;
            order.open_[i] = open;
            order.close_[i] = close;
        }

        if (pos != format.size())
            throw ParseException("format: unexpected characters after 3 parts");

        order.validate();
        return order;
    }

    // Один из 6 стандартных обходов по буквам ("КЛП", "ЛКП", "LKR", ...) со скобками
    // по умолчанию: К - {}, Л - (), П - [].  Это "сохранение по фиксированному обходу".
    static TraversalOrder standard(const std::string& letters) {
        TraversalOrder order;
        std::size_t pos = 0;
        for (int i = 0; i < PARTS; i++) {
            TraversalPart part;
            if (!read_letter(letters, pos, part))
                throw ParseException("traversal: expected 3 letters К/Л/П, e.g. \"КЛП\"");
            order.parts_[i] = part;
            order.bracketed_[i] = true;
            order.open_[i] = default_open(part);
            order.close_[i] = default_close(part);
        }
        if (pos != letters.size())
            throw ParseException("traversal: expected exactly 3 letters");

        order.validate();
        return order;
    }

    TraversalPart part(int i) const { return parts_[i]; }
    bool has_brackets(int i) const { return bracketed_[i]; }
    char open(int i) const { return open_[i]; }
    char close(int i) const { return close_[i]; }

    // Символ, который в значениях надо экранировать
    bool is_special(char c) const {
        if (c == ESCAPE)
            return true;
        for (int i = 0; i < PARTS; i++)
            if (bracketed_[i] && (c == open_[i] || c == close_[i]))
                return true;
        return false;
    }

    // Обратно в формат-строку: "{К}(Л)[П]"
    std::string to_format() const {
        std::string result;
        for (int i = 0; i < PARTS; i++) {
            if (bracketed_[i])
                result += open_[i];
            result += letter(parts_[i]);
            if (bracketed_[i])
                result += close_[i];
        }
        return result;
    }

    // Только буквы: "КЛП"
    std::string letters() const {
        std::string result;
        for (int i = 0; i < PARTS; i++)
            result += letter(parts_[i]);
        return result;
    }

    static constexpr char ESCAPE = '\\';

private:
    TraversalPart parts_[PARTS];
    bool bracketed_[PARTS];
    char open_[PARTS];
    char close_[PARTS];

    TraversalOrder() : parts_(), bracketed_(), open_(), close_() {}

    static const char* letter(TraversalPart part) {
        switch (part) {
            case TraversalPart::Root:  return "К";
            case TraversalPart::Left:  return "Л";
            default:                   return "П";
        }
    }

    static char default_open(TraversalPart part) {
        return part == TraversalPart::Root ? '{' : part == TraversalPart::Left ? '(' : '[';
    }

    static char default_close(TraversalPart part) {
        return part == TraversalPart::Root ? '}' : part == TraversalPart::Left ? ')' : ']';
    }

    // Читает букву части: К/Л/П (UTF-8, любой регистр) или K/L/R (латиница). Сдвигает pos.
    static bool read_letter(const std::string& s, std::size_t& pos, TraversalPart& part) {
        if (pos >= s.size())
            return false;

        char c = s[pos];
        if (c == 'K' || c == 'k') { part = TraversalPart::Root;  pos++; return true; }
        if (c == 'L' || c == 'l') { part = TraversalPart::Left;  pos++; return true; }
        if (c == 'R' || c == 'r') { part = TraversalPart::Right; pos++; return true; }

        // Кириллица в UTF-8 - два байта
        const char* cyrillic[] = {"К", "к", "Л", "л", "П", "п"};
        const TraversalPart kinds[] = {TraversalPart::Root, TraversalPart::Root, TraversalPart::Left,
                                       TraversalPart::Left, TraversalPart::Right, TraversalPart::Right};
        for (int i = 0; i < 6; i++) {
            if (s.compare(pos, 2, cyrillic[i]) == 0) {
                part = kinds[i];
                pos += 2;
                return true;
            }
        }
        return false;
    }

    static void check_bracket(char c) {
        bool printable = c > ' ' && c < 127;
        bool alnum = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        if (!printable || alnum || c == ESCAPE)
            throw ParseException("format: invalid bracket character");
    }

    void validate() const {
        int seen[PARTS] = {0, 0, 0};
        for (int i = 0; i < PARTS; i++)
            seen[static_cast<int>(parts_[i])]++;
        for (int i = 0; i < PARTS; i++)
            if (seen[i] != 1)
                throw ParseException("format: each of К, Л, П must appear exactly once");

        if (!bracketed_[0])
            throw ParseException("format: root without brackets cannot be the first part");

        for (int i = 0; i < PARTS; i++)
            if (parts_[i] != TraversalPart::Root && close_[i] == open_[0])
                throw ParseException("format: closing bracket of a subtree must differ from the "
                                     "opening bracket of the first part");
    }
};
