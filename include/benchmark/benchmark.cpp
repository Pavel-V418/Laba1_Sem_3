#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "benchmark/timer.h"
#include "tree/bs_tree.h"

namespace {

using AvlTree = BSTree<int, KeyLess<int>, AvlBalance>;
using PlainTree = BSTree<int, KeyLess<int>, NoBalance>;

constexpr int query_count = 500;
constexpr int repetitions = 5;
constexpr int max_ordered_bst_size = 10000;
constexpr int max_random_bst_size = 250000;
constexpr int max_array_size = 50000;

struct Timings {
    double build_ms;
    double find_us;
    double remove_us;
};

struct Workload {
    std::vector<int> find_keys;
    std::vector<int> remove_keys;
};

std::vector<int> sorted_keys(int n) {
    std::vector<int> keys(n);
    for (int i = 0; i < n; ++i)
        keys[i] = i * 2;
    return keys;
}

std::vector<int> generate_data(int n, const std::string& order) {
    auto data = sorted_keys(n);
    if (order == "random") {
        std::mt19937 rng(1337);
        std::shuffle(data.begin(), data.end(), rng);
    } else if (order == "reversed") {
        std::reverse(data.begin(), data.end());
    }
    return data;
}

// Для данного n запросы одинаковы при всех порядках вставки и для всех структур.
Workload make_workload(int n) {
    Workload workload;
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> pick_key(0, n - 1);
    for (int i = 0; i < query_count; ++i)
        workload.find_keys.push_back(2 * pick_key(rng));

    workload.remove_keys = sorted_keys(n);
    std::shuffle(workload.remove_keys.begin(), workload.remove_keys.end(), rng);
    workload.remove_keys.resize(query_count);
    return workload;
}

template<class Build, class Find, class Remove>
Timings measure(const Workload& workload, Build build, Find find, Remove remove) {
    std::array<double, repetitions> build_ms;
    std::array<double, repetitions> find_us;
    std::array<double, repetitions> remove_us;

    for (int i = 0; i < repetitions; ++i) {
        Timer timer;
        auto structure = build();
        build_ms[i] = timer.elapsed_ms();

        timer.restart();
        for (int key : workload.find_keys) {
            volatile bool found = find(structure, key);
            (void)found;
        }
        find_us[i] = timer.elapsed_us() / workload.find_keys.size();

        timer.restart();
        for (int key : workload.remove_keys)
            remove(structure, key);
        remove_us[i] = timer.elapsed_us() / workload.remove_keys.size();
    }

    std::sort(build_ms.begin(), build_ms.end());
    std::sort(find_us.begin(), find_us.end());
    std::sort(remove_us.begin(), remove_us.end());
    return {build_ms[repetitions / 2], find_us[repetitions / 2], remove_us[repetitions / 2]};
}

template<class Tree>
Timings measure_tree(const std::vector<int>& data, const Workload& workload) {
    return measure(workload,
                   [&data] { return Tree(data.data(), static_cast<int>(data.size())); },
                   [](const Tree& tree, int key) { return tree.contains(key); },
                   [](Tree& tree, int key) { tree.remove(key); });
}

Timings measure_array(const std::vector<int>& data, const Workload& workload) {
    return measure(workload,
                   [&data] { return data; },
                   [](const std::vector<int>& array, int key) {
                       return std::find(array.begin(), array.end(), key) != array.end();
                   },
                   [](std::vector<int>& array, int key) {
                       auto found = std::find(array.begin(), array.end(), key);
                       if (found != array.end())
                           array.erase(found);
                   });
}

void write_timings(std::ostream& out, const char* structure, const std::string& order, int n,
                   const Timings& timings) {
    out << "Integer," << structure << ',' << order << ',' << n << ','
        << timings.build_ms << ',' << timings.find_us << ',' << timings.remove_us << ",0,\n";
}

void write_skip(std::ostream& out, const char* structure, const std::string& order, int n, int limit) {
    out << "Integer," << structure << ',' << order << ',' << n << ",,,,1,n exceeds limit " << limit << '\n';
}

} // namespace

int main(int argc, char** argv) {
    const std::string csv_path = argc > 1 ? argv[1] : "benchmark.csv";
    std::ofstream out(csv_path);
    if (!out) {
        std::cerr << "Cannot open " << csv_path << '\n';
        return 1;
    }
    out << "attribute,structure,order,n,build_ms,find_us,remove_us,skipped,skip_reason\n";

    const int sizes[] = {1000, 5000, 10000, 25000, 50000, 100000, 250000, 500000, 1000000};
    const std::string orders[] = {"random", "sorted", "reversed"};
    for (int n : sizes) {
        const auto workload = make_workload(n);
        for (const auto& order : orders) {
            std::cout << "Order: " << order << ", N = " << n << std::endl;
            const auto data = generate_data(n, order);
            write_timings(out, "AVL Tree", order, n, measure_tree<AvlTree>(data, workload));

            int bst_limit = order == "random" ? max_random_bst_size : max_ordered_bst_size;
            if (n <= bst_limit)
                write_timings(out, "Unbalanced BST", order, n, measure_tree<PlainTree>(data, workload));
            else
                write_skip(out, "Unbalanced BST", order, n, bst_limit);

            if (n <= max_array_size)
                write_timings(out, "Linear Array", order, n, measure_array(data, workload));
            else
                write_skip(out, "Linear Array", order, n, max_array_size);
        }
    }

    out.close();
    if (!out) {
        std::cerr << "Cannot write results to " << csv_path << '\n';
        return 1;
    }
    std::cout << "Results saved to " << csv_path << '\n';
    return 0;
}
