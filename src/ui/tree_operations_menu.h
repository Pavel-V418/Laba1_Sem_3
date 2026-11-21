#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "tree_values.h"
#include "tree/bs_tree_print.h"

namespace tree_ui {

inline constexpr int max_generated_count = 10000;

template<class T>
void PrintTraversal(std::unique_ptr<IEnumerator<T>> values, int count) {
    constexpr int max_shown = 30;
    int shown = 0;
    while (shown < max_shown && values->has_more_elements()) {
        std::cout << ' ';
        PrintValue(values->next());
        ++shown;
    }
    if (shown == 0)
        std::cout << " (empty)";
    if (shown < count)
        std::cout << " ... (" << count - shown << " more)";
    std::cout << '\n';
}

template<class Tree>
void PrintValues(const Tree& tree) {
    using T = typename Tree::value_type;
    std::cout << "Elements in sorted order:";
    PrintTraversal(std::unique_ptr<IEnumerator<T>>(tree.get_enumerator("LKR")), tree.get_count());
}

template<class Tree>
void PrintStructure(const Tree& tree) {
    constexpr int max_drawn_nodes = 100;
    if (tree.get_count() > max_drawn_nodes) {
        std::cout << "Structure view is limited to " << max_drawn_nodes << " nodes.\n";
        return;
    }
    print_structure(tree, std::cout);
}

inline TraversalOrder ReadTraversalOrder() {
    constexpr const char* orders[] = {"KLR", "KRL", "LKR", "LRK", "RKL", "RLK"};
    std::cout << "Traversal (K = root, L = left, R = right)\n";
    for (int i = 0; i < 6; ++i)
        std::cout << i + 1 << ". " << orders[i] << '\n';
    std::cout << "Order: ";
    return TraversalOrder::standard(orders[ReadIntInRange(1, 6) - 1]);
}

template<class Tree>
void TraversalMenu(Tree& tree) {
    using T = typename Tree::value_type;
    std::cout << "\nTraversal and threading\n"
                 "1. Traverse in a selected order\n"
                 "2. Thread in a selected order\n"
                 "3. Traverse using current threading (sorted if unthreaded)\n"
                 "4. Unthread\n"
                 "0. Back\n"
                 "Choice: ";
    switch (ReadIntInRange(0, 4)) {
        case 1: {
            auto order = ReadTraversalOrder();
            auto values = std::unique_ptr<IEnumerator<T>>(tree.get_enumerator(order));
            std::cout << "Traversal:";
            PrintTraversal(std::move(values), tree.get_count());
            break;
        }
        case 2:
            tree.thread(ReadTraversalOrder());
            std::cout << (tree.is_empty() ? "Tree is empty.\n" : "Tree threaded.\n");
            break;
        case 3:
            std::cout << "Traversal:";
            PrintTraversal(std::unique_ptr<IEnumerator<T>>(tree.get_enumerator()), tree.get_count());
            break;
        case 4:
            tree.unthread();
            std::cout << "Threading cleared.\n";
            break;
    }
}

inline TraversalOrder ReadFormat() {
    std::cout << "Format (empty for {K}(L)[R]): ";
    std::string format = Trim(ReadLine());
    return format.empty() ? TraversalOrder::standard("KLR") : TraversalOrder::parse(format);
}

template<class Tree>
Tree ReadSerializedTree(const Tree& reference) {
    auto order = ReadFormat();
    std::cout << "Serialized tree (empty for an empty tree): ";
    Tree result = Tree::from_string(ReadLine(), order, reference.get_comparator());
    if constexpr (std::is_same_v<typename Tree::value_type, double>) {
        result.for_each([](double value) {
            if (!std::isfinite(value))
                throw std::invalid_argument("A finite real number is required");
        });
    }
    return result;
}

template<class Tree>
void SerializationMenu(Tree& tree) {
    std::cout << "\nSave or load a string\n"
                 "1. Save to string\n"
                 "2. Load string and replace tree\n"
                 "0. Back\n"
                 "Choice: ";
    switch (ReadIntInRange(0, 2)) {
        case 1: {
            auto order = ReadFormat();
            std::cout << "Serialized tree:\n" << tree.to_string(order) << '\n';
            break;
        }
        case 2:
            tree = ReadSerializedTree(tree);
            std::cout << "Loaded " << tree.get_count() << " elements.\n";
            break;
    }
}

template<class Tree>
Tree ReadOtherTree(const Tree& reference) {
    using T = typename Tree::value_type;
    std::cout << "Second tree\n1. Enter values manually\n2. Read a serialized tree\nChoice: ";
    if (ReadIntInRange(1, 2) == 2)
        return ReadSerializedTree(reference);

    std::cout << "Number of elements (0-" << max_generated_count << "): ";
    int count = ReadIntInRange(0, max_generated_count);
    Tree other(reference.get_comparator());
    while (other.get_count() < count) {
        if (!other.try_insert(ReadValue<T>("Value: ")))
            std::cout << "Element already exists.\n";
    }
    return other;
}

template<class Tree>
void ShowResult(Tree& tree, Tree result) {
    std::cout << "Result has " << result.get_count() << " elements.\n";
    PrintValues(result);
    std::cout << "Replace current tree with result? (y/n): ";
    if (ReadYesNo())
        tree = std::move(result);
}

template<class Tree>
void RelationsMenu(Tree& tree) {
    using T = typename Tree::value_type;
    std::cout << "\nSubtrees, merge and equality\n"
                 "1. Extract subtree by root\n"
                 "2. Check whether a subtree occurs\n"
                 "3. Merge with another tree\n"
                 "4. Compare with another tree\n"
                 "0. Back\n"
                 "Choice: ";
    switch (ReadIntInRange(0, 4)) {
        case 1: {
            Tree subtree = tree.subtree(ReadKey<T>("Subtree root: "));
            PrintStructure(subtree);
            ShowResult(tree, std::move(subtree));
            break;
        }
        case 2: {
            Tree other = ReadOtherTree(tree);
            std::cout << (tree.contains_subtree(other) ? "Subtree found.\n" : "Subtree not found.\n");
            break;
        }
        case 3:
            ShowResult(tree, tree + ReadOtherTree(tree));
            break;
        case 4: {
            Tree other = ReadOtherTree(tree);
            std::cout << (tree == other ? "Trees are equal.\n" : "Trees differ.\n");
            break;
        }
    }
}

template<class Tree>
void ShowReduction(const Tree& tree) {
    using T = typename Tree::value_type;
    if (tree.is_empty()) {
        std::cout << "Tree is empty.\n";
        return;
    }

    if constexpr (std::is_same_v<T, int>) {
        auto sum = tree.reduce([](int value, long long total) { return total + value; }, 0LL);
        std::cout << "Sum: " << sum;
    } else if constexpr (std::is_same_v<T, std::string>) {
        auto length = tree.reduce([](const T& value, std::size_t total) { return total + value.size(); },
                                  std::size_t{0});
        std::cout << "Total string bytes: " << length;
    } else if constexpr (std::is_same_v<T, Person>) {
        int earliest = tree.reduce([](const T& value, int year) {
            return value.get_birth_year() < year ? value.get_birth_year() : year;
        }, Person::MAX_BIRTH_YEAR);
        std::cout << "Earliest birth year: " << earliest;
    } else {
        T sum = tree.reduce([](const T& value, const T& total) { return total + value; }, T{});
        std::cout << "Sum: ";
        PrintValue(sum);
    }
    std::cout << '\n';
}

template<class Tree>
void FunctionalMenu(Tree& tree) {
    using T = typename Tree::value_type;
    std::cout << "\nMap, where and reduce\n"
                 "1. Map: replace a selected value\n"
                 "2. Where: keep an inclusive range\n"
                 "3. Reduce: summarize values\n"
                 "0. Back\n"
                 "Choice: ";
    switch (ReadIntInRange(0, 3)) {
        case 1: {
            T key = ReadKey<T>("Value to replace: ");
            tree.find(key); // проверяем, что выбранный ключ есть в дереве
            T replacement = ReadValue<T>("New value: ");
            const auto& less = tree.get_comparator();
            auto result = tree.template map<T>([&](const T& value) {
                return !less(key, value) && !less(value, key) ? replacement : value;
            }, less);
            ShowResult(tree, std::move(result));
            break;
        }
        case 2: {
            T first = ReadKey<T>("Lower bound: ");
            T last = ReadKey<T>("Upper bound: ");
            const auto& less = tree.get_comparator();
            if (less(last, first))
                throw std::invalid_argument("Upper bound is below the lower bound");
            ShowResult(tree, tree.where([&](const T& value) {
                return !less(value, first) && !less(last, value);
            }));
            break;
        }
        case 3:
            ShowReduction(tree);
            break;
    }
}

} // namespace tree_ui
