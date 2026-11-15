// ЛР-1: временная демонстрация ядра. Консольное меню появится на этапе 12.
#include <iostream>
#include <string>

#include "tree/bs_tree.h"

int main() {
    int raw[] = {50, 30, 70, 20, 40, 60, 80};
    BSTree<int> tree(raw, 7);

    std::cout << "Elements (in-order):";
    tree.for_each([](int x) { std::cout << ' ' << x; });
    std::cout << "\nCount: " << tree.get_count() << ", height: " << tree.get_height() << '\n';

    BSTree<int> even = tree.where([](int x) { return x % 20 == 0; });
    std::cout << "where(x % 20 == 0):";
    even.for_each([](int x) { std::cout << ' ' << x; });

    int sum = tree.reduce([](int x, int acc) { return x + acc; }, 0);
    std::cout << "\nreduce(+, 0) = " << sum << '\n';

    std::string text = tree.to_string("{К}(Л)[П]");
    std::cout << "to_string({К}(Л)[П]): " << text << '\n';
    BSTree<int> restored = BSTree<int>::from_string(text, "{К}(Л)[П]");
    std::cout << "from_string -> count " << restored.get_count() << '\n';

    try {
        BSTree<int>::from_string("{1}({x}()[])[]", "{К}(Л)[П]");
    } catch (const ParseException& e) {
        std::cout << e.what() << '\n';
    }

    try {
        tree.remove(999);
    } catch (const TreeException& e) {
        std::cout << "remove(999): " << e.what() << '\n';
    }
    return 0;
}
