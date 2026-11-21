#pragma once

#include "../common/comparators.h"
#include "../common/pair.h"
#include "../LR2/mutableArraySequence.h"
#include "../LR2/i_collection.h"
#include "../LR2/i_enumerator.h"
#include "balance.h"
#include "serializer.h"
#include "traversal_order.h"
#include "tree_exceptions.h"

namespace bst_tree_print_detail {
struct StructurePrinter;
}

// Бинарное дерево поиска, хранящее элементы типа T (без повторов).

// Compare - строгий слабый порядок: less(a, b) == true, если a "меньше" b.
// Balance - стратегия балансировки: AvlBalance (по умолчанию) или NoBalance.

template<class T, class Compare = KeyLess<T>, class Balance = AvlBalance>
class BSTree : public ICollection<T> {
    friend Balance; // стратегия меняет структуру дерева (нужно, например, при поворотах - меняют связи узлов и вызывают replace_child)
    friend struct bst_tree_print_detail::StructurePrinter;

private:
    struct Node {
        T value;
        Node* left;
        Node* right;
        Node* parent;
        Node* next; // указатель прошивки на следующий узел в порядке обхода
        int height; // высота поддерева с корнем в этом узле. поддерживается только у AVL

        Node(const T& value, Node* parent) // всегда создаёт лист
            : value(value), left(nullptr), right(nullptr), parent(parent), next(nullptr), height(1) {}
    };

    // Кадр (замороженный рекурсивный вызов) явного стека для обходов: узел и номер следующей части порядка (0..3)
    struct ConstFrame {
        const Node* node; // какой узел обрабатывается
        int step; // какой шаг выполнить следующим
        ConstFrame() : node(nullptr), step(0) {}
        ConstFrame(const Node* node, int step) : node(node), step(step) {}
    };

    template<class NodePtr, class Action>
    static void traverse_nodes_impl(NodePtr root, const TraversalOrder& order, Action action) { // при обходе передавать адрес самого узла
        if (root == nullptr)
            return;

        struct Frame {
            NodePtr node;
            int step;
            Frame() : node(nullptr), step(0) {}
            Frame(NodePtr n, int s) : node(n), step(s) {}
        };

        MutableArraySequence<Frame> stack;
        stack.append(Frame(root, 0));

        while (!stack.is_empty()) {
            int top = stack.get_length() - 1;
            Frame frame = stack.get(top);
            if (frame.step == TraversalOrder::PARTS) {
                stack.remove_at(top);
                continue;
            }
            stack.set(Frame(frame.node, frame.step + 1), top);

            switch (order.part(frame.step)) {
                case TraversalPart::Root:
                    action(frame.node);
                    break;
                case TraversalPart::Left:
                    if (frame.node->left != nullptr)
                        stack.append(Frame(frame.node->left, 0));
                    break;
                case TraversalPart::Right:
                    if (frame.node->right != nullptr)
                        stack.append(Frame(frame.node->right, 0));
                    break;
            }
        }
    }

    template<class Action>
    void traverse_nodes(const TraversalOrder& order, Action action) { // обёртка - передаёт корень тек дерева, порядок и действие
        traverse_nodes_impl(root_, order, action);
    }

public:
    using value_type = T; // Псевдоним типа элемента.
    // Через него внешний код, например tree_of в тестах, узнаёт тип элемента по типу дерева: typename Tree::value_type.

    explicit BSTree(Compare less = Compare())
        : root_(nullptr), threaded_head_(nullptr), count_(0), less_(less) {}

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

    // Глубокая копия (если бы скопировали только указатель корня, то 2 дерева делили бы 1 узлы, и втрой дестр. кдалил бы их повторно)
    BSTree(const BSTree& other)
        : root_(copy_tree(other.root_)), threaded_head_(nullptr), count_(other.count_), less_(other.less_) {}

    BSTree(BSTree&& other) noexcept
        : root_(other.root_), threaded_head_(other.threaded_head_), count_(other.count_), less_(other.less_) {
        other.root_ = nullptr;
        other.threaded_head_ = nullptr;
        other.count_ = 0;
    }

    // copy-and-swap: строгая гарантия исключений и одна реализация на копию и перемещение
    BSTree& operator=(BSTree other) noexcept {
        swap(other); // если при копировании не хватит памяти, то дело до swap не дойдет
        return *this;
    }

    ~BSTree() {
        destroy(root_);
    }

    void swap(BSTree& other) noexcept { // noexcept, тк swap используется в присваивании через copy-and-swap
        exchange(root_, other.root_);
        exchange(threaded_head_, other.threaded_head_);
        exchange(count_, other.count_);
        exchange(less_, other.less_);
    }

    int get_count() const override { return count_; }
    bool is_empty() const override { return count_ == 0; }
    const Compare& get_comparator() const { return less_; }

    // AVL хранит высоту в узлах => O(1); без балансировки - обход за O(n).
    int get_height() const {
        if (Balance::tracks_height) // вычисляется компилятором за счёт constexpr в tracks_height
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
        unthread();

        Balance::after_insert(*this, node);
        return true;
    }

    bool contains(const T& item) const override {
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

    // O(h), плюс O(n) для сброса прошивки. Нет элемента => ElementNotFoundException.
    void remove(const T& item) {
        Node* del_node = find_node(item);
        if (del_node == nullptr)
            throw ElementNotFoundException();

        unthread(); // цепочка next ещё содержит удаляемый узел
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

    void clear() override {
        destroy(root_);
        root_ = nullptr;
        threaded_head_ = nullptr;
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

    /*============ Поддеревья ============*/

    // Извлечение поддерева по заданному элементу: O(h + k), где k - размер поддерева.
    // Возвращает новое независимое дерево (глубокая копия ветви).
    BSTree subtree(const T& item) const {
        const Node* target = find_node(item);
        if (target == nullptr)
            throw ElementNotFoundException();

        BSTree result(less_);
        result.root_ = copy_tree(target);
        result.count_ = count_subtree(result.root_);
        return result;
    }

    // Проверка на вхождение поддерева: O(h + k), где h - высота текущего дерева, k - размер other.
    // Пустое дерево содержится в любом дереве (в т.ч. в пустом).
    // Непустое дерево не может содержаться в пустом.
    // Сравнивает структуру и полные значения узлов через operator== типа T.
    bool contains_subtree(const BSTree& other) const {
        if (other.is_empty())
            return true;
        if (is_empty())
            return false;

        const Node* target = find_node(other.root_->value);
        if (target == nullptr)
            return false;

        return are_trees_equal(target, other.root_);
    }

    /*============ Обходы по заданному порядку ============*/
    // Обход в порядке order (любой из 6: КЛП, КПЛ, ЛКП, ЛПК, ПКЛ, ПЛК) - TraversalOrder, action(const T&).

    template<class Action>
    void traverse(const TraversalOrder& order, Action action) const {
        if (root_ == nullptr)
            return;

        MutableArraySequence<ConstFrame> stack; // стэк кадров (кадр - на каком узле щас рабоаем)
        stack.append(ConstFrame(root_, 0));

        while (!stack.is_empty()) {
            int top = stack.get_length() - 1;
            ConstFrame frame = stack.get(top); // читаем кадр на вершине (читаем копию потому что get возвр const)
            if (frame.step == TraversalOrder::PARTS) { // шаги 0 1 2 выполнены
                stack.remove_at(top);
                continue;
            }
            // важно перезаписать до switch, т.к. дальше мы можем положить в стэк ребёнка
            // и тогда сначала надо будет обработать его, а потом вернуться к текущему кадру
            // и обработать его со сл шага. Если сделаем после switch, то мы бы уже не смогли до него добраться
            stack.set(ConstFrame(frame.node, frame.step + 1), top); // сл шаг этого кадра

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

    // Собирает элементы в массив в заданном порядке
    MutableArraySequence<T> to_sequence(const TraversalOrder& order) const {
        MutableArraySequence<T> result;
        traverse(order, [&result](const T& item) { result.append(item); });
        return result;
    }

    // Собирает элементы по имени порядка ("КЛП", "ЛКП", ...)
    MutableArraySequence<T> to_sequence(const std::string& order_name) const {
        return to_sequence(TraversalOrder::standard(order_name));
    }

    /*итератор*/

    // Класс итератора по дереву (наследует IEnumerator<T> из ЛР-2).
    // Поддерживает обход как по прошитому дереву (за O(1) на шаг),
    // так и ленивый итеративный обход непрошитого дерева в любом из 6 порядков.
    class TreeEnumerator : public IEnumerator<T> {
    public:
        // Итератор по прошитому дереву
        explicit TreeEnumerator(const Node* head)
            : current_node_(head), lazy_mode_(false), order_(TraversalOrder::standard("ЛКП")) {}

        // Ленивый итератор по произвольному порядку обхода (через явный стек кадров)
        TreeEnumerator(const Node* root, const TraversalOrder& order)
            : current_node_(nullptr), lazy_mode_(true), order_(order) {
            if (root != nullptr) {
                stack_.append(ConstFrame(root, 0));
                advance_lazy();
            }
        }

        bool has_more_elements() override {
            return current_node_ != nullptr;
        }

        const T& next() override {
            if (!has_more_elements())
                throw std::out_of_range("TreeEnumerator::next: no more elements");

            const T& val = current_node_->value;
            if (!lazy_mode_) {
                current_node_ = current_node_->next;
            } else {
                advance_lazy();
            }
            return val;
        }

    private:
        const Node* current_node_;
        bool lazy_mode_; // false - дерево прошито => переход по указателю, true - перечислитель сам ищет очередной узел через advance_lazy(), продолжая обход со своего стека кадров
        TraversalOrder order_;
        MutableArraySequence<ConstFrame> stack_;

        void advance_lazy() { // Она обрабатывает Л и П спуском к ребёнку, снимает завершённые кадры и на части К делает паузу
            current_node_ = nullptr;
            while (!stack_.is_empty()) {
                int top = stack_.get_length() - 1;
                ConstFrame frame = stack_.get(top);
                if (frame.step == TraversalOrder::PARTS) {
                    stack_.remove_at(top);
                    continue;
                }
                stack_.set(ConstFrame(frame.node, frame.step + 1), top);

                switch (order_.part(frame.step)) {
                    case TraversalPart::Root:
                        current_node_ = frame.node;
                        return;
                    case TraversalPart::Left:
                        if (frame.node->left != nullptr)
                            stack_.append(ConstFrame(frame.node->left, 0));
                        break;
                    case TraversalPart::Right:
                        if (frame.node->right != nullptr)
                            stack_.append(ConstFrame(frame.node->right, 0));
                        break;
                }
            }
        }
    };

    // Сброс прошивки
    void unthread() {
        if (threaded_head_ == nullptr)
            return;
        Node* current = threaded_head_;
        while (current != nullptr) {
            Node* next = current->next;
            current->next = nullptr;
            current = next;
        }
        threaded_head_ = nullptr;
    }

    bool is_threaded() const {
        return threaded_head_ != nullptr || root_ == nullptr;
    }

    // Прошивка дерева в порядке order (любой из 6 стандартных порядков)
    void thread(const TraversalOrder& order) {
        unthread();
        if (root_ == nullptr)
            return;

        Node* first = nullptr; // первый узел обхода
        Node* prev = nullptr; // предыдущий посещённый

        traverse_nodes(order, [&first, &prev](Node* node) { // обходим узлы в выбранном порядке
            if (first == nullptr) // на первом посещении запоминаем начало цепочки
                first = node;
            if (prev != nullptr) // соединяем предыдущий узел с текущим
                prev->next = node;
            prev = node; // запоминаем текущий узел как предыдцщий
            node->next = nullptr;
        });

        threaded_head_ = first;
    }

    // Прошивка по фиксированному обходу (по умолчанию симметричный ЛКП)
    void thread() {
        thread(TraversalOrder::standard("ЛКП"));
    }

    // Прошивка по имени порядка ("КЛП", "ЛКП", ...)
    void thread(const std::string& order_name) {
        thread(TraversalOrder::standard(order_name));
    }

    // Вызывающий код удаляет перечислитель. Все перегрузки возвращают перечислитель,
    // действительный до изменения структуры дерева, смены или сброса прошивки,
    // присваивания, swap, перемещения из дерева либо его уничтожения.
    IEnumerator<T>* get_enumerator() const override {
        if (threaded_head_ != nullptr || root_ == nullptr)
            return new TreeEnumerator(threaded_head_);
        return new TreeEnumerator(root_, TraversalOrder::standard("ЛКП"));
    }

    IEnumerator<T>* get_enumerator(const TraversalOrder& order) const {
        return new TreeEnumerator(root_, order);
    }

    IEnumerator<T>* get_enumerator(const std::string& order_name) const {
        return get_enumerator(TraversalOrder::standard(order_name));
    }

    /*============ Сохранение в строку ============*/

    // По формат-строке: tree.to_string("{К}(Л)[П]")
    // Пустое поддерево - пустые скобки, пустое дерево - пустая строка. O(n).
    std::string to_string(const std::string& format) const {
        return to_string(TraversalOrder::parse(format));
    }

    // По фиксированному обходу КЛП со скобками {К}(Л)[П]
    std::string to_string() const {
        return to_string(TraversalOrder::standard("КЛП"));
    }

    std::string to_string(const TraversalOrder& order) const {
        std::string out; // строка - результат, в которой дописываются символы
        if (root_ == nullptr)
            return out;

        MutableArraySequence<ConstFrame> stack; // стэк кадров
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
                append_escaped(out, Serializer<T>::to_string(frame.node->value), order); // значение в текст (to_str)
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

    /*============ Чтение из строки ============*/

    // Восстанавливает дерево ровно той структуры, что записана в строке.
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
        result.root_ = parse_nodes(text, order, count); // собираем дерево из текста
        result.count_ = count;

        if (!result.is_ordered()) // проверка на корректность (BST ли построенное дерево)
            throw ParseException("elements violate the search tree order (or repeat)");
        if (!result.check_invariants()) // структура не AVL - перестраиваем
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

    /*============ Перегрузка операторов (этап 8) ============*/

    // Равенство по составу. Равные через operator== значения должны иметь
    // эквивалентные ключи для компараторов обоих деревьев.
    // O(n * h), где h — высота other. Дополнительная память O(1).
    bool operator==(const BSTree& other) const {
        if (count_ != other.count_)
            return false;
        for (const Node* node = leftmost(root_); node != nullptr; node = successor(node)) {
            const Node* match = other.find_node(node->value);
            if (match == nullptr || !(node->value == match->value))
                return false;
        }
        return true;
    }

    bool operator!=(const BSTree& other) const {
        return !(*this == other);
    }

    // Объединение двух деревьев: t1 + t2
    BSTree operator+(const BSTree& other) const {
        BSTree result(*this);
        other.for_each([&result](const T& item) {
            result.try_insert(item);
        });
        return result;
    }

    // Добавление элементов из другого дерева: t1 += t2
    BSTree& operator+=(const BSTree& other) {
        other.for_each([this](const T& item) {
            try_insert(item);
        });
        return *this;
    }

    // Добавление одиночного элемента: t += item
    BSTree& operator+=(const T& item) {
        insert(item);
        return *this;
    }

private:
    Node* root_;
    Node* threaded_head_; // начало списка прошивки (nullptr, если дерево не прошито)
    int count_; // число элементов
    Compare less_; // компаратор

    // Своя замена std::swap
    template<class X>
    static void exchange(X& a, X& b) noexcept {
        X temp = a;
        a = b;
        b = temp;
    }

    // Шаблонный поиск узла: работает одинаково для const Node* и Node*, без const_cast
    template<class NodePtr>
    static NodePtr find_from(NodePtr root, const T& item, const Compare& less) {
        NodePtr current = root;
        while (current != nullptr) {
            if (less(item, current->value))
                current = current->left;
            else if (less(current->value, item))
                current = current->right;
            else
                return current;
        }
        return nullptr;
    }

    const Node* find_node(const T& item) const {
        return find_from(root_, item, less_);
    }

    Node* find_node(const T& item) {
        return find_from(root_, item, less_);
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

        // поднимаемся, пока приходим из правого поддерева (Потому что ЛКП посещает родителя до его правой ветви)
        const Node* parent = node->parent;
        while (parent != nullptr && node == parent->right) {
            node = parent;
            parent = parent->parent;
        }
        return parent;
    }

    struct BuildFrame {
        Node* node; // какой узел сейчас смотрим
        int step; // следующий часть узла
        BuildFrame() : node(nullptr), step(0) {}
        BuildFrame(Node* node, int step) : node(node), step(step) {} // (2,1) - узел знач 2 и разбираем его Left
    };

    // Строго ли возрастает дерево - обходит готовое дерево в ЛКП
    bool is_ordered() const {
        const Node* previous = nullptr;
        for (const Node* node = leftmost(root_); node != nullptr; node = successor(node)) {
            if (previous != nullptr && !less_(previous->value, node->value))
                return false;
            previous = node;
        }
        return true;
    }
    // помогает записать значение одного узла, чтобы скобки внутри значения сохранились как текст
    static void append_escaped(std::string& out, const std::string& value, const TraversalOrder& order) {
        for (char c : value) {
            if (order.is_special(c))
                out += TraversalOrder::ESCAPE;
            out += c;
        }
    }

    static std::string position_message(const char* what, std::size_t pos) { // где произошла ошибка (собирает её текст)
        return std::string(what) + " at position " + std::to_string(pos);
    }

    static void expect(const std::string& text, std::size_t& pos, char expected) { // прочитай именно эту скобку (expected)
        if (pos >= text.size()) // кончилась строка
            throw ParseException(position_message(
            (std::string("unexpected end, expected '") + expected + "'").c_str(), pos));
        if (text[pos] != expected) // на месте есть другой символ
            throw ParseException(position_message(
                (std::string("expected '") + expected + "', got '" + text[pos] + "'").c_str(), pos));
        pos++; // правильный символ
    }

    // прочитать текст значения до разделителя
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

    // собираем деверо из текста
    static Node* parse_nodes(const std::string& text, const TraversalOrder& order, int& count) {
        MutableArraySequence<BuildFrame> stack;
        Node* root = new Node(T(), nullptr);
        count = 1;
        std::size_t pos = 0; // где сейчас читаем строку

        try {
            stack.append(BuildFrame(root, 0));

            while (!stack.is_empty()) {
                int top = stack.get_length() - 1;
                BuildFrame frame = stack.get(top);
                // узел полностью прочитан - обе ветви обработаны
                if (frame.step == TraversalOrder::PARTS) {
                    update_height(frame.node);
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
                // читаем значение узла
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

    // Рекурсия глубиной O(log n) - для сбалансированной постройки это безопасно (items - отсортирована)
    static Node* build_balanced(const Sequence<T>& items, int lo, int hi, Node* parent) {
        if (lo > hi)
            return nullptr;

        int mid = lo + (hi - lo) / 2;
        Node* node = new Node(items.get(mid), parent); // средний элемент - корень
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
            MutableArraySequence<Pair<const Node*, Node*>> stack; // стэк из пар (1 - читает ориг узел, 2 - меняет копию)
            stack.append(Pair<const Node*, Node*>(source_root, new_root));

            while (!stack.is_empty()) {
                Pair<const Node*, Node*> top = stack.get_last();
                stack.remove_at(stack.get_length() - 1);
                const Node* source = top.key; // адрес оригинала
                Node* copy = top.value; // адрес копии
                // копируем детей
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

    // Подсчет количества узлов в поддереве (итеративно через стек)
    static int count_subtree(const Node* root) {
        if (root == nullptr)
            return 0;
        MutableArraySequence<const Node*> stack;
        stack.append(root);
        int count = 0;
        while (!stack.is_empty()) {
            const Node* current = stack.get_last();
            stack.remove_at(stack.get_length() - 1);
            count++;
            if (current->left != nullptr)
                stack.append(current->left);
            if (current->right != nullptr)
                stack.append(current->right);
        }
        return count;
    }

    // Проверка структурного равенства и равенства значений двух поддеревьев (итеративно через стек)
    static bool are_trees_equal(const Node* a, const Node* b) {
        if (a == nullptr && b == nullptr)
            return true;
        if (a == nullptr || b == nullptr)
            return false;

        MutableArraySequence<Pair<const Node*, const Node*>> stack;
        stack.append(Pair<const Node*, const Node*>(a, b));

        while (!stack.is_empty()) {
            Pair<const Node*, const Node*> top = stack.get_last();
            stack.remove_at(stack.get_length() - 1);

            const Node* na = top.key;
            const Node* nb = top.value;

            if (!(na->value == nb->value))
                return false;

            if ((na->left == nullptr) != (nb->left == nullptr))
                return false;
            if (na->left != nullptr)
                stack.append(Pair<const Node*, const Node*>(na->left, nb->left));

            if ((na->right == nullptr) != (nb->right == nullptr))
                return false;
            if (na->right != nullptr)
                stack.append(Pair<const Node*, const Node*>(na->right, nb->right));
        }
        return true;
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
