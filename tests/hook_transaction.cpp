#include "HookTransaction.h"
#include <iostream>
int installed = 0;
struct Good {
    static int hook() {
        ++installed;
        return 0;
    }
    static void unhook() {
        --installed;
    }
};
struct Bad {
    static int hook() {
        return -1;
    }
    static void unhook() {
        throw std::runtime_error("never installed");
    }
};
int main() {
    try {
        bool rejected = false;
        try {
            terrain_runtime::HookTransaction t;
            t.add<Good>("first");
            t.add<Bad>("missing");
            t.commit();
        } catch (std::runtime_error const &e) {
            rejected = std::string(e.what()).find("missing") != std::string::npos;
        }
        if (!rejected || installed)
            throw std::runtime_error("partial adapter left installed");
        {
            terrain_runtime::HookTransaction t;
            t.add<Good>("success");
            t.commit();
        }
        if (installed != 1)
            throw std::runtime_error("committed hook removed");
        Good::unhook();
        std::cout << "hook installation failure rollback passed\n";
        return 0;
    } catch (std::exception const &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
