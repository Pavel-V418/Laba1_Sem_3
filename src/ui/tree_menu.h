#pragma once

#include <iostream>
#include <random>
#include <utility>

#include "tree_operations_menu.h"

namespace tree_ui {

template<class Tree>
void GenerateTree(Tree& tree) {
    using T = typename Tree::value_type;
    std::cout << "Number of elements (0-" << max_generated_count << "): ";
    int count = ReadIntInRange(0, max_generated_count);
    std::cout << "Insertion order\n1. Random\n2. Ascending\n3. Descending\nChoice: ";
    int order = ReadIntInRange(1, 3);

    MutableArraySequence<int> keys;
    for (int i = 0; i < count; ++i)
        keys.append(order == 3 ? count - 1 - i : i);

    if (order == 1) {
        std::cout << "Seed (empty for 42): ";
        std::mt19937 rng(static_cast<unsigned>(ReadIntOrDefault(42)));
        for (int i = count - 1; i > 0; --i) {
            int j = std::uniform_int_distribution<int>(0, i)(rng);
            int key = keys.get(i);
            keys.set(keys.get(j), i);
            keys.set(key, j);
        }
    }

    Tree generated(tree.get_comparator());
    for (int i = 0; i < count; ++i)
        generated.insert(GeneratedValue<T>(keys.get(i)));
    tree = std::move(generated);
    std::cout << "Generated " << tree.get_count() << " elements.\n";
}

template<class Tree>
void TreeMenu(Tree& tree, const char* type_name, const char* balance_name) {
    using T = typename Tree::value_type;
    while (true) {
        std::cout << "\n=== " << type_name << " / " << balance_name << " | count " << tree.get_count()
                  << " | height " << tree.get_height() << " ===\n"
                     "1. Insert manually\n"
                     "2. Generate and replace tree\n"
                     "3. Remove\n"
                     "4. Find\n"
                     "5. Show sorted elements\n"
                     "6. Show tree structure\n"
                     "7. Clear tree\n"
                     "8. Traversal and threading\n"
                     "9. Save or load a string\n"
                     "10. Subtrees, merge and equality\n"
                     "11. Map, where and reduce\n"
                     "0. Close tree\n"
                     "Choice: ";
        int choice = ReadIntInRange(0, 11);
        try {
            switch (choice) {
                case 0:
                    return;
                case 1:
                    tree.insert(ReadValue<T>("Value to insert: "));
                    std::cout << "Inserted.\n";
                    break;
                case 2:
                    GenerateTree(tree);
                    break;
                case 3:
                    tree.remove(ReadKey<T>("Value to remove: "));
                    std::cout << "Removed.\n";
                    break;
                case 4: {
                    const T& found = tree.find(ReadKey<T>("Value to find: "));
                    std::cout << "Found: ";
                    PrintValue(found);
                    std::cout << '\n';
                    break;
                }
                case 5:
                    PrintValues(tree);
                    break;
                case 6:
                    PrintStructure(tree);
                    break;
                case 7:
                    tree.clear();
                    std::cout << "Tree cleared.\n";
                    break;
                case 8:
                    TraversalMenu(tree);
                    break;
                case 9:
                    SerializationMenu(tree);
                    break;
                case 10:
                    RelationsMenu(tree);
                    break;
                case 11:
                    FunctionalMenu(tree);
                    break;
            }
        } catch (const InputClosedException&) {
            throw;
        } catch (const std::exception& error) {
            std::cout << "Error: " << error.what() << '\n';
        }
    }
}

template<class T, class Compare = KeyLess<T>>
void ChooseBalance(const char* type_name) {
    std::cout << "\n=== " << type_name << " ===\n"
                 "1. Create AVL tree\n"
                 "2. Create BST without balancing\n"
                 "0. Back\n"
                 "Choice: ";
    int choice = ReadIntInRange(0, 2);
    if (choice == 1) {
        BSTree<T, Compare, AvlBalance> tree;
        TreeMenu(tree, type_name, "AVL");
    } else if (choice == 2) {
        BSTree<T, Compare, NoBalance> tree;
        TreeMenu(tree, type_name, "BST");
    }
}

} // namespace tree_ui
