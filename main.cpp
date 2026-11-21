#include <iostream>
#include <string>

#include "ui/tree_menu.h"

int main() {
    try {
        while (true) {
            std::cout << "\n=== Binary search tree ===\n"
                         "1. int\n"
                         "2. double\n"
                         "3. String\n"
                         "4. Complex\n"
                         "5. Person\n"
                         "6. Polynomial function\n"
                         "0. Exit\n"
                         "Choice: ";
            switch (ReadIntInRange(0, 6)) {
                case 0:
                    return 0;
                case 1:
                    tree_ui::ChooseBalance<int>("int");
                    break;
                case 2:
                    tree_ui::ChooseBalance<double>("double");
                    break;
                case 3:
                    tree_ui::ChooseBalance<std::string>("string");
                    break;
                case 4:
                    tree_ui::ChooseBalance<Complex>("Complex");
                    break;
                case 5:
                    tree_ui::ChooseBalance<Person, PersonByIdLess>("Person");
                    break;
                case 6:
                    tree_ui::ChooseBalance<PolynomialFunction>("Polynomial function");
                    break;
            }
        }
    } catch (const InputClosedException&) {
        std::cout << "\nInput closed.\n";
    }
}
