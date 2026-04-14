#include "log_view.hpp"

int main() {
    try {
        LogView().run();

        return EXIT_SUCCESS;
    } catch (const std::exception &ex) {
        std::cerr << "Ошибка во время выполнения: " << ex.what() << '\n';
        return EXIT_FAILURE;
    }
}
