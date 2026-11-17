#include <iostream>
#include <cmath>
#include <chrono>
#include <thread>
#include <vector>
#include <iomanip>

#define HAVE_STRUCT_TIMESPEC
#include <pthread.h>
#pragma comment(lib, "pthreadVCE2.lib")

using namespace std;
using namespace std::chrono;

inline double f(double x) {
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

    // Возврат значения через динамическую память согласно лекции
    double* result = new double(local_sum * args->h);
    return static_cast<void*>(result);
}

double integrate_posix(int N, int T) {
    double a = 3.0, b = 7.0;
    double h = (b - a) / N;
    double total_sum = 0.0;

    vector<pthread_t> threads(T);
    vector<PthreadArgs> args(T);

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
        void* ptr = nullptr;
        pthread_join(threads[k], &ptr);
        if (ptr) {
            total_sum += *static_cast<double*>(ptr);
            delete static_cast<double*>(ptr);
        }
    }

    return total_sum;
}

// Параметры передаются по отдельности, промежуточная сумма считается в локальной переменной
void right_rectangles_std(double a, double h, int start_i, int end_i, double* out_sum) {
    double local_sum = 0.0; // Локально! Решает проблему False Sharing
    for (int i = start_i; i <= end_i; ++i) {
        double x = a + i * h;
        local_sum += f(x);
    }
    *out_sum = local_sum * h; // Запись в общую память ровно один раз
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
        // Передача аргументов по отдельности без объединения в структуру
        threads[k] = thread(right_rectangles_std, a, h, current_i, end_i, &local_sums[k]);
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

    cout << fixed << setprecision(6);

    auto start_1 = high_resolution_clock::now();
    double res_1 = integrate_posix(N, 1);
    auto end_1 = high_resolution_clock::now();
    double time_1 = duration_cast<microseconds>(end_1 - start_1).count() / 1000.0;

    cout << "\nОднопоточный режим\n";
    cout << "Результат: " << res_1 << "\n";
    cout << "Время:     " << time_1 << " мс\n";

    // Расчет через POSIX на T потоках
    auto start_posix_T = high_resolution_clock::now();
    double res_posix_T = integrate_posix(N, T);
    auto end_posix_T = high_resolution_clock::now();
    double time_posix_T = duration_cast<microseconds>(end_posix_T - start_posix_T).count() / 1000.0;

    // Расчет через std::thread на T потоках
    auto start_std_T = high_resolution_clock::now();
    double res_std_T = integrate_std_thread(N, T);
    auto end_std_T = high_resolution_clock::now();
    double time_std_T = duration_cast<microseconds>(end_std_T - start_std_T).count() / 1000.0;

    cout << "\n Основной многопоточный расчет (T = " << T << " потоков) \n";
    cout << "POSIX Threads:\n";
    cout << "  Результат: " << res_posix_T << "\n";
    cout << "  Время:     " << time_posix_T << " мс\n";
    cout << "  Ускорение: " << (time_1 / time_posix_T) << "x\n";

    cout << "std::thread:\n";
    cout << "  Результат: " << res_std_T << "\n";
    cout << "  Время:     " << time_std_T << " мс\n";
    cout << "  Ускорение: " << (time_1 / time_std_T) << "x\n";
    cout << "\n";

    vector<double> time_posix(max_T + 1, 0.0);
    vector<double> time_std(max_T + 1, 0.0);
    vector<double> res_posix(max_T + 1, 0.0);
    vector<double> res_std(max_T + 1, 0.0);

    // Тестирование POSIX Threads
    cout << "\nТестирование POSIX Threads\n";
    cout << "Потоки | Время (мс) | Ускорение | Эффективность (%) | Результат\n";
    for (int t = 1; t <= max_T; ++t) {
        auto start = high_resolution_clock::now();
        res_posix[t] = integrate_posix(N, t);
        auto end = high_resolution_clock::now();
        time_posix[t] = duration_cast<microseconds>(end - start).count() / 1000.0;

        double speedup = (t == 1) ? 1.0 : (time_posix[1] / time_posix[t]);
        double eff = (speedup / t) * 100.0;

        cout << setw(6) << t << " | "
            << setw(10) << fixed << setprecision(2) << time_posix[t] << " | "
            << setw(9) << fixed << setprecision(4) << speedup << " | "
            << setw(17) << fixed << setprecision(2) << eff << " | "
            << setprecision(6) << res_posix[t] << "\n";
    }

    // Тестирование std::thread
    cout << "\nТестирование std::thread\n";
    cout << "Потоки | Время (мс) | Ускорение | Эффективность (%) | Результат\n";
    for (int t = 1; t <= max_T; ++t) {
        auto start = high_resolution_clock::now();
        res_std[t] = integrate_std_thread(N, t);
        auto end = high_resolution_clock::now();
        time_std[t] = duration_cast<microseconds>(end - start).count() / 1000.0;

        double speedup = (t == 1) ? 1.0 : (time_std[1] / time_std[t]);
        double eff = (speedup / t) * 100.0;

        cout << setw(6) << t << " | "
            << setw(10) << fixed << setprecision(2) << time_std[t] << " | "
            << setw(9) << fixed << setprecision(4) << speedup << " | "
            << setw(17) << fixed << setprecision(2) << eff << " | "
            << setprecision(6) << res_std[t] << "\n";
    }

    system("pause");
    return 0;
}