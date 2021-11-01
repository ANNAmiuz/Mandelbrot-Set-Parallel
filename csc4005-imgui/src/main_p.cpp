#include <chrono>
#include <iostream>
#include <graphic/graphic.hpp>
#include <imgui_impl_sdl.h>
#include <vector>
#include <complex>
#include <thread>
#include <cstring>

#define DEBUGx
#define MIN_PER_THREAD 10 // min number for each thread
#define TEST1             // for grading
#define TEST2x            // for sbatch test
int THREAD = 0;

/*
 * In this program, there are two mode: Pthread parallel and Sequential BY passing thread number to the program (1 for sequential).
 * Pthread Parallel: each proc gets at least MIN_PER_THREAD * size to compute. Static allocation.
 */

struct Square
{
    std::vector<int> buffer;
    size_t length;

    explicit Square(size_t length) : buffer(length), length(length * length) {}

    void resize(size_t new_length)
    {
        buffer.assign(new_length * new_length, false);
        length = new_length;
    }

    auto &operator[](std::pair<size_t, size_t> pos)
    {
        return buffer[pos.second * length + pos.first];
    }
};

void exec_func(int startidx, int localsize, Square *buffer, int size, float scale, double x_center, double y_center, int k_value)
{
    double cx = static_cast<double>(size) / 2 + x_center;
    double cy = static_cast<double>(size) / 2 + y_center;
    double zoom_factor = static_cast<double>(size) / 4 * scale;
    for (int i = 0; i < size; ++i)
    {
        for (int j = startidx; j < startidx + localsize; ++j)
        {
            double x = (static_cast<double>(j) - cx) / zoom_factor;
            double y = (static_cast<double>(i) - cy) / zoom_factor;
            std::complex<double> z{0, 0};
            std::complex<double> c{x, y};
            int k = 0;
            do
            {
                z = z * z + c;
                k++;
            } while (norm(z) < 2.0 && k < k_value);
            buffer->operator[]({i, j}) = k;
        }
    }
}

void calculate(Square *buffer, int size, float scale, double x_center, double y_center, int k_value)
{
    std::vector<std::thread> thread_pool{};
    int i, offset, startidx, localsize;
    offset = 0;
    for (i = 0; i < THREAD; ++i)
    {
        startidx = offset;
        localsize = std::ceil(((float)size - i) / THREAD);
        offset += localsize;
#ifdef DEBUG
        std::cout << "offset: " << offset << std::endl;
#endif
        thread_pool.push_back(std::thread{
            exec_func, startidx, localsize, buffer, size, scale, x_center, y_center, k_value});
    }
    for (i = 0; i < THREAD; ++i)
        thread_pool[i].join();
}

static constexpr float MARGIN = 4.0f;
static constexpr float BASE_SPACING = 2000.0f;

int main(int argc, char **argv)
{
    if (argc != 5)
    {
        std::cerr << "usage: " << argv[0] << " <length> <THREAD_SIZE> <graph> <K_VALUE>" << std::endl;
        return 0;
    }
#ifdef TEST1
    std::cout << "Pthread Parellel" << std::endl;
#endif
    static long size = std::atol(argv[1]);
    THREAD = std::atoi(argv[2]);
    THREAD = (size >= THREAD * MIN_PER_THREAD) ? THREAD : std::ceil((double)size / MIN_PER_THREAD);
    if (THREAD <= 0 || size <= 0)
    {
        std::cerr << "<THREAD SIZE> or <length>" << THREAD << size << " is not greadter than 0 " << std::endl;
        return 0;
    }
    int graph = std::atoi(argv[3]);
    static int k_value = std::atoi(argv[4]);
    graphic::GraphicContext context{"Assignment 2"};
    Square canvas(100);
    size_t duration = 0;
    size_t pixels = 0;

    bool drawn = false;
    int size_passed = int(size);
    context.run([&](graphic::GraphicContext *context [[maybe_unused]], SDL_Window *)
                {
                    {
                        auto io = ImGui::GetIO();
                        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
                        ImGui::SetNextWindowSize(io.DisplaySize);
                        ImGui::Begin("Assignment 2", nullptr,
                                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
                        ImDrawList *draw_list = ImGui::GetWindowDrawList();
                        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
                                    ImGui::GetIO().Framerate);
                        static int center_x = -100;
                        static int center_y = -300;
                        //static int size = 800;
                        static float scale = 0.5;
                        static ImVec4 col = ImVec4(1.0f, 1.0f, 0.4f, 1.0f);
                        ImGui::DragInt("Center X", &center_x, 1, -4 * size, 4 * size, "%d");
                        ImGui::DragInt("Center Y", &center_y, 1, -4 * size, 4 * size, "%d");
                        ImGui::DragInt("Fineness", &size_passed, 10, 100, 1000, "%d");
                        ImGui::DragFloat("Scale", &scale, 1, 1, 100, "%.01f");
                        ImGui::DragInt("K", &k_value, 1, 100, 1000, "%d");
                        ImGui::ColorEdit4("Color", &col.x);
                        {
                            using namespace std::chrono;
                            auto spacing = BASE_SPACING / static_cast<float>(size);
                            auto radius = spacing / 2;
                            const ImVec2 p = ImGui::GetCursorScreenPos();
                            const ImU32 col32 = ImColor(col);
                            float x = p.x + MARGIN, y = p.y + MARGIN;
                            if (drawn == false)
                            {
                                drawn = true;
                                //std::cout<<drawn<<std::endl;
                                canvas.resize(size);
                                auto begin = high_resolution_clock::now();
                                calculate(&canvas, size, scale, center_x, center_y, k_value);
                                auto end = high_resolution_clock::now();
                                pixels += size*size;
                                duration += duration_cast<nanoseconds>(end - begin).count();
                                auto speed = static_cast<double>(pixels) / static_cast<double>(duration) * 1e9;
#ifdef TEST1
                                std::cout<< "Data size: "<<size<<std::endl;
                                std::cout<< "Thread number is: "<<THREAD<<std::endl;
                                std::cout << "K_VALUE is: " << k_value << std::endl;
                                std::cout << pixels << " pixels in " << duration << " nanoseconds\n";
                                std::cout << "speed: " << speed << " pixels per nanosecond" << std::endl;
#endif
#ifdef TEST2
                                std::cout<<speed<<std::endl;
#endif
                                
                            }
                            if (!graph) exit(0);
                            for (int i = 0; i < size; ++i)
                            {
                                for (int j = 0; j < size; ++j)
                                {
                                    if (canvas[{i, j}] == k_value)
                                    {
                                        draw_list->AddCircleFilled(ImVec2(x, y), radius, col32);
                                    }
                                    x += spacing;
                                }
                                y += spacing;
                                x = p.x + MARGIN;
                            }
                        }
                        ImGui::End();
                    } });
    return 0;
}
