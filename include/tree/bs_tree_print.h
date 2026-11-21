#pragma once

#include <ostream>
#include <string>

#include "bs_tree.h"

template<class T, class Compare, class Balance>
std::ostream& operator<<(std::ostream& os, const BSTree<T, Compare, Balance>& tree) {
    return os << tree.to_string();
}

namespace bst_tree_print_detail {

struct StructurePrinter {
    template<class T, class Compare, class Balance>
    static void print(const BSTree<T, Compare, Balance>& tree, std::ostream& os) {
        if (tree.root_ == nullptr) {
            os << "  (дерево пусто)\n";
            return;
        }
        print_node<T>(tree.root_, "", false, os);
    }

private:
    template<class T, class Node>
    static void print_node(const Node* node, const std::string& prefix, bool is_left, std::ostream& os) {
        bool is_root = prefix.empty();
        if (is_root) {
            os << "── " << Serializer<T>::to_string(node->value) << '\n';
        } else {
            os << prefix << (is_left ? "├── [L] " : "└── [R] ") << Serializer<T>::to_string(node->value) << '\n';
        }

        std::string next_prefix = prefix + (is_root ? "   " : (is_left ? "│   " : "    "));
        if (node->left != nullptr || node->right != nullptr) {
            if (node->left != nullptr) {
                print_node<T>(node->left, next_prefix, true, os);
            } else {
                os << next_prefix << "├── [L] (пусто)\n";
            }
            if (node->right != nullptr) {
                print_node<T>(node->right, next_prefix, false, os);
            } else {
                os << next_prefix << "└── [R] (пусто)\n";
            }
        }
    }
};

} // namespace bst_tree_print_detail

template<class T, class Compare, class Balance>
void print_structure(const BSTree<T, Compare, Balance>& tree, std::ostream& os) {
    bst_tree_print_detail::StructurePrinter::print(tree, os);
}
