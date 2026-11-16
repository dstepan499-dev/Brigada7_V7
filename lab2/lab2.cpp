#include <iostream>
#include <fstream>
#include <vector>
#include <queue>
#include <string>
#include <chrono>
#include <thread>
#include <cmath>
#define HAVE_STRUCT_TIMESPEC
#include <pthread.h>
#pragma comment(lib, "pthreadVCE2.lib")
using namespace std;
struct Task {
    int id;
    long long a;
    long long b;
    int simulated_delay_ms;
};
struct Result {
    int task_id;
    long long a;
    long long b;
    long long gcd;
    int node_id;
    long long calc_time_ms;
};
struct NodeChannel {
    queue<Result> q;
    pthread_mutex_t mutex;
    pthread_cond_t cv;
    bool finished;
    NodeChannel() : finished(false) {
        pthread_mutex_init(&mutex, nullptr);
        pthread_cond_init(&cv, nullptr);
    }
    ~NodeChannel() {
        pthread_mutex_destroy(&mutex);
        pthread_cond_destroy(&cv);
    }
};
struct NodeContext {
    int node_id;
    int buffer_capacity;
    int write_delay_ms;
    NodeChannel channel;
};
vector<Task> g_tasks;
size_t g_task_index = 0;
pthread_mutex_t g_input_mutex = PTHREAD_MUTEX_INITIALIZER;
ofstream g_output_file;
pthread_mutex_t g_output_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER;
long long calculate_gcd(long long a, long long b) {
    a = std::llabs(a);
    b = std::llabs(b);
    if (a == 0 && b == 0) return 0;
    while (b != 0) {
        long long temp = b;
        b = a % b;
        a = temp;
    }
    return a;
}
void log_message(const string& msg) {
    pthread_mutex_lock(&g_log_mutex);
    cout << msg << endl;
    pthread_mutex_unlock(&g_log_mutex);
}
void flush_buffer_to_file(int node_id, vector<Result>& buffer, int write_delay_ms) {
    if (buffer.empty()) return;
    pthread_mutex_lock(&g_output_mutex);
    if (write_delay_ms > 0) {
        this_thread::sleep_for(chrono::milliseconds(write_delay_ms));
    }
    for (const auto& r : buffer) {
        g_output_file << "Задача " << r.task_id << ": НОД(" << r.a << ", " << r.b
            << ") = " << r.gcd
            << " [Узел " << r.node_id << ", время счета: " << r.calc_time_ms << " мс]\n";
    }
    g_output_file.flush();
    pthread_mutex_unlock(&g_output_mutex);
    log_message("[Узел " + to_string(node_id) + " - Writer] Записана группа из " +
        to_string(buffer.size()) + " результатов в общий файл");
    buffer.clear();
}
void* worker_thread_func(void* arg) {
    NodeContext* ctx = static_cast<NodeContext*>(arg);
    while (true) {
        Task current_task;
        bool has_task = false;
        pthread_mutex_lock(&g_input_mutex);
        if (g_task_index < g_tasks.size()) {
            current_task = g_tasks[g_task_index++];
            has_task = true;
        }
        pthread_mutex_unlock(&g_input_mutex);
        if (!has_task) {
            break;
        }
        auto start_time = chrono::high_resolution_clock::now();
        if (current_task.simulated_delay_ms > 0) {
            this_thread::sleep_for(chrono::milliseconds(current_task.simulated_delay_ms));
        }
        long long gcd_val = calculate_gcd(current_task.a, current_task.b);
        auto end_time = chrono::high_resolution_clock::now();
        long long duration = chrono::duration_cast<chrono::milliseconds>(end_time - start_time).count();
        Result res{ current_task.id, current_task.a, current_task.b, gcd_val, ctx->node_id, duration };
        pthread_mutex_lock(&ctx->channel.mutex);
        ctx->channel.q.push(res);
        pthread_cond_signal(&ctx->channel.cv);
        pthread_mutex_unlock(&ctx->channel.mutex);
    }
    pthread_mutex_lock(&ctx->channel.mutex);
    ctx->channel.finished = true;
    pthread_cond_signal(&ctx->channel.cv);
    pthread_mutex_unlock(&ctx->channel.mutex);
    return nullptr;
}
void* writer_thread_func(void* arg) {
    NodeContext* ctx = static_cast<NodeContext*>(arg);
    vector<Result> local_buffer;
    while (true) {
        Result current_res;
        bool has_item = false;
        pthread_mutex_lock(&ctx->channel.mutex);
        while (ctx->channel.q.empty() && !ctx->channel.finished) {
            pthread_cond_wait(&ctx->channel.cv, &ctx->channel.mutex);
        }
        if (!ctx->channel.q.empty()) {
            current_res = ctx->channel.q.front();
            ctx->channel.q.pop();
            has_item = true;
        }
        else if (ctx->channel.finished) {
            pthread_mutex_unlock(&ctx->channel.mutex);
            break;
        }
        pthread_mutex_unlock(&ctx->channel.mutex);
        if (has_item) {
            local_buffer.push_back(current_res);
            if (static_cast<int>(local_buffer.size()) >= ctx->buffer_capacity) {
                flush_buffer_to_file(ctx->node_id, local_buffer, ctx->write_delay_ms);
            }
        }
    }
    if (!local_buffer.empty()) {
        flush_buffer_to_file(ctx->node_id, local_buffer, ctx->write_delay_ms);
    }
    return nullptr;
}
int main(int argc, char* argv[]) {
    setlocale(LC_ALL, "Russian");
    ifstream infile("input.txt");
    if (!infile.is_open()) {
        cerr << "Ошибка: невозможно открыть input.txt!" << endl;
        return 1;
    }
    long long a, b;
    int delay;
    int task_counter = 1;
    while (infile >> a >> b) {
        delay = 0;
        if (infile.peek() != '\n' && infile >> delay) {}
        g_tasks.push_back(Task{ task_counter++, a, b, delay });
    }
    infile.close();
    if (g_tasks.empty()) {
        cerr << "Ошибка: входной файл пуст!" << endl;
        return 1;
    }
    g_output_file.open("output.txt", ios::out | ios::trunc);
    if (!g_output_file.is_open()) {
        cerr << "Ошибка: невозможно открыть output.txt для записи!" << endl;
        return 1;
    }
    const int NUM_NODES = 2;
    const int BUFFER_CAPACITY = 3;
    int simulated_io_delay = 0;
    if (argc > 1) {
        simulated_io_delay = atoi(argv[1]);
    }

    cout << " Число узлов (пар потоков): " << NUM_NODES << endl;
    cout << " Всего потоков: " << NUM_NODES * 2 << endl;
    cout << " Емкость пакета буфера: " << BUFFER_CAPACITY << endl;
    cout << " Задач к обработке: " << g_tasks.size() << endl;
    cout << "\n" << endl;
    auto total_start = chrono::high_resolution_clock::now();
    vector<NodeContext> nodes(NUM_NODES);
    vector<pthread_t> worker_threads(NUM_NODES);
    vector<pthread_t> writer_threads(NUM_NODES);
    for (int i = 0; i < NUM_NODES; ++i) {
        nodes[i].node_id = i + 1;
        nodes[i].buffer_capacity = BUFFER_CAPACITY;
        nodes[i].write_delay_ms = simulated_io_delay;
        pthread_create(&worker_threads[i], nullptr, worker_thread_func, &nodes[i]);
        pthread_create(&writer_threads[i], nullptr, writer_thread_func, &nodes[i]);
    }
    for (int i = 0; i < NUM_NODES; ++i) {
        pthread_join(worker_threads[i], nullptr);
        pthread_join(writer_threads[i], nullptr);
    }
    auto total_end = chrono::high_resolution_clock::now();
    long long total_time = chrono::duration_cast<chrono::milliseconds>(total_end - total_start).count();
    g_output_file.close();
    pthread_mutex_destroy(&g_input_mutex);
    pthread_mutex_destroy(&g_output_mutex);
    pthread_mutex_destroy(&g_log_mutex);
    cout << "\n" << endl;
    cout << " Общее время работы программы: " << total_time << " мс" << endl;
    cout << " Результаты записаны в общий файл output.txt" << endl;
    cout << "\n" << endl;
    return 0;
}