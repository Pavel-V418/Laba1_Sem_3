#pragma once

// Стратегии балансировки для BSTree<T, Compare, Balance>.
// Стратегия - класс со статическими шаблонными методами, которые дерево вызывает после изменения структуры.

// Стратегия - friend дерева, поэтому может пользоваться tree.replace_child и полями узлов.
// Обычное (несбалансированное) дерево поиска: на отсортированных данных вырождается в список.
// struct: закрытых частей нет, только пустые реализации интерфейса.

// В NoBalance поле height есть, но не обновляется: это 4 байта на узел ради единого типа Node.

struct NoBalance {
    static constexpr bool tracks_height = false;
    template<class Tree, class Node>
    static void after_insert(Tree&, Node*) {}

    template<class Tree, class Node>
    static void after_remove(Tree&, Node*) {}

    template<class Node>
    static bool check_node(const Node*) { return true; }
};

// AVL-дерево: для каждого узла |h(left) - h(right)| <= 1  =>  высота <= 1.44 * log2(n + 2).
// class, а не struct: снаружи доступен только интерфейс стратегии, повороты скрыты.
class AvlBalance {
public:
    static constexpr bool tracks_height = true; // поддерживает ли node->height

    template<class Tree, class Node>
    static void after_insert(Tree& tree, Node* node) { // подъём от родителя до нового листа
        rebalance_up(tree, node->parent); // сам лист сбалансирован (высота 1)
    }

    template<class Tree, class Node>
    static void after_remove(Tree& tree, Node* node) { // подъём от узла, который передал remove
        rebalance_up(tree, node);
    }

    template<class Node>
    static bool check_node(const Node* node) { // проверяет, правильно ли у одного узла записана высота и сбалансирован ли он
        int expected = 1 + max_of(height(node->left), height(node->right)); // какая высота должна быть у тек узла
        int balance = balance_factor(node);
        return node->height == expected && balance >= -1 && balance <= 1;
    }

private:
    template<class Node>
    static int height(const Node* node) {
        return node != nullptr ? node->height : 0;
    }

    static int max_of(int a, int b) { // замена std::max
        return a > b ? a : b;
    }

    template<class Node>
    static void update_height(Node* node) { // пересчёт высоты узла по высотам детей
        node->height = 1 + max_of(height(node->left), height(node->right));
    }

    // > 0 - перевес влево, < 0 - вправо
    template<class Node>
    static int balance_factor(const Node* node) { // перевес
        return height(node->left) - height(node->right);
    }

    // Левый поворот вокруг x (y = x->right поднимается на место x):
    template<class Tree, class Node>
    static Node* rotate_left(Tree& tree, Node* x) {
        Node* y = x->right;
        x->right = y->left;
        if (y->left != nullptr)
            y->left->parent = x;

        tree.replace_child(x->parent, x, y); // y встаёт на место x
        y->left = x;
        x->parent = y;

        update_height(x); // сначала x - он теперь ниже
        update_height(y);
        return y;
    }

    // зеркально rotate_left
    template<class Tree, class Node>
    static Node* rotate_right(Tree& tree, Node* x) {
        Node* y = x->left;
        x->left = y->right;
        if (y->right != nullptr)
            y->right->parent = x;

        tree.replace_child(x->parent, x, y);
        y->right = x;
        x->parent = y;

        update_height(x);
        update_height(y);
        return y;
    }

    // Балансирует один узел; возвращает узел, который теперь стоит на его месте
    template<class Tree, class Node>
    static Node* rebalance(Tree& tree, Node* node) {
        update_height(node);
        int balance = balance_factor(node);

        if (balance > 1) {                                   // перевес влево
            if (balance_factor(node->left) < 0)              // случай "лево-право"
                rotate_left(tree, node->left);
            return rotate_right(tree, node);
        }
        if (balance < -1) {                                  // перевес вправо
            if (balance_factor(node->right) > 0)             // случай "право-лево"
                rotate_right(tree, node->right);
            return rotate_left(tree, node);
        }
        return node;
    }

    // Подъём до корня: O(log n) узлов, O(1) работы на каждом
    template<class Tree, class Node>
    static void rebalance_up(Tree& tree, Node* node) {
        while (node != nullptr) {
            node = rebalance(tree, node);
            node = node->parent;
        }
    }
};
