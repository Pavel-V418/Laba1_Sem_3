#pragma once

#include "../common/comparators.h"
#include "../common/pair.h"
#include "../LR2/mutableArraySequence.h"
#include "balance.h"
#include "serializer.h"
#include "traversal_order.h"
#include "tree_exceptions.h"

// Бинарное дерево поиска, хранящее элементы типа T (без повторов).

// Compare  - строгий слабый порядок: less(a, b) == true, если a "меньше" b.
// Balance  - стратегия балансировки: AvlBalance (по умолчанию) или NoBalance.

template<class T, class Compare = KeyLess<T>, class Balance = AvlBalance>
class BSTree {
    friend Balance; // стратегия меняет структуру дерева

public:
    using value_type = T; // Псевдоним типа элемента.
    // Через него внешний код, например tree_of в тестах, узнаёт тип элемента по типу дерева: typename Tree::value_type.

    explicit BSTree(Compare less = Compare())
        : root_(nullptr), count_(0), less_(less) {}

    // Элементы из обычного массива
    BSTree(const T* items, int count, Compare less = Compare()) //const T* items - указатель на первый элемент массива
        : BSTree(less) {
        if (count < 0 || (items == nullptr && count > 0))
            throw std::invalid_argument("BSTree: invalid array");
        for (int i = 0; i < count; i++)
            insert(items[i]);
    }

    // Элементы из sequence
    explicit BSTree(const Sequence<T>& items, Compare less = Compare())
        : BSTree(less) {
        for (int i = 0; i < items.get_length(); i++)
            insert(items.get(i));
    }

    // Глубокая копия
    BSTree(const BSTree& other)
        : root_(copy_tree(other.root_)), count_(other.count_), less_(other.less_) {}

    BSTree(BSTree&& other) noexcept
        : root_(other.root_), count_(other.count_), less_(other.less_) {
        other.root_ = nullptr;
        other.count_ = 0;
    }

    // copy-and-swap: строгая гарантия исключений и одна реализация на копию и перемещение (ПОЧИТАТЬ ПРО ЭТО)
    BSTree& operator=(BSTree other) noexcept {
        swap(other);
        return *this;
    }

    ~BSTree() {
        destroy(root_);
    }

    void swap(BSTree& other) noexcept {
        exchange(root_, other.root_);
        exchange(count_, other.count_);
        exchange(less_, other.less_);
    }

    int get_count() const { return count_; }
    bool is_empty() const { return count_ == 0; }
    const Compare& get_comparator() const { return less_; }

    // AVL хранит высоту в узлах => O(1); без балансировки - обход за O(n).
    int get_height() const {
        if (Balance::tracks_height)
            return root_ != nullptr ? root_->height : 0;
        return compute_height();
    }

    // O(h). Эквивалентный элемент уже есть => DuplicateElementException.
    void insert(const T& item) {
        if (!try_insert(item))
            throw DuplicateElementException();
    }

    // O(h). Вставляет, если эквивалентного элемента нет; возвращает, была ли вставка. (это мягкая версия, нужна для map)
    bool try_insert(const T& item) {
        Node* parent = nullptr;
        Node* current = root_;
        bool go_left = false;

        while (current != nullptr) {
            parent = current;
            if (less_(item, current->value)) { // элементы меньше - уходим влево
                current = current->left;
                go_left = true;
            } else if (less_(current->value, item)) { // уходим вправо
                current = current->right;
                go_left = false;
            } else { // такой элемент уже есть
                return false;
            }
        }

        Node* node = new Node(item, parent);
        if (parent == nullptr)
            root_ = node;
        else if (go_left)
            parent->left = node;
        else
            parent->right = node;
        count_++;

        Balance::after_insert(*this, node);
        return true;
    }

    bool contains(const T& item) const {
        return find_node(item) != nullptr;
    }

    // Хранимый элемент, эквивалентный item. Полезно, когда компаратор сравнивает часть
    // полей: find(Student с нужным id) вернёт полную запись. Нет такого => исключение.
    const T& find(const T& item) const {
        const Node* node = find_node(item);
        if (node == nullptr)
            throw ElementNotFoundException();
        return node->value;
    }

    // O(h). Нет элемента => ElementNotFoundException.
    void remove(const T& item) {
        Node* del_node = const_cast<Node*>(find_node(item));
        if (del_node == nullptr)
            throw ElementNotFoundException();

        Node* rebalance_from; // самый нижний узел с изменившимся поддеревом

        if (del_node->left == nullptr || del_node->right == nullptr) {
            // 0 или 1 ребёнок: ребёнок (или nullptr) встаёт на место del_node
            Node* child = del_node->left != nullptr ? del_node->left : del_node->right;
            rebalance_from = del_node->parent;
            replace_child(del_node->parent, del_node, child);
        } else {
            // 2 ребёнка: на место встаёт преемник s - минимум правого поддерева (у s нет левого ребёнка)
            Node* s = del_node->right;
            while (s->left != nullptr)
                s = s->left;

            if (s->parent != del_node) { // преемник далеко от del_node
                rebalance_from = s->parent;
                replace_child(s->parent, s, s->right); // вырезаем s, его правое поддерево поднимается
                s->right = del_node->right;
                s->right->parent = s;
            } else {
                rebalance_from = s; // s - правый ребёнок del_node, просто поднимается
            }

            replace_child(del_node->parent, del_node, s); // преемник на место del_node
            s->left = del_node->left; // забрал левое поддерево
            s->left->parent = s;
            s->height = del_node->height;
        }

        delete del_node;
        count_--;

        Balance::after_remove(*this, rebalance_from);
    }

    void clear() {
        destroy(root_);
        root_ = nullptr;
        count_ = 0;
    }

    const T& get_min() const {
        if (root_ == nullptr)
            throw EmptyTreeException();
        return leftmost(root_)->value;
    }

    const T& get_max() const {
        if (root_ == nullptr)
            throw EmptyTreeException();
        const Node* node = root_;
        while (node->right != nullptr)
            node = node->right;
        return node->value;
    }

    /*============ Обход по возрастанию (ЛКП: левое->корень->правое) ============*/
    // проходит все элементы по возрастанию и для каждого вызывает действие, переданное снаружи
    // action - любой объект, который можно вызывать как функцию (лямбда, обычная функция, функтор)
    template<class Action>
    void for_each(Action action) const {
        for (const Node* node = leftmost(root_); node != nullptr; node = successor(node))
            action(node->value);
    }

    // собираем элементы в MutArrSeq
    MutableArraySequence<T> to_sequence() const {
        MutableArraySequence<T> result;
        for_each([&result](const T& item) { result.append(item); });
        return result;
    }

    /*============ map / where / reduce ============*/

    // Новое дерево из f(a) для каждого элемента a. Тип результата может отличаться
    // f не обязана сохранять порядок, поэтому дерево строится заново вставками: O(n log n).
    // Если f склеивает элементы (f(a) эквивалентно f(b)), в результат попадает один из них
    template<class U = T, class CompareU = KeyLess<U>, class F>
    BSTree<U, CompareU, Balance> map(F f, CompareU less = CompareU()) const {
        BSTree<U, CompareU, Balance> result(less);
        for_each([&result, &f](const T& item) { result.try_insert(f(item)); });
        return result;
    }

    // Новое дерево из элементов, для которых pred(a) == true.
    // Отобранные элементы уже отсортированы => идеально сбалансированное дерево за O(n).
    template<class Predicate>
    BSTree where(Predicate pred) const {
        MutableArraySequence<T> selected;
        for_each([&selected, &pred](const T& item) {
            if (pred(item))
                selected.append(item);
        });
        return from_sorted(selected, less_);
    }

    // Свёртка по возрастанию элементов a1 < a2 < ... < an:
    // reduce(f, c) = f(an, f(a(n-1), ... f(a2, f(a1, c))))
    // Тип аккумулятора R может отличаться от T, f: (const T&, R) -> R.
    template<class F, class R>
    R reduce(F f, R init) const {
        R accumulator = init; // здесь копится результат
        for_each([&accumulator, &f](const T& item) { accumulator = f(item, accumulator); });
        return accumulator;
    }

    // Строит идеально сбалансированное дерево из строго возрастающей последовательности: O(n).
    // Сначала проверяет порядок (тоже O(n)); нарушен => std::invalid_argument.
    static BSTree from_sorted(const Sequence<T>& items, Compare less = Compare()) {
        for (int i = 1; i < items.get_length(); i++)
            if (!less(items.get(i - 1), items.get(i)))
                throw std::invalid_argument("from_sorted: items must be strictly increasing");

        BSTree result(less);
        result.root_ = build_balanced(items, 0, items.get_length() - 1, nullptr);
        result.count_ = items.get_length();
        return result;
    }

    /*============ Обходы по заданному порядку ============*/
    // Обход в порядке order (любой из 6: КЛП, КПЛ, ЛКП, ЛПК, ПКЛ, ПЛК), action(const T&).

    template<class Action>
    void traverse(const TraversalOrder& order, Action action) const {
        if (root_ == nullptr)
            return;

        MutableArraySequence<ConstFrame> stack;
        stack.append(ConstFrame(root_, 0));

        while (!stack.is_empty()) {
            int top = stack.get_length() - 1;
            ConstFrame frame = stack.get(top);
            if (frame.step == TraversalOrder::PARTS) {
                stack.remove_at(top);
                continue;
            }
            stack.set(ConstFrame(frame.node, frame.step + 1), top);

            switch (order.part(frame.step)) {
                case TraversalPart::Root:
                    action(frame.node->value);
                    break;
                case TraversalPart::Left:
                    if (frame.node->left != nullptr)
                        stack.append(ConstFrame(frame.node->left, 0));
                    break;
                case TraversalPart::Right:
                    if (frame.node->right != nullptr)
                        stack.append(ConstFrame(frame.node->right, 0));
                    break;
            }
        }
    }

    MutableArraySequence<T> to_sequence(const TraversalOrder& order) const {
        MutableArraySequence<T> result;
        traverse(order, [&result](const T& item) { result.append(item); });
        return result;
    }

    /*============ Сохранение в строку (этап 3) ============*/

    // По формат-строке: tree.to_string("{К}(Л)[П]") -> "{50}({30}()[])[{70}()[]]"
    // Пустое поддерево - пустые скобки, пустое дерево - пустая строка. O(n).
    std::string to_string(const std::string& format) const {
        return to_string(TraversalOrder::parse(format));
    }

    // По фиксированному обходу КЛП со скобками {К}(Л)[П]
    std::string to_string() const {
        return to_string(TraversalOrder::standard("КЛП"));
    }

    std::string to_string(const TraversalOrder& order) const {
        std::string out;
        if (root_ == nullptr)
            return out;

        MutableArraySequence<ConstFrame> stack;
        stack.append(ConstFrame(root_, 0));

        while (!stack.is_empty()) {
            int top = stack.get_length() - 1;
            ConstFrame frame = stack.get(top);

            if (frame.step == TraversalOrder::PARTS) {
                // узел записан целиком: закрываем скобку той части родителя, в которой он стоял
                stack.remove_at(top);
                if (!stack.is_empty()) {
                    int parent_index = stack.get_length() - 1;
                    ConstFrame parent = stack.get(parent_index);
                    out += order.close(parent.step);
                    stack.set(ConstFrame(parent.node, parent.step + 1), parent_index);
                }
                continue;
            }

            int step = frame.step;
            if (order.has_brackets(step))
                out += order.open(step);

            if (order.part(step) == TraversalPart::Root) {
                append_escaped(out, Serializer<T>::to_string(frame.node->value), order);
                if (order.has_brackets(step))
                    out += order.close(step);
                stack.set(ConstFrame(frame.node, step + 1), top);
                continue;
            }

            const Node* child = order.part(step) == TraversalPart::Left ? frame.node->left : frame.node->right;
            if (child == nullptr) {
                out += order.close(step);                    // пустое поддерево: "()"
                stack.set(ConstFrame(frame.node, step + 1), top);
            } else {
                stack.append(ConstFrame(child, 0));          // скобку закроем, когда ребёнок допишется
            }
        }
        return out;
    }

    /*============ Чтение из строки (этап 3) ============*/

    // Восстанавливает дерево ровно той структуры, что записана в строке.
    //   - строка не соответствует формату, значение не разбирается => ParseException
    //   - элементы нарушают порядок дерева поиска (или повторяются) => ParseException
    //   - для AVL: если записанная структура не сбалансирована, дерево перестраивается
    //     в сбалансированное из тех же элементов (порядок обхода тогда изменится).
    static BSTree from_string(const std::string& text, const std::string& format, Compare less = Compare()) {
        return from_string(text, TraversalOrder::parse(format), less);
    }

    // По фиксированному обходу КЛП со скобками {К}(Л)[П]
    static BSTree from_string(const std::string& text, Compare less = Compare()) {
        return from_string(text, TraversalOrder::standard("КЛП"), less);
    }

    static BSTree from_string(const std::string& text, const TraversalOrder& order, Compare less = Compare()) {
        BSTree result(less);
        if (text.empty())
            return result;

        int count = 0;
        result.root_ = parse_nodes(text, order, count);
        result.count_ = count;

        if (!result.is_ordered())
            throw ParseException("elements violate the search tree order (or repeat)");
        if (!result.check_invariants())                      // структура не AVL - перестраиваем
            result = from_sorted(result.to_sequence(), less);
        return result;
    }

    /*============ Проверка структуры (для тестов) ============*/

    // Порядок элементов, ссылки на родителей, число узлов и инвариант стратегии (AVL).
    bool check_invariants() const {
        if (root_ != nullptr && root_->parent != nullptr)
            return false;

        int visited = 0;
        const Node* previous = nullptr;
        for (const Node* node = leftmost(root_); node != nullptr; node = successor(node)) {
            if (node->left != nullptr && node->left->parent != node)
                return false;
            if (node->right != nullptr && node->right->parent != node)
                return false;
            if (previous != nullptr && !less_(previous->value, node->value))
                return false; // ЛКП обязан давать строго возрастающую последовательность
            if (!Balance::check_node(node))
                return false;
            previous = node;
            visited++;
        }
        return visited == count_;
    }

private:
    struct Node {
        T value;
        Node* left;
        Node* right;
        Node* parent;
        int height; // высота поддерева с корнем в этом узле. поддерживается только у AVL

        Node(const T& value, Node* parent) // всегда создаёт лист
            : value(value), left(nullptr), right(nullptr), parent(parent), height(1) {}
    };

    Node* root_;
    int count_; // число элементов
    Compare less_; // компаратор

    // Своя замена std::swap
    template<class X>
    static void exchange(X& a, X& b) noexcept {
        X temp = a;
        a = b;
        b = temp;
    }

    const Node* find_node(const T& item) const {
        const Node* current = root_;
        while (current != nullptr) {
            if (less_(item, current->value))
                current = current->left;
            else if (less_(current->value, item))
                current = current->right;
            else
                return current;
        }
        return nullptr;
    }

    // Ставит new_child на место old_child у parent (или в корень, если parent == nullptr)
    void replace_child(Node* parent, Node* old_child, Node* new_child) {
        if (parent == nullptr)
            root_ = new_child;
        else if (parent->left == old_child)
            parent->left = new_child;
        else
            parent->right = new_child;

        if (new_child != nullptr)
            new_child->parent = parent;
    }

    static const Node* leftmost(const Node* node) { // минимум поддерева
        if (node == nullptr)
            return nullptr;
        while (node->left != nullptr)
            node = node->left;
        return node;
    }

    // Следующий по возрастанию узел
    static const Node* successor(const Node* node) {
        if (node->right != nullptr)
            return leftmost(node->right);

        // поднимаемся, пока приходим из правого поддерева
        const Node* parent = node->parent;
        while (parent != nullptr && node == parent->right) {
            node = parent;
            parent = parent->parent;
        }
        return parent;
    }

    // Кадр явного стека для обходов: узел и номер следующей части порядка (0..3)
    struct ConstFrame {
        const Node* node;
        int step;
        ConstFrame() : node(nullptr), step(0) {}
        ConstFrame(const Node* node, int step) : node(node), step(step) {}
    };

    struct BuildFrame {
        Node* node;
        int step;
        BuildFrame() : node(nullptr), step(0) {}
        BuildFrame(Node* node, int step) : node(node), step(step) {}
    };

    // Строго ли возрастает обход ЛКП (порядок дерева поиска, без повторов)
    bool is_ordered() const {
        const Node* previous = nullptr;
        for (const Node* node = leftmost(root_); node != nullptr; node = successor(node)) {
            if (previous != nullptr && !less_(previous->value, node->value))
                return false;
            previous = node;
        }
        return true;
    }

    static void append_escaped(std::string& out, const std::string& value, const TraversalOrder& order) {
        for (char c : value) {
            if (order.is_special(c))
                out += TraversalOrder::ESCAPE;
            out += c;
        }
    }

    static std::string position_message(const char* what, std::size_t pos) {
        return std::string(what) + " at position " + std::to_string(pos);
    }

    static void expect(const std::string& text, std::size_t& pos, char expected) {
        if (pos >= text.size())
            throw ParseException(position_message(
                (std::string("unexpected end, expected '") + expected + "'").c_str(), pos));
        if (text[pos] != expected)
            throw ParseException(position_message(
                (std::string("expected '") + expected + "', got '" + text[pos] + "'").c_str(), pos));
        pos++;
    }

    // Значение узла: до первого неэкранированного символа скобок
    static std::string read_value(const std::string& text, std::size_t& pos, const TraversalOrder& order) {
        std::string value;
        while (pos < text.size()) {
            char c = text[pos];
            if (c == TraversalOrder::ESCAPE) {
                if (pos + 1 >= text.size())
                    throw ParseException(position_message("dangling escape character", pos));
                value += text[pos + 1];
                pos += 2;
                continue;
            }
            if (order.is_special(c))
                break;
            value += c;
            pos++;
        }
        return value;
    }

    // Разбор непустой строки в узлы. Итеративно (зеркально to_string): кадр - строящийся узел
    // и номер части. Ребёнок прикрепляется к родителю, только когда разобран целиком, поэтому
    // при ошибке на стеке лежат непересекающиеся поддеревья - их и освобождаем.
    static Node* parse_nodes(const std::string& text, const TraversalOrder& order, int& count) {
        MutableArraySequence<BuildFrame> stack;
        Node* root = new Node(T(), nullptr);
        count = 1;
        std::size_t pos = 0;

        try {
            stack.append(BuildFrame(root, 0));

            while (!stack.is_empty()) {
                int top = stack.get_length() - 1;
                BuildFrame frame = stack.get(top);

                if (frame.step == TraversalOrder::PARTS) {
                    update_height(frame.node);               // дети уже готовы
                    stack.remove_at(top);
                    if (!stack.is_empty()) {
                        int parent_index = stack.get_length() - 1;
                        BuildFrame parent = stack.get(parent_index);
                        expect(text, pos, order.close(parent.step));
                        if (order.part(parent.step) == TraversalPart::Left)
                            parent.node->left = frame.node;
                        else
                            parent.node->right = frame.node;
                        frame.node->parent = parent.node;
                        stack.set(BuildFrame(parent.node, parent.step + 1), parent_index);
                    }
                    continue;
                }

                int step = frame.step;
                if (order.has_brackets(step))
                    expect(text, pos, order.open(step));

                if (order.part(step) == TraversalPart::Root) {
                    std::size_t value_pos = pos;
                    std::string raw = read_value(text, pos, order);
                    try {
                        frame.node->value = Serializer<T>::from_string(raw);
                    } catch (const ParseException& e) {
                        throw ParseException(position_message(e.detail().c_str(), value_pos));
                    }
                    if (order.has_brackets(step))
                        expect(text, pos, order.close(step));
                    stack.set(BuildFrame(frame.node, step + 1), top);
                } else if (pos < text.size() && text[pos] == order.close(step)) {
                    pos++;                                   // пустое поддерево
                    stack.set(BuildFrame(frame.node, step + 1), top);
                } else {
                    stack.append(BuildFrame(new Node(T(), nullptr), 0));
                    count++;
                }
            }

            if (pos != text.size())
                throw ParseException(position_message("unexpected characters after the tree", pos));
        } catch (...) {
            for (int i = 0; i < stack.get_length(); i++)
                if (stack.get(i).node != root)
                    destroy(stack.get(i).node);
            destroy(root);
            throw;
        }
        return root;
    }

    static void update_height(Node* node) {
        int left_height = node->left != nullptr ? node->left->height : 0;
        int right_height = node->right != nullptr ? node->right->height : 0;
        node->height = 1 + (left_height > right_height ? left_height : right_height);
    }

    // Итеративный подсчёт высоты: стек пар (узел, глубина)
    int compute_height() const {
        if (root_ == nullptr)
            return 0;

        MutableArraySequence<Pair<const Node*, int>> stack;
        stack.append(Pair<const Node*, int>(root_, 1));
        int result = 0;

        while (!stack.is_empty()) {
            Pair<const Node*, int> top = stack.get_last();
            stack.remove_at(stack.get_length() - 1);

            if (top.value > result)
                result = top.value;
            if (top.key->left != nullptr)
                stack.append(Pair<const Node*, int>(top.key->left, top.value + 1));
            if (top.key->right != nullptr)
                stack.append(Pair<const Node*, int>(top.key->right, top.value + 1));
        }
        return result;
    }

    // Рекурсия глубиной O(log n) - для сбалансированной постройки это безопасно
    static Node* build_balanced(const Sequence<T>& items, int lo, int hi, Node* parent) {
        if (lo > hi)
            return nullptr;

        int mid = lo + (hi - lo) / 2;
        Node* node = new Node(items.get(mid), parent);
        try {
            node->left = build_balanced(items, lo, mid - 1, node);
            node->right = build_balanced(items, mid + 1, hi, node);
        } catch (...) {
            destroy(node);
            throw;
        }

        update_height(node);
        return node;
    }

    static Node* clone_node(const Node* source, Node* parent) {
        Node* node = new Node(source->value, parent);
        node->height = source->height;
        return node;
    }

    // Итеративное копирование: стек пар (узел-оригинал, его копия)
    static Node* copy_tree(const Node* source_root) {
        if (source_root == nullptr)
            return nullptr;

        Node* new_root = clone_node(source_root, nullptr);
        try {
            MutableArraySequence<Pair<const Node*, Node*>> stack;
            stack.append(Pair<const Node*, Node*>(source_root, new_root));

            while (!stack.is_empty()) {
                Pair<const Node*, Node*> top = stack.get_last();
                stack.remove_at(stack.get_length() - 1);
                const Node* source = top.key;
                Node* copy = top.value;

                if (source->left != nullptr) {
                    copy->left = clone_node(source->left, copy);
                    stack.append(Pair<const Node*, Node*>(source->left, copy->left));
                }
                if (source->right != nullptr) {
                    copy->right = clone_node(source->right, copy);
                    stack.append(Pair<const Node*, Node*>(source->right, copy->right));
                }
            }
        } catch (...) {
            destroy(new_root); // не оставляем утечку из недостроенной копии
            throw;
        }
        return new_root;
    }

    // Удаление без доп. памяти: правыми поворотами вытягиваем дерево в "палку" вправо
    // и удаляем её узлы по одному. Каждый узел поворачивается не больше раза => O(n).
    static void destroy(Node* node) {
        while (node != nullptr) {
            if (node->left != nullptr) {
                Node* left = node->left;
                node->left = left->right;
                left->right = node;
                node = left;
            } else {
                Node* right = node->right;
                delete node;
                node = right;
            }
        }
    }
};
