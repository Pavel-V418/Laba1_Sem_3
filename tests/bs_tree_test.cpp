// ЛР-1, этапы 1-7: BSTree<T> - базовые операции, AVL-балансировка, map/where/reduce, поддеревья, прошивка, типы
#include "tree/bs_tree.h"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <memory>
#include <random>
#include <initializer_list>
#include <sstream>
#include <string>

#include "model/person.h"
#include "model/complex.h"
#include "model/function.h"
#include "tree/bs_tree_print.h"

// В тестах STL допустим: initializer_list только для удобной записи tree_of<Tree>({1, 2, 3}).
// Дерево собирается через конструктор ядра из обычного массива.
template<class Tree>
Tree tree_of(std::initializer_list<typename Tree::value_type> items) {
    return Tree(items.begin(), static_cast<int>(items.size()));
}

using AvlInt = BSTree<int>;                            // AvlBalance по умолчанию
using PlainInt = BSTree<int, KeyLess<int>, NoBalance>;

/*============ Общие тесты для обеих стратегий ============*/

template<class Tree>
class BSTreeTest : public ::testing::Test {};

using TreeTypes = ::testing::Types<AvlInt, PlainInt>;
TYPED_TEST_SUITE(BSTreeTest, TreeTypes);

TYPED_TEST(BSTreeTest, EmptyTree) {
    TypeParam t;
    EXPECT_TRUE(t.is_empty());
    EXPECT_EQ(t.get_count(), 0);
    EXPECT_EQ(t.get_height(), 0);
    EXPECT_TRUE(t.check_invariants());
    EXPECT_FALSE(t.contains(1));
    EXPECT_THROW(t.get_min(), EmptyTreeException);
    EXPECT_THROW(t.get_max(), EmptyTreeException);
    EXPECT_THROW(t.find(1), ElementNotFoundException);
    EXPECT_THROW(t.remove(1), ElementNotFoundException);
    EXPECT_EQ(t.to_sequence().get_length(), 0);
}

TYPED_TEST(BSTreeTest, SingleElement) {
    TypeParam t;
    t.insert(5);
    EXPECT_EQ(t.get_count(), 1);
    EXPECT_EQ(t.get_height(), 1);
    EXPECT_EQ(t.get_min(), 5);
    EXPECT_EQ(t.get_max(), 5);
    EXPECT_EQ(t.find(5), 5);
    t.remove(5);
    EXPECT_TRUE(t.is_empty());
    EXPECT_TRUE(t.check_invariants());
}

TYPED_TEST(BSTreeTest, InsertContainsFind) {
    auto t = tree_of<TypeParam>({50, 30, 70, 20, 40, 60, 80});
    EXPECT_EQ(t.get_count(), 7);
    for (int x : {20, 30, 40, 50, 60, 70, 80})
        EXPECT_TRUE(t.contains(x)) << x;
    for (int x : {0, 25, 55, 100})
        EXPECT_FALSE(t.contains(x)) << x;
    EXPECT_EQ(t.get_min(), 20);
    EXPECT_EQ(t.get_max(), 80);
    EXPECT_TRUE(t.check_invariants());
}

TYPED_TEST(BSTreeTest, DuplicatesRejected) {
    auto t = tree_of<TypeParam>({1, 2, 3});
    EXPECT_THROW(t.insert(2), DuplicateElementException);
    EXPECT_FALSE(t.try_insert(2));
    EXPECT_TRUE(t.try_insert(4));
    EXPECT_EQ(t.get_count(), 4);
    EXPECT_THROW(tree_of<TypeParam>({1, 1}), DuplicateElementException);
}

TYPED_TEST(BSTreeTest, RemoveAllCases) {
    //          50
    //       30      70
    //     20  40  60  80
    //            35 45
    auto t = tree_of<TypeParam>({50, 30, 70, 20, 40, 60, 80, 35, 45});

    t.remove(20); // лист
    EXPECT_FALSE(t.contains(20));
    EXPECT_TRUE(t.check_invariants());

    t.remove(30); // узел с одним ребёнком (после удаления 20) или двумя - в AVL структура иная
    EXPECT_FALSE(t.contains(30));
    EXPECT_TRUE(t.check_invariants());

    t.remove(50); // корень с двумя детьми
    EXPECT_FALSE(t.contains(50));
    EXPECT_TRUE(t.check_invariants());

    EXPECT_THROW(t.remove(50), ElementNotFoundException);
    EXPECT_EQ(t.get_count(), 6);

    MutableArraySequence<int> rest = t.to_sequence();
    int expected[] = {35, 40, 45, 60, 70, 80};
    ASSERT_EQ(rest.get_length(), 6);
    for (int i = 0; i < 6; i++)
        EXPECT_EQ(rest.get(i), expected[i]);
}

TYPED_TEST(BSTreeTest, ClearAndReuse) {
    auto t = tree_of<TypeParam>({3, 1, 2});
    t.clear();
    EXPECT_TRUE(t.is_empty());
    EXPECT_TRUE(t.check_invariants());
    t.insert(10);
    EXPECT_EQ(t.get_count(), 1);
}

// Случайные операции сверяем с эталоном - массивом флагов "есть ли число x"
TYPED_TEST(BSTreeTest, RandomOperationsMatchReference) {
    const int max_value = 500;
    TypeParam t;
    bool present[max_value + 1] = {};
    int reference_count = 0;
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> value(0, max_value);

    for (int step = 0; step < 5000; step++) {
        int x = value(rng);
        if (rng() % 3 == 0) {
            if (present[x]) {
                present[x] = false;
                reference_count--;
                t.remove(x);
            } else {
                EXPECT_THROW(t.remove(x), ElementNotFoundException);
            }
        } else {
            bool inserted = !present[x];
            if (inserted) {
                present[x] = true;
                reference_count++;
            }
            EXPECT_EQ(t.try_insert(x), inserted);
        }
        ASSERT_EQ(t.get_count(), reference_count);
        if (step % 250 == 0) {
            ASSERT_TRUE(t.check_invariants()) << "step " << step;
        }
    }

    EXPECT_TRUE(t.check_invariants());
    MutableArraySequence<int> items = t.to_sequence();
    int i = 0;
    for (int x = 0; x <= max_value; x++) {
        if (present[x]) {
            EXPECT_EQ(items.get(i++), x);
        }
    }
    EXPECT_EQ(i, items.get_length());
}

TYPED_TEST(BSTreeTest, CopyIsDeep) {
    auto a = tree_of<TypeParam>({5, 3, 8});
    TypeParam b = a;
    b.insert(10);
    b.remove(3);
    EXPECT_TRUE(a.contains(3));
    EXPECT_FALSE(a.contains(10));
    EXPECT_EQ(a.get_count(), 3);
    EXPECT_TRUE(a.check_invariants());
    EXPECT_TRUE(b.check_invariants());

    TypeParam c;
    c = a; // присваивание
    a.clear();
    EXPECT_EQ(c.get_count(), 3);
    TypeParam& same = c;
    c = same; // самоприсваивание
    EXPECT_EQ(c.get_count(), 3);
}

TYPED_TEST(BSTreeTest, MoveLeavesSourceEmpty) {
    auto a = tree_of<TypeParam>({1, 2, 3});
    TypeParam b = std::move(a);
    EXPECT_EQ(b.get_count(), 3);
    EXPECT_EQ(a.get_count(), 0);   // NOLINT: проверяем состояние после move
    EXPECT_TRUE(a.check_invariants());
    a.insert(7); // объект после move пригоден к использованию
    EXPECT_TRUE(a.contains(7));
}

TYPED_TEST(BSTreeTest, ConstructFromArray) {
    int raw[] = {4, 2, 6};
    TypeParam t(raw, 3);
    EXPECT_EQ(t.get_count(), 3);
    EXPECT_TRUE(TypeParam(nullptr, 0).is_empty());
    EXPECT_THROW(TypeParam(nullptr, 2), std::invalid_argument);
    EXPECT_THROW(TypeParam(raw, -1), std::invalid_argument);
}

TYPED_TEST(BSTreeTest, ConstructFromSequence) {
    int raw[] = {4, 2, 6};
    MutableArraySequence<int> seq(raw, 3);
    TypeParam t(seq);
    EXPECT_EQ(t.get_count(), 3);
    EXPECT_EQ(t.get_min(), 2);
}

// Отсортированный ввод: обычное BST вырождается, AVL - нет. Главное - никаких
// переполнений стека при копировании и удалении длинной "палки".
TYPED_TEST(BSTreeTest, SortedInputDoesNotCrash) {
    const int n = 20000;
    TypeParam t;
    for (int i = 0; i < n; i++)
        t.insert(i);
    TypeParam copy = t;
    EXPECT_EQ(copy.get_count(), n);
    EXPECT_TRUE(copy.check_invariants());
    EXPECT_GE(t.get_height(), static_cast<int>(std::log2(n)));
}

/*============ Специфика AVL ============*/

TEST(AvlTest, HeightIsLogarithmic) {
    const int n = 100000;
    AvlInt t;
    for (int i = 0; i < n; i++)
        t.insert(i); // худший случай для обычного BST
    EXPECT_TRUE(t.check_invariants());
    EXPECT_LE(t.get_height(), static_cast<int>(1.45 * std::log2(n + 2)));

    for (int i = 0; i < n; i += 2)
        t.remove(i);
    EXPECT_TRUE(t.check_invariants());
    EXPECT_EQ(t.get_count(), n / 2);
}

TEST(PlainTest, SortedInputDegenerates) {
    PlainInt t;
    for (int i = 0; i < 1000; i++)
        t.insert(i);
    EXPECT_EQ(t.get_height(), 1000); // список - причина, зачем нужна балансировка
}

/*============ Разные типы и компараторы ============*/

TEST(BSTreeTypes, Strings) {
    auto t = tree_of<BSTree<std::string>>({"pear", "apple", "fig"});
    EXPECT_EQ(t.get_min(), "apple");
    EXPECT_EQ(t.get_max(), "pear");
    EXPECT_TRUE(t.contains("fig"));
}

TEST(BSTreeTypes, DoublesWithDescendingComparator) {
    auto t = tree_of<BSTree<double, KeyGreater<double>>>({1.5, -2.0, 3.25});
    EXPECT_DOUBLE_EQ(t.get_min(), 3.25); // "минимум" по компаратору - наибольшее число
    EXPECT_TRUE(t.check_invariants());
}

TEST(BSTreeTypes, FindReturnsStoredElementByPartialKey) {
    BSTree<Person, PersonByIdLess> t;
    t.insert(Person(1, "Иванов", "Иван", "Иванович", 2005));
    t.insert(Person(2, "Петров", "Пётр", "", 2004));

    Person probe(2, "x", "x", "", 2000); // компаратор смотрит только на id
    EXPECT_EQ(t.find(probe).get_last_name(), "Петров");
    EXPECT_THROW(t.insert(Person(1, "Другой", "Человек", "", 2000)), DuplicateElementException);
}

TEST(BSTreeTypes, FunctionPointersWithCustomComparator) {
    using Fn = int (*)(int);
    struct FnLess {
        bool operator()(Fn a, Fn b) const { return std::less<Fn>()(a, b); } // порядок по адресу
    };
    Fn inc = [](int x) { return x + 1; };
    Fn dbl = [](int x) { return x * 2; };

    auto t = tree_of<BSTree<Fn, FnLess>>({inc, dbl});
    EXPECT_EQ(t.get_count(), 2);
    EXPECT_EQ(t.find(dbl)(5), 10);
}

/*============ map / where / reduce ============*/

TEST(MapWhereReduce, MapSameType) {
    auto t = tree_of<AvlInt>({1, 2, 3, 4});
    AvlInt doubled = t.map([](int x) { return x * 2; });
    MutableArraySequence<int> items = doubled.to_sequence();
    ASSERT_EQ(items.get_length(), 4);
    for (int i = 0; i < 4; i++)
        EXPECT_EQ(items.get(i), 2 * (i + 1));
    EXPECT_EQ(t.get_count(), 4); // исходное дерево не изменилось
}

TEST(MapWhereReduce, MapChangesOrder) {
    auto t = tree_of<AvlInt>({1, 2, 3});
    AvlInt negated = t.map([](int x) { return -x; }); // порядок разворачивается
    EXPECT_TRUE(negated.check_invariants());
    EXPECT_EQ(negated.get_min(), -3);
}

TEST(MapWhereReduce, MapToOtherType) {
    auto t = tree_of<AvlInt>({3, 10, 7});
    BSTree<std::string> s = t.map<std::string>([](int x) { return std::to_string(x); });
    EXPECT_EQ(s.get_count(), 3);
    EXPECT_EQ(s.get_min(), "10"); // строки сравниваются лексикографически
}

TEST(MapWhereReduce, MapMergesCollisions) {
    AvlInt t;
    for (int i = 0; i < 100; i++)
        t.insert(i);
    AvlInt remainders = t.map([](int x) { return x % 3; });
    EXPECT_EQ(remainders.get_count(), 3);
}

TEST(MapWhereReduce, MapOnEmpty) {
    AvlInt t;
    EXPECT_TRUE(t.map([](int x) { return x; }).is_empty());
}

TEST(MapWhereReduce, WhereFiltersAndBalances) {
    PlainInt t;
    for (int i = 0; i < 1000; i++)
        t.insert(i); // вырожденное исходное дерево
    PlainInt even = t.where([](int x) { return x % 2 == 0; });
    EXPECT_EQ(even.get_count(), 500);
    EXPECT_TRUE(even.check_invariants());
    EXPECT_LE(even.get_height(), 9); // результат идеально сбалансирован: ceil(log2(501))

    EXPECT_TRUE(t.where([](int) { return false; }).is_empty());
    EXPECT_EQ(t.where([](int) { return true; }).get_count(), 1000);
}

TEST(MapWhereReduce, WhereResultIsValidAvl) {
    AvlInt t;
    for (int i = 0; i < 1000; i++)
        t.insert(i);
    AvlInt big = t.where([](int x) { return x > 600; });
    EXPECT_TRUE(big.check_invariants()); // высоты узлов корректны => дальнейшие вставки работают
    big.insert(-1);
    big.remove(700);
    EXPECT_TRUE(big.check_invariants());
}

TEST(MapWhereReduce, ReduceExampleFromAssignment) {
    // ТЗ: reduce(f, [1, 2, 3], 4) = f(3, f(2, f(1, 4))) при f(x1, x2) = 2*x1 + 3*x2.
    // f(1, 4) = 14, f(2, 14) = 4 + 42 = 46, f(3, 46) = 6 + 138 = 144.
    // В ТЗ ответ 132 - арифметическая ошибка: в шаге 2*2 + 3*14 потеряно слагаемое 4.
    auto t = tree_of<AvlInt>({3, 1, 2});
    int result = t.reduce([](int x1, int x2) { return 2 * x1 + 3 * x2; }, 4);
    EXPECT_EQ(result, 144);
}

TEST(MapWhereReduce, ReduceEmptyReturnsInit) {
    AvlInt t;
    EXPECT_EQ(t.reduce([](int a, int b) { return a + b; }, 7), 7);
}

TEST(MapWhereReduce, ReduceToOtherType) {
    auto t = tree_of<AvlInt>({1, 2, 3});
    std::string joined = t.reduce(
        [](int x, std::string acc) { return acc + std::to_string(x); }, std::string("#"));
    EXPECT_EQ(joined, "#123");
}

TEST(FromSorted, RejectsUnsortedInput) {
    int raw[] = {1, 3, 2};
    MutableArraySequence<int> seq(raw, 3);
    EXPECT_THROW(AvlInt::from_sorted(seq), std::invalid_argument);

    int dup[] = {1, 1};
    MutableArraySequence<int> dups(dup, 2);
    EXPECT_THROW(AvlInt::from_sorted(dups), std::invalid_argument);
}

/*============ Тесты этапа 5: поддеревья ============*/

TYPED_TEST(BSTreeTest, SubtreeRootLeafAndInternal) {
    //          50
    //       30      70
    //     20  40  60  80
    auto t = tree_of<TypeParam>({50, 30, 70, 20, 40, 60, 80});

    // 1. Поддерево корня должно содержать все 7 элементов
    TypeParam sub_root = t.subtree(50);
    EXPECT_EQ(sub_root.get_count(), 7);
    EXPECT_TRUE(sub_root.check_invariants());
    EXPECT_TRUE(t.contains_subtree(sub_root));

    // 2. Поддерево внутреннего узла 30 (узлы 30, 20, 40)
    TypeParam sub_30 = t.subtree(30);
    EXPECT_EQ(sub_30.get_count(), 3);
    EXPECT_TRUE(sub_30.contains(30));
    EXPECT_TRUE(sub_30.contains(20));
    EXPECT_TRUE(sub_30.contains(40));
    EXPECT_FALSE(sub_30.contains(50));
    EXPECT_TRUE(sub_30.check_invariants());
    EXPECT_TRUE(t.contains_subtree(sub_30));

    // 3. Поддерево листа 80 (только 80)
    TypeParam sub_80 = t.subtree(80);
    EXPECT_EQ(sub_80.get_count(), 1);
    EXPECT_TRUE(sub_80.contains(80));
    EXPECT_TRUE(sub_80.check_invariants());
    EXPECT_TRUE(t.contains_subtree(sub_80));

    // 4. Несуществующий элемент -> ElementNotFoundException
    EXPECT_THROW(t.subtree(999), ElementNotFoundException);
}

TYPED_TEST(BSTreeTest, SubtreeIndependence) {
    auto t = tree_of<TypeParam>({50, 30, 70, 20, 40});
    TypeParam sub = t.subtree(30);

    // Модификация оригинала не затрагивает извлеченное поддерево
    t.remove(20);
    t.insert(35);
    EXPECT_FALSE(t.contains(20));
    EXPECT_TRUE(sub.contains(20));
    EXPECT_FALSE(sub.contains(35));
    EXPECT_EQ(sub.get_count(), 3);

    // Модификация копии не затрагивает оригинал
    sub.insert(25);
    EXPECT_FALSE(t.contains(25));
    EXPECT_TRUE(sub.contains(25));
}

TYPED_TEST(BSTreeTest, ContainsSubtreeEdgeCases) {
    TypeParam empty_tree;
    TypeParam single = tree_of<TypeParam>({10});
    auto t = tree_of<TypeParam>({50, 30, 70, 20, 40, 60, 80});

    // 1. Пустое поддерево содержится в любом дереве
    EXPECT_TRUE(empty_tree.contains_subtree(empty_tree));
    EXPECT_TRUE(single.contains_subtree(empty_tree));
    EXPECT_TRUE(t.contains_subtree(empty_tree));

    // 2. Непустое поддерево не содержится в пустом
    EXPECT_FALSE(empty_tree.contains_subtree(single));
    EXPECT_FALSE(empty_tree.contains_subtree(t));

    // 3. Дерево содержит само себя
    EXPECT_TRUE(single.contains_subtree(single));
    EXPECT_TRUE(t.contains_subtree(t));

    // 4. Дерево с несовпадающим корнем
    TypeParam alien = tree_of<TypeParam>({100, 90, 110});
    EXPECT_FALSE(t.contains_subtree(alien));

    // 5. Тот же корень поддерева, но другая структура потомков
    // В t узел 30 имеет детей 20 и 40. Создадим дерево с корнем 30, но без 20
    TypeParam modified_30 = tree_of<TypeParam>({30, 40});
    EXPECT_FALSE(t.contains_subtree(modified_30));

    // Или с другим потомком (30 -> 20, 45 вместо 40)
    TypeParam wrong_child_30 = tree_of<TypeParam>({30, 20, 45});
    EXPECT_FALSE(t.contains_subtree(wrong_child_30));
}

/*============ Тесты этапа 6: прошивка и TreeEnumerator ============*/

TYPED_TEST(BSTreeTest, ThreadingAndEnumeratorEmptyAndSingle) {
    TypeParam empty_tree;
    EXPECT_TRUE(empty_tree.is_threaded());
    empty_tree.thread();
    EXPECT_TRUE(empty_tree.is_threaded());

    auto* en_empty = empty_tree.get_enumerator();
    EXPECT_FALSE(en_empty->has_more_elements());
    EXPECT_THROW(en_empty->next(), std::out_of_range);
    delete en_empty;

    TypeParam single = tree_of<TypeParam>({42});
    EXPECT_FALSE(single.is_threaded());
    single.thread("КЛП");
    EXPECT_TRUE(single.is_threaded());

    auto* en_single = single.get_enumerator();
    EXPECT_TRUE(en_single->has_more_elements());
    EXPECT_EQ(en_single->next(), 42);
    EXPECT_FALSE(en_single->has_more_elements());
    EXPECT_THROW(en_single->next(), std::out_of_range);
    delete en_single;
}

TYPED_TEST(BSTreeTest, ThreadingAllSixOrdersMatchesToSequence) {
    auto t = tree_of<TypeParam>({50, 30, 70, 20, 40, 60, 80});
    const char* const orders[] = {"КЛП", "КПЛ", "ЛКП", "ЛПК", "ПКЛ", "ПЛК"};

    for (const char* order_name : orders) {
        TraversalOrder order = TraversalOrder::standard(order_name);
        auto expected = t.to_sequence(order);

        // 1. Проверяем ленивый итератор без предварительной прошивки
        auto* lazy_en = t.get_enumerator(order);
        for (int i = 0; i < expected.get_length(); i++) {
            ASSERT_TRUE(lazy_en->has_more_elements());
            EXPECT_EQ(lazy_en->next(), expected.get(i));
        }
        EXPECT_FALSE(lazy_en->has_more_elements());
        delete lazy_en;

        // 2. Прошиваем дерево в этом порядке
        t.thread(order);
        EXPECT_TRUE(t.is_threaded());

        // 3. Проверяем обход по прошитому дереву (по указателям next)
        auto* thread_en = t.get_enumerator();
        for (int i = 0; i < expected.get_length(); i++) {
            ASSERT_TRUE(thread_en->has_more_elements());
            EXPECT_EQ(thread_en->next(), expected.get(i));
        }
        EXPECT_FALSE(thread_en->has_more_elements());
        delete thread_en;
    }
}

TYPED_TEST(BSTreeTest, UnthreadAndInvalidateOnMutation) {
    auto t = tree_of<TypeParam>({50, 30, 70});
    t.thread("ЛКП");
    EXPECT_TRUE(t.is_threaded());

    // Ручной unthread
    t.unthread();
    EXPECT_FALSE(t.is_threaded());

    // Автоматическая инвалидация прошивки при вставке
    t.thread("ЛКП");
    EXPECT_TRUE(t.is_threaded());
    t.insert(20);
    EXPECT_FALSE(t.is_threaded());

    // Автоматическая инвалидация прошивки при удалении
    t.thread("ЛКП");
    EXPECT_TRUE(t.is_threaded());
    t.remove(20);
    EXPECT_FALSE(t.is_threaded());

    // Инвалидация при clear
    t.thread("ЛКП");
    EXPECT_TRUE(t.is_threaded());
    t.clear();
    EXPECT_TRUE(t.is_threaded()); // пустое дерево всегда threaded
}

TYPED_TEST(BSTreeTest, RemovesAnyPositionInThreadAndCanThreadAgain) {
    for (int removed : {1, 2, 3}) {
        auto tree = tree_of<TypeParam>({2, 1, 3});
        tree.thread("ЛКП");
        EXPECT_THROW(tree.remove(4), ElementNotFoundException);
        EXPECT_TRUE(tree.is_threaded());

        tree.remove(removed);
        EXPECT_FALSE(tree.is_threaded());
        EXPECT_TRUE(tree.check_invariants());
        EXPECT_FALSE(tree.contains(removed));
        EXPECT_EQ(tree.get_count(), 2);

        tree.thread("ЛКП");
        std::unique_ptr<IEnumerator<int>> values(tree.get_enumerator());
        EXPECT_EQ(values->next(), removed == 1 ? 2 : 1);
        EXPECT_EQ(values->next(), removed == 3 ? 2 : 3);
        EXPECT_FALSE(values->has_more_elements());
    }
}

/*============ Тесты этапа 7: пользовательские типы данных ============*/

TEST(CustomTypesTest, ComplexNumbers) {
    BSTree<Complex> tree;
    tree.insert(Complex(1.0, 2.0));
    tree.insert(Complex(-1.0, 3.0));
    tree.insert(Complex(2.0, -1.0));
    tree.insert(Complex(1.0, -1.0));

    EXPECT_EQ(tree.get_count(), 4);
    EXPECT_TRUE(tree.check_invariants());
    EXPECT_TRUE(tree.contains(Complex(1.0, 2.0)));
    EXPECT_FALSE(tree.contains(Complex(0.0, 0.0)));

    // Проверка сериализации/десериализации
    std::string text = tree.to_string("{К}(Л)[П]");
    auto restored = BSTree<Complex>::from_string(text, "{К}(Л)[П]");
    EXPECT_EQ(restored.get_count(), 4);
    EXPECT_TRUE(restored.contains(Complex(1.0, 2.0)));
    EXPECT_TRUE(restored.contains(Complex(-1.0, 3.0)));

    // Дерево с компаратором по модулю
    BSTree<Complex, ComplexAbsLess> abs_tree;
    abs_tree.insert(Complex(3.0, 4.0)); // abs = 5
    abs_tree.insert(Complex(1.0, 1.0)); // abs = sqrt(2)
    abs_tree.insert(Complex(0.0, 13.0)); // abs = 13
    EXPECT_EQ(abs_tree.get_min(), Complex(1.0, 1.0));
    EXPECT_EQ(abs_tree.get_max(), Complex(0.0, 13.0));
}

TEST(CustomTypesTest, PersonModel) {
    Person p1(101, "Иванов", "Иван", "Иванович", 2000);
    Person p2(102, "Петров", "Петр", "", 1999);
    Person p3(103, "Сидоров", "Алексей", "Сергеевич", 2001);

    BSTree<Person> tree;
    tree.insert(p1);
    tree.insert(p2);
    tree.insert(p3);

    EXPECT_EQ(tree.get_count(), 3);
    EXPECT_TRUE(tree.check_invariants());

    // Сериализация и восстановление
    std::string text = tree.to_string();
    auto restored = BSTree<Person>::from_string(text);
    EXPECT_EQ(restored.get_count(), 3);
    EXPECT_TRUE(restored.contains(p2));

    // Поиск по частичному ключу (по ID)
    BSTree<Person, PersonByIdLess> id_tree;
    id_tree.insert(p1);
    id_tree.insert(p2);

    Person dummy(102, "Любой", "Любой", "", 2000);
    const Person& found = id_tree.find(dummy);
    EXPECT_EQ(found.get_last_name(), "Петров");
    EXPECT_EQ(found.get_birth_year(), 1999);
}

TEST(CustomTypesTest, PolynomialFunctionModel) {
    // f(x) = 1 + 2x
    PolynomialFunction f{1.0, 2.0};
    EXPECT_DOUBLE_EQ(f(3.0), 7.0);

    // g(x) = 3 + x^2
    PolynomialFunction g{3.0, 0.0, 1.0};
    EXPECT_DOUBLE_EQ(g(2.0), 7.0);

    // (f o g)(x) = f(g(x)) = 1 + 2*(3 + x^2) = 7 + 2x^2
    PolynomialFunction fog = f.compose(g);
    EXPECT_DOUBLE_EQ(fog(0.0), 7.0);
    EXPECT_DOUBLE_EQ(fog(2.0), 15.0); // 7 + 2*4 = 15
    EXPECT_DOUBLE_EQ(fog.get_coeff(0), 7.0);
    EXPECT_DOUBLE_EQ(fog.get_coeff(1), 0.0);
    EXPECT_DOUBLE_EQ(fog.get_coeff(2), 2.0);

    // Дерево полиномов
    BSTree<PolynomialFunction> tree;
    tree.insert(f);
    tree.insert(g);
    tree.insert(fog);
    tree.insert(PolynomialFunction{0.0});

    EXPECT_EQ(tree.get_count(), 4);
    EXPECT_TRUE(tree.check_invariants());
    EXPECT_TRUE(tree.contains(fog));

    // Сериализация и восстановление
    std::string text = tree.to_string("{К}(Л)[П]");
    auto restored = BSTree<PolynomialFunction>::from_string(text, "{К}(Л)[П]");
    EXPECT_EQ(restored.get_count(), 4);
    EXPECT_TRUE(restored.contains(fog));
}

TEST(CustomTypesTest, CloseNumericKeysRemainDistinct) {
    const Complex a(0.0, 0.0), b(0.75e-9, 0.0), c(1.5e-9, 0.0);
    auto complex_tree = tree_of<BSTree<Complex>>({b, c, a});
    EXPECT_TRUE(complex_tree.check_invariants());
    EXPECT_EQ(complex_tree.get_count(), 3);
    EXPECT_LT(a, b);
    EXPECT_LT(b, c);
    EXPECT_NE(a, b);
    EXPECT_TRUE(a.is_approximately_equal(b));

    ComplexAbsLess less_by_abs;
    EXPECT_TRUE(less_by_abs(Complex(1.0), Complex(-1.0000000005)));

    const PolynomialFunction f{0.0}, g{0.75e-9}, h{1.5e-9};
    auto polynomial_tree = tree_of<BSTree<PolynomialFunction>>({g, h, f});
    EXPECT_TRUE(polynomial_tree.check_invariants());
    EXPECT_EQ(polynomial_tree.get_count(), 3);
    EXPECT_LT(f, g);
    EXPECT_LT(g, h);
    EXPECT_NE(f, g);
    EXPECT_TRUE(f.is_approximately_equal(g));

    const PolynomialFunction small_slope{0.0, 1e-20};
    EXPECT_EQ(small_slope.get_degree(), 1);
    EXPECT_DOUBLE_EQ(small_slope(2.0), 2e-20);
}

TEST(CustomTypesTest, NonfiniteNumericKeysAreOrdered) {
    const double inf = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_LT(Complex(1.0), Complex(inf));
    EXPECT_LT(Complex(inf), Complex(nan));
    EXPECT_FALSE(Complex(nan) < Complex(nan));
    EXPECT_LT(PolynomialFunction{inf}, PolynomialFunction{nan});
    EXPECT_FALSE((PolynomialFunction{nan} < PolynomialFunction{nan}));
}

TEST(CustomTypesTest, ComplexSerializationPreservesExponentsAndSignedZero) {
    const double subnormal = std::numeric_limits<double>::denorm_min();
    auto tree = tree_of<BSTree<Complex>>({
        Complex(1.0, 1e-20), Complex(-1e30, -1e-25), Complex(2.0, -0.0), Complex(3.0, subnormal)
    });
    auto restored = BSTree<Complex>::from_string(tree.to_string());
    EXPECT_EQ(restored.get_count(), 4);
    EXPECT_EQ(restored.find(Complex(1.0, 1e-20)), Complex(1.0, 1e-20));
    EXPECT_EQ(restored.find(Complex(-1e30, -1e-25)), Complex(-1e30, -1e-25));
    EXPECT_TRUE(std::signbit(restored.find(Complex(2.0, 0.0)).get_im()));
    EXPECT_EQ(restored.find(Complex(3.0, subnormal)).get_im(), subnormal);
    EXPECT_THROW(Serializer<Complex>::from_string("1e+5i"), ParseException);
}

TEST(CustomTypesTest, PersonSerializationEscapesNamesAndReadsLegacyRecords) {
    const Person escaped(101, R"(A:\B)", "C:D", R"(E\)", 2000);
    const Person legacy(102, R"(A\)", "C", "", 2001);
    auto tree = tree_of<BSTree<Person>>({escaped, legacy});
    auto restored = BSTree<Person>::from_string(tree.to_string());
    EXPECT_EQ(restored.find(escaped), escaped);
    EXPECT_EQ(restored.find(legacy), legacy);

    const std::string legacy_record = R"(102:A\:C::2001)";
    EXPECT_EQ(Serializer<Person>::from_string(legacy_record), legacy);
    EXPECT_EQ(Serializer<Person>::to_string(legacy), legacy_record);
    EXPECT_THROW(Serializer<Person>::from_string(R"(v2:101:A\q:C::2000)"), ParseException);
}

/*============ Тесты этапа 8: общие интерфейсы и операторы ============*/

TEST(InterfacesAndOperatorsTest, PolymorphicICollection) {
    // Проверка полиморфизма через ICollection<int>*
    ICollection<int>* col = new BSTree<int>();
    EXPECT_TRUE(col->is_empty());
    EXPECT_EQ(col->get_count(), 0);

    BSTree<int>* tree = dynamic_cast<BSTree<int>*>(col);
    ASSERT_NE(tree, nullptr);
    tree->insert(10);
    tree->insert(20);
    tree->insert(30);

    EXPECT_FALSE(col->is_empty());
    EXPECT_EQ(col->get_count(), 3);
    EXPECT_TRUE(col->contains(20));
    EXPECT_FALSE(col->contains(99));

    // Проверка интерфейса IEnumerable через базовый указатель ICollection*
    IEnumerator<int>* en = col->get_enumerator();
    int count = 0;
    while (en->has_more_elements()) {
        en->next();
        count++;
    }
    EXPECT_EQ(count, 3);
    delete en;

    col->clear();
    EXPECT_TRUE(col->is_empty());
    EXPECT_EQ(col->get_count(), 0);

    delete col;
}

TEST(InterfacesAndOperatorsTest, EqualityOperators) {
    BSTree<int> a;
    BSTree<int> b;
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);

    a.insert(10);
    a.insert(20);
    EXPECT_FALSE(a == b);
    EXPECT_TRUE(a != b);

    // b собираем в другом порядке, но с теми же элементами
    b.insert(20);
    b.insert(10);
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);

    b.insert(30);
    EXPECT_FALSE(a == b);
}

TEST(InterfacesAndOperatorsTest, EqualityIgnoresComparatorDirection) {
    struct DirectionLess {
        bool descending;
        bool operator()(int a, int b) const { return descending ? a > b : a < b; }
    };

    const int values[] = {2, 1, 3};
    BSTree<int, DirectionLess> ascending(values, 3, DirectionLess{false});
    BSTree<int, DirectionLess> descending(values, 3, DirectionLess{true});
    EXPECT_EQ(ascending, descending);
    EXPECT_EQ(descending, ascending);
    EXPECT_FALSE(ascending.contains_subtree(descending));

    descending.remove(3);
    descending.insert(4);
    EXPECT_NE(ascending, descending);
    EXPECT_NE(descending, ascending);
}

TEST(InterfacesAndOperatorsTest, EqualityAndSubtreeCompareFullPersonRecords) {
    using PersonTree = BSTree<Person, PersonByIdLess>;
    const Person root(20, "Ivanov", "Ivan", "", 2000);
    const Person child(10, "Petrov", "Petr", "", 2001);
    const Person changed_child(10, "Sidorov", "Petr", "", 2001);

    const auto original = tree_of<PersonTree>({root, child});
    const auto changed = tree_of<PersonTree>({root, changed_child});

    EXPECT_EQ(original.find(changed_child), child);
    EXPECT_NE(original, changed);
    EXPECT_NE(changed, original);
    EXPECT_FALSE(original.contains_subtree(changed));
    EXPECT_FALSE(original.contains_subtree(tree_of<PersonTree>({changed_child})));
    EXPECT_TRUE(original.contains_subtree(tree_of<PersonTree>({child})));
}

TEST(InterfacesAndOperatorsTest, PlusAndPlusAssignOperators) {
    BSTree<int> a;
    a.insert(10);
    a.insert(20);

    BSTree<int> b;
    b.insert(20);
    b.insert(30);

    // a + b (объединение)
    BSTree<int> c = a + b;
    EXPECT_EQ(c.get_count(), 3);
    EXPECT_TRUE(c.contains(10));
    EXPECT_TRUE(c.contains(20));
    EXPECT_TRUE(c.contains(30));

    // a += item
    a += 40;
    EXPECT_EQ(a.get_count(), 3);
    EXPECT_TRUE(a.contains(40));

    // a += b
    a += b;
    EXPECT_EQ(a.get_count(), 4);
    EXPECT_TRUE(a.contains(30));
}

TEST(InterfacesAndOperatorsTest, StreamOutputOperator) {
    BSTree<int> tree;
    tree.insert(2);
    tree.insert(1);
    tree.insert(3);

    std::ostringstream oss;
    oss << tree;
    // По умолчанию to_string() использует КЛП {К}(Л)[П]
    EXPECT_EQ(oss.str(), "{2}({1}()[])[{3}()[]]");
}
