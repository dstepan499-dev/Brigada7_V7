#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>
#include <iomanip>
#include <thread>

const double A = 3.0;
const double B = 7.0;

inline double f(double x) {
    return std::pow(x + 1.0, 2) / std::sqrt(std::log(x));
}

double integrate_simpson_range(long long start_k, long long end_k, double h, double a) {
    if (start_k >= end_k) return 0.0;

    long long start_idx = 2 * start_k;
    long long end_idx = 2 * end_k;

    double sum_odd = 0.0;
    for (long long i = start_idx + 1; i < end_idx; i += 2) {
        sum_odd += f(a + i * h);
    }

    double sum_even = 0.0;
    for (long long i = start_idx + 2; i < end_idx; i += 2) {
        sum_even += f(a + i * h);
    }

    double local_res = (f(a + start_idx * h) + f(a + end_idx * h) +
        4.0 * sum_odd + 2.0 * sum_even) * (h / 3.0);
    return local_res;
}

double integrate_std_thread(long long N, int num_threads) {
    double h = (B - A) / static_cast<double>(N);
    long long M = N / 2;

    std::vector<std::thread> threads;
    std::vector<double> partial_sums(num_threads, 0.0);

    long long chunk_size = M / num_threads;
    long long remainder = M % num_threads;

    long long current_start = 0;
    for (int t = 0; t < num_threads; ++t) {
        long long current_chunk = chunk_size + (t < remainder ? 1 : 0);
        long long current_end = current_start + current_chunk;

        threads.emplace_back([t, current_start, current_end, h, &partial_sums]() {
            partial_sums[t] = integrate_simpson_range(current_start, current_end, h, A);
            });

        current_start = current_end;
    }

    double total = 0.0;
    for (int t = 0; t < num_threads; ++t) {
        threads[t].join();
        total += partial_sums[t];
    }
    return total;
}

#ifndef _WIN32
#include <pthread.h>
struct PthreadData {
    long long start_k;
    long long end_k;
    double h;
    double a;
    double result;
};

void* pthread_worker(void* arg) {
    auto* data = static_cast<PthreadData*>(arg);
    data->result = integrate_simpson_range(data->start_k, data->end_k, data->h, data->a);
    return nullptr;
}

double integrate_posix(long long N, int num_threads) {
    double h = (B - A) / static_cast<double>(N);
    long long M = N / 2;
    std::vector<pthread_t> threads(num_threads);
    std::vector<PthreadData> thread_data(num_threads);

    long long chunk_size = M / num_threads;
    long long remainder = M % num_threads;
    long long current_start = 0;

    for (int t = 0; t < num_threads; ++t) {
        long long current_chunk = chunk_size + (t < remainder ? 1 : 0);
        long long current_end = current_start + current_chunk;
        thread_data[t] = { current_start, current_end, h, A, 0.0 };
        pthread_create(&threads[t], nullptr, pthread_worker, &thread_data[t]);
        current_start = current_end;
    }

    double total = 0.0;
    for (int t = 0; t < num_threads; ++t) {
        pthread_join(threads[t], nullptr);
        total += thread_data[t].result;
    }
    return total;
}
#else
    
double integrate_posix(long long N, int num_threads) {
    return integrate_std_thread(N, num_threads);
}
#endif

struct BenchRow {
    int threads;
    double time_sec;
    double result;
    double speedup;
    double efficiency;
};

int main() {
    setlocale(LC_ALL, "Russian");  

    long long N;
    int main_threads, max_threads;

    std::cout << "Лабораторная 1, Бригада 7, метод Симпсона" << std::endl;
    std::cout << "Введите количество разбиений N (рекомендуется 50000000 - 100000000): ";
    if (!(std::cin >> N) || N <= 0) return 1;

    if (N % 2 != 0) {
        N++;
        std::cout << "-> N скорректировано до четного: " << N << std::endl;
    }

    std::cout << "Введите количество потоков для основного вычисления (например, 4): ";
    std::cin >> main_threads;

    std::cout << "Введите максимальное число потоков для тестирования (например, 8 или 16): ";
    std::cin >> max_threads;

    std::cout << "\n1) Основные результаты\n";

    auto t0 = std::chrono::high_resolution_clock::now();
    double res_1 = integrate_std_thread(N, 1);
    auto t1 = std::chrono::high_resolution_clock::now();
    double time_1 = std::chrono::duration<double>(t1 - t0).count();

    t0 = std::chrono::high_resolution_clock::now();
    double res_main = integrate_std_thread(N, main_threads);
    t1 = std::chrono::high_resolution_clock::now();
    double time_main = std::chrono::duration<double>(t1 - t0).count();

    double sp = time_1 / time_main;
    double eff = (sp / main_threads) * 100.0;

    std::cout << std::fixed << std::setprecision(8);
    std::cout << "1 поток:        Время = " << time_1 << " c,  Результат = " << res_1 << "\n";
    std::cout << main_threads << " потока(ов):   Время = " << time_main << " c,  Результат = " << res_main << "\n";
    std::cout << std::setprecision(2);
    std::cout << "Ускорение:      " << sp << "x\n";
    std::cout << "Эффективность:  " << eff << " %\n";

    std::vector<BenchRow> table_posix;
    std::vector<BenchRow> table_std;

    double t1_posix = 0.0;
    double t1_std = time_1;

    for (int p = 1; p <= max_threads; ++p) {
        
        auto start = std::chrono::high_resolution_clock::now();
        double r_pos = integrate_posix(N, p);
        auto end = std::chrono::high_resolution_clock::now();
        double t_pos = std::chrono::duration<double>(end - start).count();

        if (p == 1) t1_posix = t_pos;
        double sp_pos = t1_posix / t_pos;
        table_posix.push_back({ p, t_pos, r_pos, sp_pos, (sp_pos / p) * 100.0 });

        start = std::chrono::high_resolution_clock::now();
        double r_std = integrate_std_thread(N, p);
        end = std::chrono::high_resolution_clock::now();
        double t_std = std::chrono::duration<double>(end - start).count();

        double sp_std = t1_std / t_std;
        table_std.push_back({ p, t_std, r_std, sp_std, (sp_std / p) * 100.0 });
    }

    auto print_table = [](const std::string& title, const std::vector<BenchRow>& rows) {
        std::cout << title;
        std::cout << std::setw(8) << "Потоки"
            << std::setw(14) << "Время (с)"
            << std::setw(18) << "Результат"
            << std::setw(14) << "Ускорение"
            << std::setw(18) << "Эффективность(%)" << "\n";
        for (const auto& r : rows) {
            std::cout << std::setw(8) << r.threads
                << std::setw(14) << std::fixed << std::setprecision(5) << r.time_sec
                << std::setw(18) << std::setprecision(7) << r.result
                << std::setw(14) << std::setprecision(2) << r.speedup
                << std::setw(17) << std::setprecision(2) << r.efficiency << "%\n";
        }
        };

    print_table("СВОДНАЯ ТАБЛИЦА: POSIX (pthread)", table_posix);
    print_table("СВОДНАЯ ТАБЛИЦА: std::thread", table_std);

    return 0;
}
