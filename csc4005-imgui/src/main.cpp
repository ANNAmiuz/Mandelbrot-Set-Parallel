#include <chrono>
#include <iostream>
#include <graphic/graphic.hpp>
#include <imgui_impl_sdl.h>
#include <vector>
#include <complex>
#include <mpi.h>
#include <cstring>
#include <math.h>
#include <unistd.h>

#define MPARA
#define SQUEx
#define DEBUGx
#define MIN_PER_PROC 10 // min number for each process
#define TEST1           // for grading
#define TEST2x          // for sbatch test

/*
 * In this program, there are two mode: MPI parallel and Sequential BY selecting the pragma above / assign 1 proc to run sequential.
 * MPI Parallel: each proc gets at least MIN_PER_PROC * size to compute. Static allocation.
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

void calculate(Square &buffer, long size, float scale, double x_center, double y_center, int k_value, int rank, int wsize, bool &isend)
{
#ifdef SQUE
    std::cout << rank << " " << wsize << std::endl;
    double cx = static_cast<double>(size) / 2 + x_center;
    double cy = static_cast<double>(size) / 2 + y_center;
    double zoom_factor = static_cast<double>(size) / 4 * scale;
    for (int i = 0; i < size; ++i)
    {
        for (int j = 0; j < size; ++j)
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
            buffer[{i, j}] = k;
        }
    }
    isend = true;
#endif
#ifdef MPARA
    if (size >= wsize * MIN_PER_PROC)
    {
        /* Calculate local workload for each of the proc */
        int localsize = std::ceil(((float)size - rank) / wsize);
        /* Calculate the count and displs for gatherv */
        int *scounts, *displs, *localsize_offset;
        scounts = (int *)malloc(wsize * sizeof(int));
        displs = (int *)malloc(wsize * sizeof(int));
        localsize_offset = (int *)malloc(wsize * sizeof(int));
        int cur_rank, offset = 0, local_offset = 0;
        for (cur_rank = 0; cur_rank < wsize; cur_rank++)
        {
            displs[cur_rank] = offset;
            localsize_offset[cur_rank] = local_offset;
            scounts[cur_rank] = size * std::ceil(((float)size - cur_rank) / wsize);
            local_offset += std::ceil(((float)size - cur_rank) / wsize);
            offset += scounts[cur_rank];
        }
        /* Calculate start */
        double cx = static_cast<double>(size) / 2 + x_center;
        double cy = static_cast<double>(size) / 2 + y_center;
        double zoom_factor = static_cast<double>(size) / 4 * scale;
        for (int i = 0; i < size; ++i)
        {
            for (int j = localsize_offset[rank]; j < localsize_offset[rank] + localsize; ++j)
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
                buffer[{i, j}] = k;
            }
        }
        int *p = buffer.buffer.data();
#ifdef DEBUG
        std::cout << "rank: " << rank << " p address " << p << std::endl; // no one finish
        std::cout << "rank: " << rank << " start address " << p + displs[rank] << std::endl;
#endif
        MPI_Gatherv((void *)(p + displs[rank]), (long)localsize * size, MPI_INT, p, scounts, displs, MPI_INT, 0, MPI_COMM_WORLD);
        isend = true;
#ifdef DEBUG
        std::cout << "rank: " << rank << " finish gather" << std::endl; // no one finish
#endif
    }
    /* Data size too small to split into different processes */
    else
    {
        /* Calculate local workload for each of the proc */
        int working_proc = std::ceil((float)size / MIN_PER_PROC);
        int localsize;
        if (size % MIN_PER_PROC)
            localsize = rank >= working_proc ? 0 : (rank == working_proc - 1 ? size % MIN_PER_PROC : MIN_PER_PROC);
        else
            localsize = rank >= working_proc ? 0 : MIN_PER_PROC;
        /* Calculate the count and displs for gatherv */
        int *scounts, *displs, *localsize_offset;
        scounts = (int *)malloc(wsize * sizeof(int));
        displs = (int *)malloc(wsize * sizeof(int));
        localsize_offset = (int *)malloc(wsize * sizeof(int));
        int cur_rank, offset = 0, local_offset = 0;
        for (cur_rank = 0; cur_rank < working_proc - 1; cur_rank++)
        {
            displs[cur_rank] = offset;
            localsize_offset[cur_rank] = local_offset;
            scounts[cur_rank] = size * MIN_PER_PROC;
            local_offset += MIN_PER_PROC;
            offset += scounts[cur_rank];
        }
        displs[cur_rank] = offset;
        localsize_offset[cur_rank] = local_offset;
        if (size % MIN_PER_PROC)
        {
            scounts[cur_rank] = size * (size % MIN_PER_PROC);
            local_offset += size % MIN_PER_PROC;
        }
        else
        {
            scounts[cur_rank] = size * MIN_PER_PROC;
            local_offset += MIN_PER_PROC;
        }
        offset += scounts[cur_rank];
        cur_rank++;
        for (; cur_rank < wsize; cur_rank++)
        {
            displs[cur_rank] = offset;
            localsize_offset[cur_rank] = local_offset;
            scounts[cur_rank] = 0;
            local_offset += 0;
            offset += scounts[cur_rank];
        }
#ifdef DEBUG
        if (rank == 0)
        {
            for (int i = 0; i < wsize; i++)
            {
                std::cout << "local offset: " << localsize_offset[i] << std::endl;
            }
        }
#endif
        /* Calculate start */
        if (rank < working_proc)
        {
            double cx = static_cast<double>(size) / 2 + x_center;
            double cy = static_cast<double>(size) / 2 + y_center;
            double zoom_factor = static_cast<double>(size) / 4 * scale;
            for (int i = 0; i < size; ++i)
            {
                for (int j = localsize_offset[rank]; j < localsize_offset[rank] + localsize; ++j)
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
                    buffer[{i, j}] = k;
                }
            }
        }
        int *p = buffer.buffer.data();
        MPI_Gatherv((void *)(p + displs[rank]), localsize * size, MPI_INT, p, scounts, displs, MPI_INT, 0, MPI_COMM_WORLD);
        isend = true;
    }
#endif
}

static constexpr float MARGIN = 4.0f;
static constexpr float BASE_SPACING = 2000.0f;

int main(int argc, char **argv)
{
    int rank, wsize;
    bool isend = false;
    if (argc != 4)
    {
        std::cerr << "usage: " << argv[0] << " <length> <GUI> <K_VALUE>" << std::endl;
        return 0;
    }
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &wsize);
    int graph = std::atoi(argv[2]);
    Square canvas(100);
    static int center_x = -100;
    static int center_y = -300;
    static long size = std::atol(argv[1]);
    static int k_value = std::atoi(argv[3]);
    if (size <= 0)
    {
        std::cerr << "usage: " << argv[0] << " <length> greater than 0 " << std::endl;
        return 0;
    }
    if (k_value <= 0)
    {
        std::cerr << "usage: " << argv[0] << " <K_VALUE> greater than 0 " << std::endl;
        return 0;
    }
    if (rank == 0)
    {
#ifdef TEST1
        std::cout << "MPI Static Parallel" << std::endl;
        std::cout << "Data size: " << size << std::endl;
        std::cout << "Core number is: " << wsize << std::endl;
        std::cout << "K_VALUE is: " << k_value << std::endl;
#endif
    }
    static float scale = 0.5;

    canvas.resize(size);
    if (0 == rank)
    {
        int size_passed = int(size);
        graphic::GraphicContext context{"Assignment 2"};
        size_t duration = 0;
        size_t pixels = 0;
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
                                auto begin = high_resolution_clock::now();
                                if (!isend)
                                {
                                    calculate(canvas, size, scale, center_x, center_y, k_value, rank, wsize, isend);
                                    auto end = high_resolution_clock::now();
                                    pixels += size*size;
                                    duration += duration_cast<nanoseconds>(end - begin).count();
                                    auto speed = static_cast<double>(pixels) / static_cast<double>(duration) * 1e9;
                                    {
#ifdef TEST1
                                        std::cout << pixels << " pixels in last " << duration << " nanoseconds\n";
                                        std::cout << "speed: " << speed << " pixels per second" << std::endl;
#endif
#ifdef TEST2
                                std::cout<<speed<<std::endl;
#endif
                                    }
                                }
                                if (!graph) {MPI_Finalize();exit(0);}
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
    }
#ifdef MPARA
    else if (rank != 0)
    {
        calculate(canvas, size, scale, center_x, center_y, k_value, rank, wsize, isend);
    }
#endif
    MPI_Finalize();
    return 0;
}
