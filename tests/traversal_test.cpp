// ЛР-1, этап 3: обходы по заданному порядку, формат-строки, сохранение в строку и чтение из строки
#include <gtest/gtest.h>

#include <initializer_list>
#include <random>
#include <string>

#include "tree/bs_tree.h"

using AvlInt = BSTree<int>;
using PlainInt = BSTree<int, KeyLess<int>, NoBalance>;

namespace {

template<class Tree>
Tree tree_of(std::initializer_list<typename Tree::value_type> items) {
    return Tree(items.begin(), static_cast<int>(items.size()));
}

// Содержимое последовательности одной строкой "1 2 3" - удобно сравнивать и читать в выводе
template<class T>
std::string joined(const Sequence<T>& seq) {
    std::string result;
    for (int i = 0; i < seq.get_length(); i++) {
        if (i > 0)
            result += ' ';
        result += Serializer<T>::to_string(seq.get(i));
    }
    return result;
}

//          50
//       30      70
//     20  40  60  80
PlainInt sample_tree() {
    return tree_of<PlainInt>({50, 30, 70, 20, 40, 60, 80});
}

const char* const ALL_ORDERS[] = {"КЛП", "КПЛ", "ЛКП", "ЛПК", "ПКЛ", "ПЛК"};

}  // namespace

/*============ Формат-строки ============*/

TEST(TraversalOrderTest, ParsesFormatFromAssignment) {
    TraversalOrder order = TraversalOrder::parse("{К}(Л)[П]");
    EXPECT_EQ(order.letters(), "КЛП");
    EXPECT_EQ(order.part(0), TraversalPart::Root);
    EXPECT_EQ(order.open(1), '(');
    EXPECT_EQ(order.close(2), ']');
    EXPECT_EQ(order.to_format(), "{К}(Л)[П]");
}

TEST(TraversalOrderTest, LatinAndLowercaseLetters) {
    EXPECT_EQ(TraversalOrder::parse("{K}(L)[R]").to_format(), "{К}(Л)[П]");
    EXPECT_EQ(TraversalOrder::parse("<л>|к|<п>").letters(), "ЛКП");
}

TEST(TraversalOrderTest, RootWithoutBracketsAndMixedBrackets) {
    TraversalOrder order = TraversalOrder::parse("{Л]К[П}"); // как "{1]К[2][3}" в пояснении ТЗ
    EXPECT_EQ(order.letters(), "ЛКП");
    EXPECT_FALSE(order.has_brackets(1));
    EXPECT_EQ(order.open(0), '{');
    EXPECT_EQ(order.close(0), ']');
}

TEST(TraversalOrderTest, StandardOrders) {
    for (const char* letters : ALL_ORDERS)
        EXPECT_EQ(TraversalOrder::standard(letters).letters(), letters);
    EXPECT_EQ(TraversalOrder::standard("LKR").to_format(), "(Л){К}[П]");
}

TEST(TraversalOrderTest, InvalidFormatsRejected) {
    const char* invalid[] = {
        "",                 // пусто
        "{К}(Л)",           // две части
        "{К}(Л)[П]x",       // лишние символы
        "{К}(Л)[К]",        // К дважды, нет П
        "К(Л)[П]",          // корень без скобок первым
        "{К}Л[П]",          // поддерево без скобок
        "{К}(Л{[П]",        // закрывающая скобка Л совпадает с открывающей первой части
        "\\К\\(Л)[П]",      // '\' нельзя использовать как скобку
        "1К1(Л)[П]",        // цифра как скобка
        "{X}(Л)[П]",        // неизвестная буква
        "{К",               // нет закрывающей скобки
    };
    for (const char* format : invalid)
        EXPECT_THROW(TraversalOrder::parse(format), ParseException) << format;

    EXPECT_THROW(TraversalOrder::standard("КЛ"), ParseException);
    EXPECT_THROW(TraversalOrder::standard("ККП"), ParseException);
    EXPECT_THROW(TraversalOrder::standard("КЛПК"), ParseException);
}

/*============ Шесть обходов ============*/

TEST(TraverseTest, AllSixOrders) {
    PlainInt t = sample_tree();
    struct Case { const char* letters; const char* expected; };
    Case cases[] = {
        {"КЛП", "50 30 20 40 70 60 80"},
        {"КПЛ", "50 70 80 60 30 40 20"},
        {"ЛКП", "20 30 40 50 60 70 80"},
        {"ЛПК", "20 40 30 60 80 70 50"},
        {"ПКЛ", "80 70 60 50 40 30 20"},
        {"ПЛК", "80 60 70 40 20 30 50"},
    };
    for (const Case& c : cases)
        EXPECT_EQ(joined(t.to_sequence(TraversalOrder::standard(c.letters))), c.expected) << c.letters;
}

TEST(TraverseTest, InOrderMatchesAscending) {
    AvlInt t;
    for (int i = 100; i > 0; i--)
        t.insert(i * 7 % 101);
    EXPECT_EQ(joined(t.to_sequence(TraversalOrder::standard("ЛКП"))), joined(t.to_sequence()));
}

TEST(TraverseTest, EmptyTree) {
    PlainInt t;
    int visited = 0;
    t.traverse(TraversalOrder::standard("КЛП"), [&visited](int) { visited++; });
    EXPECT_EQ(visited, 0);
}

TEST(TraverseTest, DeepDegenerateTreeNoStackOverflow) {
    PlainInt t;
    const int n = 100000;
    for (int i = 0; i < n; i++)
        t.insert(i);
    long long sum = 0;
    t.traverse(TraversalOrder::standard("ПЛК"), [&sum](int x) { sum += x; });
    EXPECT_EQ(sum, static_cast<long long>(n) * (n - 1) / 2);
}

/*============ Сохранение в строку ============*/

TEST(ToStringTest, ExactFormatFromAssignment) {
    auto t = tree_of<PlainInt>({2, 1, 3});
    EXPECT_EQ(t.to_string("{К}(Л)[П]"), "{2}({1}()[])[{3}()[]]");
    EXPECT_EQ(t.to_string(), "{2}({1}()[])[{3}()[]]"); // фиксированный обход по умолчанию
    EXPECT_EQ(t.to_string("{K}(L)[R]"), t.to_string("{К}(Л)[П]"));
}

TEST(ToStringTest, OtherOrdersAndBareRoot) {
    auto t = tree_of<PlainInt>({2, 1, 3});
    EXPECT_EQ(t.to_string("(Л){К}[П]"), "((){1}[]){2}[(){3}[]]");
    EXPECT_EQ(t.to_string("(Л)К[П]"), "(()1[])2[()3[]]");
    EXPECT_EQ(t.to_string("(Л)[П]К"), "(()[]1)[()[]3]2");
    EXPECT_EQ(t.to_string("[П]{К}(Л)"), "[[]{3}()]{2}([]{1}())");
}

TEST(ToStringTest, EmptyAndSingle) {
    PlainInt empty;
    EXPECT_EQ(empty.to_string("{К}(Л)[П]"), "");
    auto one = tree_of<PlainInt>({7});
    EXPECT_EQ(one.to_string("{К}(Л)[П]"), "{7}()[]");
}

TEST(ToStringTest, InvalidFormatThrows) {
    auto t = tree_of<PlainInt>({1});
    EXPECT_THROW(t.to_string("{К}(Л)"), ParseException);
}

TEST(ToStringTest, ValuesWithBracketsAreEscaped) {
    auto t = tree_of<BSTree<std::string>>({"a(b)", "x\\y"});
    std::string text = t.to_string("{К}(Л)[П]");
    EXPECT_NE(text.find("a\\(b\\)"), std::string::npos) << text;
    EXPECT_NE(text.find("x\\\\y"), std::string::npos) << text;
}

/*============ Чтение из строки ============*/

TEST(FromStringTest, ExactStructureRestored) {
    PlainInt t = PlainInt::from_string("{2}({1}()[])[{3}()[]]", "{К}(Л)[П]");
    EXPECT_EQ(t.get_count(), 3);
    EXPECT_TRUE(t.check_invariants());
    EXPECT_EQ(joined(t.to_sequence(TraversalOrder::standard("КЛП"))), "2 1 3");
}

TEST(FromStringTest, EmptyString) {
    EXPECT_TRUE(PlainInt::from_string("", "{К}(Л)[П]").is_empty());
}

// Для каждого из 6 порядков: сохранили -> прочитали -> строка та же (структура сохранилась)
TEST(FromStringTest, RoundTripAllOrdersPlain) {
    std::mt19937 rng(7);
    PlainInt t;
    for (int i = 0; i < 300; i++)
        t.try_insert(static_cast<int>(rng() % 2000) - 1000);

    for (const char* letters : ALL_ORDERS) {
        TraversalOrder order = TraversalOrder::standard(letters);
        std::string text = t.to_string(order);
        PlainInt restored = PlainInt::from_string(text, order);
        EXPECT_TRUE(restored.check_invariants()) << letters;
        EXPECT_EQ(restored.get_count(), t.get_count()) << letters;
        EXPECT_EQ(restored.to_string(order), text) << letters;
    }
}

TEST(FromStringTest, RoundTripAllOrdersAvl) {
    AvlInt t;
    for (int i = 0; i < 500; i++)
        t.insert(i);
    for (const char* letters : ALL_ORDERS) {
        TraversalOrder order = TraversalOrder::standard(letters);
        AvlInt restored = AvlInt::from_string(t.to_string(order), order);
        EXPECT_TRUE(restored.check_invariants()) << letters;
        EXPECT_EQ(restored.to_string(order), t.to_string(order)) << letters; // AVL-структура та же
    }
}

TEST(FromStringTest, CustomFormatsRoundTrip) {
    PlainInt t = sample_tree();
    const char* formats[] = {"(Л)К[П]", "(Л)[П]К", "{Л]К[П}", "<К>|Л|/П/", "{К}(Л){П}"};
    for (const char* format : formats) {
        PlainInt restored = PlainInt::from_string(t.to_string(format), format);
        EXPECT_EQ(restored.to_string(format), t.to_string(format)) << format;
    }
}

TEST(FromStringTest, StringsWithSpecialCharacters) {
    auto t = tree_of<BSTree<std::string>>({"plain", "a(b)", "", "x\\y", "{]}", "[П]", "Привет"});
    const char* formats[] = {"{К}(Л)[П]", "(Л)К[П]", "[П](Л){К}"};
    for (const char* format : formats) {
        auto restored = BSTree<std::string>::from_string(t.to_string(format), format);
        EXPECT_EQ(restored.get_count(), 7) << format;
        EXPECT_TRUE(restored.contains("{]}")) << format;
        EXPECT_TRUE(restored.contains("")) << format;
        EXPECT_EQ(restored.to_string(format), t.to_string(format)) << format;
    }
}

TEST(FromStringTest, DoublesExact) {
    auto t = tree_of<BSTree<double>>({0.1, -2.5e-300, 1e308, 3.0, -0.0001});
    auto restored = BSTree<double>::from_string(t.to_string());
    EXPECT_EQ(restored.get_count(), 5);
    EXPECT_TRUE(restored.contains(0.1)); // %.17g - значение восстанавливается бит в бит
    EXPECT_TRUE(restored.contains(-2.5e-300));
}

TEST(FromStringTest, DeepDegenerateString) {
    PlainInt chain;
    for (int i = 0; i < 20000; i++)
        chain.insert(i);
    std::string text = chain.to_string();
    PlainInt restored = PlainInt::from_string(text);
    EXPECT_EQ(restored.get_count(), 20000);
    EXPECT_EQ(restored.get_height(), 20000);
}

TEST(FromStringTest, UnbalancedStringIntoAvlIsRebalanced) {
    PlainInt chain;
    for (int i = 1; i <= 100; i++)
        chain.insert(i);
    AvlInt t = AvlInt::from_string(chain.to_string());
    EXPECT_EQ(t.get_count(), 100);
    EXPECT_TRUE(t.check_invariants());
    EXPECT_LE(t.get_height(), 7);
}

TEST(FromStringTest, InvalidStringsRejected) {
    const char* invalid[] = {
        "{1}(",                      // обрыв
        "{1}()[]x",                  // мусор в конце
        "{abc}()[]",                 // не число
        "{}()[]",                    // пустое значение для int
        "{99999999999}()[]",         // переполнение int
        "{5}({7}()[])[]",            // 7 слева от 5 - нарушен порядок
        "{5}({5}()[])[]",            // повтор
        "{1}",                       // нет поддеревьев
        "{1\\",                      // обрыв после экранирования
        "({1}()[])",                 // не тот порядок частей
        "{1}()[]{2}()[]",            // два корня
    };
    for (const char* text : invalid)
        EXPECT_THROW(PlainInt::from_string(text, "{К}(Л)[П]"), ParseException) << text;
}

TEST(FromStringTest, ErrorMessageHasPosition) {
    try {
        PlainInt::from_string("{1}({x}()[])[]", "{К}(Л)[П]");
        FAIL() << "expected ParseException";
    } catch (const ParseException& e) {
        EXPECT_NE(std::string(e.what()).find("position 5"), std::string::npos) << e.what();
    }
}
