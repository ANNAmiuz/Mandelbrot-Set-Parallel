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

#define MIN_PER_JOB 20 // workload unit for dynamic allocation
#define DEBUG1x
#define DEBUG2x
#define TEST1 // for grading
#define TEST2x  // for sbatch test

/*
 * MPI Parallel: each proc gets at least MIN_PER_JOB * size to compute each allocaton. Dynamic allocation.
 * Master & Slave paradigm: at least two proc to run.
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

void master(Square *buffer, long size, int wsize, bool &isend)
{
    int *p = buffer->buffer.data(); // start of the global buffer
    int jobs_num = std::ceil((double)size / MIN_PER_JOB);
    int *localsize_buf = (int *)malloc(jobs_num * sizeof(int));
    int *startidx_buf = (int *)malloc(jobs_num * sizeof(int));
    int *pack = (int *)malloc(2 * sizeof(int)); // pack start idx and local size
    int i, offset = 0;
    MPI_Request req;
    int terminator = jobs_num + 1;
    MPI_Bcast(&terminator, 1, MPI_INT, 0, MPI_COMM_WORLD);
    // initialize job queue
    for (i = 0; i < jobs_num - 1; ++i)
    {
        startidx_buf[i] = offset;
        localsize_buf[i] = MIN_PER_JOB;
        offset += MIN_PER_JOB;
    }
    startidx_buf[i] = offset;
    localsize_buf[i] = size % MIN_PER_JOB ? size % MIN_PER_JOB : MIN_PER_JOB;
    int count = 0, job_idx = 0, number_amount;
    int recv_startidx, recv_localsize;
    MPI_Status status;
    bool sent = false;

    for (i = 1; i < wsize; ++i)
    {
        if (job_idx >= jobs_num)
        {
            MPI_Isend(&(startidx_buf[job_idx]), 1, MPI_INT, i, terminator, MPI_COMM_WORLD, &req); //-1 to notice the slave: it is not a job and it should finish
        }
        else
        {
            if (sent)
                MPI_Wait(&req, MPI_STATUS_IGNORE);
            pack[0] = startidx_buf[job_idx];
            pack[1] = localsize_buf[job_idx];
            MPI_Isend(pack, 2, MPI_INT, i, job_idx, MPI_COMM_WORLD, &req);
            sent = true;
            count++; // the current distributed jobs
        }
        job_idx++; // get next job to allocate
    }

    while (count > 0)
    {
        MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_INT, &number_amount);
        recv_startidx = startidx_buf[status.MPI_TAG];
        recv_localsize = localsize_buf[status.MPI_TAG];
#ifdef DEBUG1
        std::cout << "[master] start idx is: " << recv_startidx << std::endl
                  << std::flush;
#endif
        MPI_Recv(p + recv_startidx * size, number_amount, MPI_INT, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        count--;
        if (job_idx < jobs_num)
        {
            MPI_Wait(&req, MPI_STATUS_IGNORE);
            pack[0] = startidx_buf[job_idx];
            pack[1] = localsize_buf[job_idx];
            MPI_Isend(pack, 2, MPI_INT, status.MPI_SOURCE, job_idx, MPI_COMM_WORLD, &req);
            job_idx++;
            count++;
        }
        else
            MPI_Isend(pack, 2, MPI_INT, status.MPI_SOURCE, terminator, MPI_COMM_WORLD, &req); //exceed the max job number to notice the slave: it is not a job and it should finish
    }
#ifdef DEBUG1
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    std::cout << "[master] " << rank << " finish all jobs at " << job_idx << std::endl
              << std::flush;
#endif
    isend = true;
    free(localsize_buf);
    free(startidx_buf);
    free(pack);
}

void slave(Square *buffer, long size, float scale, double x_center, double y_center, int k_value, bool &isend)
{
    int *p = buffer->buffer.data(); // start of the global buffer
    int start_idx, localsize;
    int terminator;
    MPI_Bcast(&terminator, 1, MPI_INT, 0, MPI_COMM_WORLD);
    int *pack = (int *)malloc(2 * sizeof(int));
    MPI_Request req;
    MPI_Status status;
    MPI_Recv(pack, 2, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
    int job_idx = status.MPI_TAG;

    while (job_idx != terminator)
    {
        start_idx = pack[0];
        localsize = pack[1];
#ifdef DEBUG2
        std::cout << "[slave] The start index is: " << start_idx << std::endl
                  << std::flush;
#endif
        exec_func(start_idx, localsize, buffer, size, scale, x_center, y_center, k_value);
        MPI_Isend(p + start_idx * size, localsize * size, MPI_INT, 0, job_idx, MPI_COMM_WORLD, &req);
        MPI_Recv(pack, 2, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        job_idx = status.MPI_TAG;
    }
#ifdef DEBUG2
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif
    isend = true;
    free(pack);
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
    if (wsize <= 1)
    {
        std::cerr << "usage: " << argv[0] << " at least two proc to run" << std::endl;
        return 0;
    }
    int graph = std::atoi(argv[2]);
    Square canvas(100);
    static int center_x = -100;
    static int center_y = -300;
    static long size = std::atol(argv[1]);
    static int k_value = std::atoi(argv[3]);
    if (size < MIN_PER_JOB)
    {
        std::cerr << "<LENGTH> " << argv[0] << " too small to display " << std::endl;
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
        std::cout << "MPI Dynamic Parallel" << std::endl;
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
                                    master(&canvas, size, wsize, isend);
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
    else if (rank != 0)
    {
        slave(&canvas, size, scale, center_x, center_y, k_value, isend);
    }
    MPI_Finalize();
    return 0;
}
