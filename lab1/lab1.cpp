#include <iostream>
#include <cmath>
#include <chrono>
#include <thread>
#include <vector>
#include <functional>

#define HAVE_STRUCT_TIMESPEC
#include <pthread.h>
#pragma comment(lib, "pthreadVCE2.lib")

using namespace std;
using namespace std::chrono;

double f(double x) {
    return pow(x + 1.0, 2.0) / sqrt(log(x));
}

struct PthreadArgs {
    double a;
    double h;
    int start_i;
    int end_i;
};

void* right_rectangles_posix(void* arg) {
    PthreadArgs* args = static_cast<PthreadArgs*>(arg);
    double local_sum = 0.0;

    for (int i = args->start_i; i <= args->end_i; ++i) {
        double x = args->a + i * args->h;
        local_sum += f(x);
    }

    double* result = new double(local_sum * args->h);
    return result;
}

double integrate_posix(int N, int T) {
    double a = 3.0, b = 7.0;
    double h = (b - a) / N;
    double total_sum = 0.0;

    pthread_t* threads = new pthread_t[T];
    PthreadArgs* args = new PthreadArgs[T];

    int chunk = N / T;
    int remainder = N % T;
    int current_i = 1;

    for (int k = 0; k < T; ++k) {
        args[k].a = a;
        args[k].h = h;
        args[k].start_i = current_i;
        args[k].end_i = current_i + chunk - 1 + (k < remainder ? 1 : 0);
        current_i = args[k].end_i + 1;

        pthread_create(&threads[k], NULL, right_rectangles_posix, &args[k]);
    }

    for (int k = 0; k < T; ++k) {
        void* ptr;
        pthread_join(threads[k], &ptr);
        total_sum += *static_cast<double*>(ptr);
        delete static_cast<double*>(ptr);
    }

    delete[] threads;
    delete[] args;
    return total_sum;
}

void right_rectangles_std(double a, double h, int start_i, int end_i, double& local_sum) {
    local_sum = 0.0;
    for (int i = start_i; i <= end_i; ++i) {
        double x = a + i * h;
        local_sum += f(x);
    }
    local_sum *= h;
}

double integrate_std_thread(int N, int T) {
    double a = 3.0, b = 7.0;
    double h = (b - a) / N;
    double total_sum = 0.0;

    vector<thread> threads(T);
    vector<double> local_sums(T, 0.0);

    int chunk = N / T;
    int remainder = N % T;
    int current_i = 1;

    for (int k = 0; k < T; ++k) {
        int end_i = current_i + chunk - 1 + (k < remainder ? 1 : 0);
        threads[k] = thread(right_rectangles_std, a, h, current_i, end_i, ref(local_sums[k]));
        current_i = end_i + 1;
    }

    for (int k = 0; k < T; ++k) {
        threads[k].join();
        total_sum += local_sums[k];
    }

    return total_sum;
}

int main() {
    setlocale(LC_ALL, "Russian");

    int N, T, max_T;
    cout << "Введите количество разбиений (N): ";
    cin >> N;
    cout << "Введите количество потоков для основного вычисления (T): ";
    cin >> T;
    cout << "Введите максимальное количество потоков для тестирования: ";
    cin >> max_T;

    auto start_single = high_resolution_clock::now();
    double result_single = integrate_posix(N, 1);
    auto end_single = high_resolution_clock::now();
    double time_single = duration_cast<microseconds>(end_single - start_single).count() / 1000.0;

    cout << "\n--- Однопоточный режим ---\n";
    cout << "Результат: " << result_single << "\n";
    cout << "Время: " << time_single << " мс\n";

    // Тестирование POSIX
    cout << "\n--- POSIX Threads ---\n";
    cout << "Потоки | Время (мс) | Ускорение | Эффективность (%) | Результат\n";
    for (int t = 1; t <= max_T; ++t) {
        auto start = high_resolution_clock::now();
        double res = integrate_posix(N, t);
        auto end = high_resolution_clock::now();
        double time = duration_cast<microseconds>(end - start).count() / 1000.0;

        double speedup = time_single / time;
        double efficiency = (speedup / t) * 100.0;

        cout << t << "      | " << time << "   | " << speedup << "      | " << efficiency << "             | " << res << "\n";
    }

    // Тестирование std::thread
    cout << "\n--- std::thread ---\n";
    cout << "Потоки | Время (мс) | Ускорение | Эффективность (%) | Результат\n";
    for (int t = 1; t <= max_T; ++t) {
        auto start = high_resolution_clock::now();
        double res = integrate_std_thread(N, t);
        auto end = high_resolution_clock::now();
        double time = duration_cast<microseconds>(end - start).count() / 1000.0;

        double speedup = time_single / time;
        double efficiency = (speedup / t) * 100.0;

        cout << t << "      | " << time << "   | " << speedup << "      | " << efficiency << "             | " << res << "\n";
    }

    system("pause");
    return 0;
}
