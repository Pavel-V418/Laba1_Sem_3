// ЛР-1, этапы 1-2, 4: BSTree<T> - базовые операции, AVL-балансировка, map / where / reduce
#include <gtest/gtest.h>

#include <cmath>
#include <random>
#include <initializer_list>
#include <string>

#include "tree/bs_tree.h"
#include "model/person.h"

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
