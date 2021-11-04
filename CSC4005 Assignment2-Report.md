# CSC4005 Assignment2-Report




## **How to compile & execute?**

### File Tree

​	The project is modified on the template provided on BB.

```shell
.
├── csc4005-imgui
│   ├── CMakeLists.txt
│   ├── freetype
│   ├── imgui
│   ├── include
│   │   └── graphic
│   │       └── graphic.hpp
│   └── src
│       ├── graphic.cpp
│       ├── main.cpp             # source code for MPI-Static
│       ├── main_d.cpp           # source code for MPI-Dynamic
│       ├── main_p.cpp			 # source code for Pthread-Static
│       └── main_p_dynamic.cpp   # source code for Pthread-Dynamic
└── report.pdf                   # report

127 directories, 899 files

```

### Execution Steps

```shell
# it is too slow to build under /pvfsmnt, you can choose to compile it under any directory, but you need to copy the programs and shared library under /pvfsmnt to execute
cd /path_to_project/
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j4

# copy to /pvfsmnt
# <test_program>: csc4005_imgui & csc4005_imguip
cp -r /path_to_project/build/<test_program> /some_path_under_pvfsmnt
cd /some_path_under_pvfsmnt

#acquire multi-core resource by salloc or sbatch
#specify the <datasize>=side length of the square

#to run the program using Pthread (static allocation)
./csc4005_imguip <DATA_SIZE> <NUM_OF_THREAD> <GRAPH_OR_NOT_BY_0/1> <K_VALUE>
#to run the program using Pthread (dynamic allocation)
./csc4005_imguipd <DATA_SIZE> <NUM_OF_THREAD> <GRAPH_OR_NOT_BY_0/1> <K_VALUE>

#to run the program using MPI (static allocation)
mpirun-gui csc4005_imgui <DATA_SIZE> <GRAPH_OR_NOT_BY_0/1> <K_VALUE>

#to run the program using MPI (dynamic allocation)
mpirun-gui csc4005_imguid <DATA_SIZE> <GRAPH_OR_NOT_BY_0/1> <K_VALUE>
```

> **DATA_SIZE**: The data size $l$ is defined as **the side length of the square to display**. With data size $l$, there are in total $l^2$ independent point position to be calculated in this Mandelbrot Set Computation.
>
> **NUM_OF_THREAD**:  recommended to **be the number of cores of current execution environment ($n<32$)**. If you were to test the program on more than $32$ threads, there may be **frequent thread switch**. 
>
> **GRAPH_OR_NOT**: if you want to see a graphical output, assign it to 1. Otherwise, assign it to 0.
>
> **K_VALUE**: the maximum iteration times per pixel.



**Pthread STATIC Allocation Program** csc4005_imguip:

​	Using Pthread to accelerate the Mandelbrot Set Computation in the server. The source code is in the file **main_p.cpp**. The job is statically allocated to each thread before the threads' execution.



**Pthread DYNAMIC Allocation Program** csc4005_imguipd:

​	Using Pthread to accelerate the Mandelbrot Set Computation in the server. The source code is in the file **main_p_dynamic.cpp**. The job is dynamically allocated to each thread during the execution.



**MPI Program** csc4005_imgui:

​	Using MPI to accelerate the Mandelbrot Set Computation in the server. The source code is in the file **main.cpp**. The job is statically allocated to each rank before the execution.



**MPI Program** csc4005_imgudi:

​	Using MPI to accelerate the Mandelbrot Set Computation in the server. The source code is in the file **main_d.cpp**. The job is dynamically allocated to each rank by root rank during execution (**master-slave**).





## 1. Introduction

​	In this assignment, we implemented the Mandelbrot Set Computation, which can be executed on the campus cluster (a cluster that provides multiple nodes for program running). The code is implemented in a robust and correct way, which functions well to display the correct graphs under different data size:

![image-20211030222727294](CSC4005 Assignment2-Report.assets/image-20211030222727294.png)

​	***Four different implementations (MPI to accelerate the computation and Pthread in CPP to accelerate the computation, with static workload allocation and dynamic workload allocation separately) are implemented for performance analysis.*** Then we test the performance of codes under different conditions (sorting data size, nodes for running, k value), and analyse the results. The performance is evaluated by the computation speed (pixels per second), and the parallel speedup over sequential version. 

​	In the second and third part, the ***implementation method*** of the Mandelbrot Set Computation is illustrated (MPI-Static, MPI-Dynamic, Pthread-Static, Pthread-Dynamic). Some implementation ***details*** are explained, and ***flow charts*** of the steps for the parallel computation is provided. 

​	In the fourth part, we give the results of ***experiments and performance analysis***. For each version of implementation (***MPI & Pthread***, **Static and Dynamic**), the computation speed (pixels per second), and the parallel speedup over sequential version, are recorded in ***tables***. To give a clear view, some statistical **charts** are provided. And some explanations for the performance differences are provided. In the explanations, we <u>*analyse the relations between data size and speedup on different core configurations, the reason for bad speedup, and the performance comparison between different implementation.*</u>

​	In the fifth part, we try to improve the four implementations based on our experiment results to get a better performance.

​	In the sixth part, we give the ***conclusion*** of this assignment.

​	In the appendix, some screenshots of the programs running on different configurations are provided to prove a good functionality of all those implementations.



## 2. Method -MPI Program

### How to structure the workload

​	We define an object class ***Square*** to simulate the total workload in the Mandelbrot Set Computation. The underlying data structure of this ***Square*** is the ***Vector*** of CPP. Each element of the ***Square*** is the status of this pixel after computation. The index of an element in this vector can be transformed into its row and column index in the total square.



### Who should receive the input data

*<u>Input to all</u> instead of input to one:*

​	The program is designed to ***get the input data in all the processes*** , instead of following the conventions that only get input data from process ranked 0. Since the input data are just the data size $l$, and some constant for GUI display​​, input to all can help reduce the communication IO overhead among all processes. Otherwise, all the processes will be involved in the broadcast of the data size $l$​​​​​.



### **Job Allocation and Execution**

#### Static Allocation

​	Each process can calculate the computation area that it take charge of. Therefore, before the execution, **all the processes can calculate these initial positions for the later computation instead of receiving from root process**. After performing the computation in all processes, the root process gathers all the results from all processes by **MPI_gatherv()**.

#### Dynamic Allocation

​	In the **master-slave** model, **master process should send workload unit to all slave processes**. Therefore, master process performs the job division at first, split the total job into a job list that can be allocated to all slaves (the initial position for computation and the total amount for each computation). 



### **Workload Distribution**

#### Static Allocation

​	We assume the initial data set as many pixels, which in together form a square graph. Therefore, the total workload is the complete square, and each process should **take charge of computation of a sub-rectangle** of this complete square.

​	We try to distribute this square to all processes as equally as possible. To divide a square to many sub-rectangles, we choose to divide its side length to all processes. The divided length $l_{i}$​​, which should be larger than the minimum divided workload $l_{min}$,together with the original length $l$, can be used to calculate the local workload $W_{i}$ of process ranked $i$. $W_i$ can be considered as the total number of points to be calculated in process ranked $i$.
$$
W_i = l \times l_i
$$
​	The workload distribution algorithm can be considered as the method that distributing $l$ to $n$ parts as equally as possible, where $l$ is the square length and $n$​​ is the number of processes. Here is the local workload computation method. 
$$
l_i = \lceil \frac {l-i}{n}\rceil
$$

​	It it possible that the $li < l_{min}$. In this situation, assume:
$$
m = \lceil \frac {l}{l_{min}}\rceil
$$
, then we distribute $l_{min}$ to the first $m$​ processes.

---------------- *Job allocation computed by all processes*---------------

```cpp
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
```



#### Dynamic Allocation

​	In the dynamic allocation, we divide the side of the square $l$ into many pieces $l_u$, which is the job unit for each allocation. The master process ranked 0 divided $l$ into $m$ pieces, with each piece $l_u$.
$$
 l=m \times l_u
$$
​	All pieces of work are stored in the job list maintained by the master process. It pass the **global start index and computation size to slave processes**. Slave processes compute based on the global start index. After computation, a slave will send the computed array to master. When the master finishes receiving the array, it tries to allocate the next job in the job list to that slave, if there are jobs remained to perform.

---------------- *Job allocation computed by master process*---------------

```cpp
// initialize job queue
    for (i = 0; i < jobs_num - 1; ++i)
    {
        startidx_buf[i] = offset;
        localsize_buf[i] = MIN_PER_JOB;
        offset += MIN_PER_JOB;
    }
    startidx_buf[i] = offset;
    localsize_buf[i] = size % MIN_PER_JOB ? size % MIN_PER_JOB : MIN_PER_JOB;

// initially send jobs to all slave processes
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

// the main loop to receive results and allocate new job
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
```



### **How to integrate the local computation into global computation**

​	Since the underlying data structure of square is a CPP vector, we should **utilize the locality** of speed up the memory operation. Notice that for a point with index ${x, y}$​ in the square, its index $i_{v}$​ in the vector is $y \times l + x$​​​​. To expose more locality in this program, we use the **1-D** view of the square, try to make the process compute a contiguous array of a pre-set size starting from its assigned global start index.

---------------- *Implementation: carefully choose a loop order to provide more locality* ---------------

```cpp
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
```



### How to gather data from all ranks to rank0

#### Static Allocation

​	Use ***MPI_Gatherv() to collect all the local computed array*** from all processes to root process ranked 0.

#### Dynamic Allocation

​	Use **MPI_Recv()** to receive the local computed array from one slave process per time.



###  **Flow Chart**

​	The program method can be illustrated by the following flow chart. 

#### Static Allocation

![image-20211101093514570](CSC4005 Assignment2-Report.assets/image-20211101093514570.png)

Some Drawbacks: 

1. Each process keeps a local Square, while only part of the Square is used in the local computation.
2. The local computations on different processes are imbalanced. In some processes, there may be more pixels that reached the threshold (k_value) to stop.

#### Dynamic Allocation

![image-20211101094611322](CSC4005 Assignment2-Report.assets/image-20211101094611322.png)

Some Drawbacks: 

1. Each process keeps a local Square, while only part of the Square is used in the local computation.





## 3. Method - Pthread Program



### How to structure the workload

​	We define an object class ***Square*** to simulate the total workload in the Mandelbrot Set Computation. The underlying data structure of this ***Square*** is the ***Vector*** of CPP. Each element of the ***Square*** is the status of this pixel after computation. The index of an element in this vector can be transformed into its row and column index in the total square.



### Who should receive the input data

*<u>Input to one</u> instead of input to all:*

​	Since we can make use of the shared memory characteristics of Pthread, there is no need to keep a data copy in all threads. Instead, we decided that the **input data should be kept by the main thread, and the peer threads can have access to it.​​​​**



### **Job Allocation and Execution**

#### Static Allocation

​	The main thread calculates the local workload for each threads: mainly the computation global start index and computation local size. And then the main thread creates $n$​ peer threads, assignment the workload to each thread, and joins all the threads after all peer threads completed.

#### Dynamic Allocation

​	The main thread initializes a job queue and maintains a thread pool with $n$​​ peer threads, in which all peer threads compete to take a job from the job queue, if there are still jobs to do. If there is no job left, the peer threads ends. The main thread joins all of them, displays the results in GUI, and outputs the time and speed measurement.  



### How to distribute workload to each thread

#### Static Allocation

​	We divide the whole working square (1-D view) into $n$​​​​​ pieces with size $t_i$​​, where $n$​​ is passed into the program as the number of working peer threads. For each pieces, as what we did in the MPI-Static program (mentioned above), the minimum size $t'$​​ is pre-set. ($t_i >= t'$​​) . And the sizes of all pieces are set as balanced as possible. For each piece, we record the global start index and the local size. And the main thread creates at most $n$​​​​ peer threads and assign one piece to each peer thread. 

-------------------*Main thread assign job to all peers statically*-------------------------

```cpp
for (i = 0; i < THREAD; ++i)
    {
        startidx = offset;
        localsize = std::ceil(((float)size - i) / THREAD);
        offset += localsize;
        thread_pool.push_back(std::thread{
            exec_func, startidx, localsize, buffer, size, scale, x_center, y_center, k_value});
    }
```

#### Dynamic Allocation

​	The main thread firstly initialize a **global job queue**, in which each job is symbolized by their global start index and size. Then it creates $n$​​ peer threads as the arguments indicated. All its peer threads compete with each other to take a job from the global job queue. When there is no job left in the queue, all peer threads join and the main thread return to display results.

-------------------*All peer working threads compete to take a job from queue*-----------------

```cpp
int startidx, localsize, current = tid;
    while (current < jobs_num)
    {
        startidx = startidx_buf[current];
        localsize = localsize_buf[current];
        exec_func(startidx, localsize, buffer, size, scale, x_center, y_center, k_value);
        job_mutex.lock();
        current = job_idx;
        job_idx++;
        job_mutex.unlock();
    }
```



### **How to integrate the local computation into global computation**

​	Due the shared memory of Pthread, all the peer threads can **directly perform their local computation on the global square**. There is no need to integrate the local computation into a global result after all peer threads compelete.



### **How to avoid conflicts between processes**

#### Static Allocation

​	All peer threads compute the pre-set area of local Square. There is **no conflict** between threads.

#### Dynamic Allocation

​	All peer threads try to take a job from the job queue, which will move the current pointer to the next job. We use a **mutex** to protect the critical section -- the **take** operations from the job queue to avoid conflicts.

```cpp
int startidx, localsize, current = tid;
    while (current < jobs_num)
    {
        startidx = startidx_buf[current];
        localsize = localsize_buf[current];
        exec_func(startidx, localsize, buffer, size, scale, x_center, y_center, k_value);
        job_mutex.lock();   //critical secttion start
        current = job_idx;
        job_idx++;
        job_mutex.unlock();  //critical section end
    }
```



### **Flow Chart**

#### Static Allocation

![image-20211101161126324](CSC4005 Assignment2-Report.assets/image-20211101161126324.png)

#### Dynamic Allocation

![image-20211101160858549](CSC4005 Assignment2-Report.assets/image-20211101160858549.png)





## 4. Experiments and Analysis

#### Experiment 1: MPI-Static

Here are some of the results of execution time in this version.

##### *The **Speed** with Different Data Size and Number of Cores (Pixels / Second)*

****

**Iteration Threshold = 100**

| Datasize\Proc | 1        | 2        | 4        | 8        | 16       | 32       | 64       | 96       | 128      |
| ------------- | -------- | -------- | -------- | -------- | -------- | -------- | -------- | -------- | -------- |
| 400           | 1.01E+08 | 1.02E+08 | 9.94E+07 | 9.99E+07 | 1.01E+08 | 1.01E+08 | 1.00E+08 | 1.01E+08 | 1.01E+08 |
| 800           | 2.04E+07 | 2.05E+07 | 2.04E+07 | 2.04E+07 | 2.04E+07 | 2.03E+07 | 2.04E+07 | 2.04E+07 | 2.09E+07 |
| 1600          | 2.09E+07 | 2.08E+07 | 2.08E+07 | 2.08E+07 | 2.09E+07 | 2.09E+07 | 2.07E+07 | 2.09E+07 | 2.08E+07 |
| 3200          | 2.00E+07 | 1.99E+07 | 2.00E+07 | 1.99E+07 | 1.99E+07 | 2.00E+07 | 1.98E+07 | 2.01E+07 | 2.00E+07 |
| 6400          | 1.84E+07 | 1.84E+07 | 1.84E+07 | 1.84E+07 | 1.84E+07 | 1.84E+07 | 1.84E+07 | 1.84E+07 | 1.84E+07 |

**Iteration Threshold = 500**

| Datasize\Proc | 1        | 2        | 4        | 8        | 16       | 32       | 64       | 96       | 128      |
| ------------- | -------- | -------- | -------- | -------- | -------- | -------- | -------- | -------- | -------- |
| 400           | 1.00E+08 | 1.01E+08 | 1.00E+08 | 1.00E+08 | 9.99E+07 | 1.00E+08 | 1.00E+08 | 9.98E+07 | 9.97E+07 |
| 800           | 5.43E+06 | 5.44E+06 | 5.43E+06 | 5.43E+06 | 5.43E+06 | 5.43E+06 | 5.42E+06 | 5.47E+06 | 5.43E+06 |
| 1600          | 5.46E+06 | 5.46E+06 | 5.45E+06 | 5.45E+06 | 5.45E+06 | 5.45E+06 | 5.45E+06 | 5.45E+06 | 5.45E+06 |
| 3200          | 5.39E+06 | 5.39E+06 | 5.38E+06 | 5.39E+06 | 5.38E+06 | 5.38E+06 | 5.39E+06 | 5.39E+06 | 5.39E+06 |
| 6400          | 5.27E+06 | 5.27E+06 | 5.27E+06 | 5.27E+06 | 5.27E+06 | 5.27E+06 | 5.27E+06 | 5.27E+06 | 5.27E+06 |

**Iteration Threshold = 1000**

| Datasize\Proc | 1        | 2        | 4        | 8        | 16       | 32       | 64       | 96       | 128      |
| ------------- | -------- | -------- | -------- | -------- | -------- | -------- | -------- | -------- | -------- |
| 400           | 1.00E+08 | 1.02E+08 | 1.00E+08 | 1.00E+08 | 1.02E+08 | 9.93E+07 | 1.00E+08 | 1.01E+08 | 9.99E+07 |
| 800           | 2.84E+06 | 2.84E+06 | 2.84E+06 | 2.84E+06 | 2.85E+06 | 2.84E+06 | 2.85E+06 | 2.84E+06 | 2.85E+06 |
| 1600          | 2.84E+06 | 2.85E+06 | 2.84E+06 | 2.84E+06 | 2.84E+06 | 2.84E+06 | 2.84E+06 | 2.84E+06 | 2.84E+06 |
| 3200          | 2.83E+06 | 2.83E+06 | 2.83E+06 | 2.83E+06 | 2.83E+06 | 2.83E+06 | 2.83E+06 | 2.83E+06 | 2.83E+06 |
| 6400          | 2.80E+06 | 2.79E+06 | 2.79E+06 | 2.80E+06 | 2.79E+06 | 2.79E+06 | 2.79E+06 | 2.79E+06 | 2.79E+06 |



##### *The **Speedup** with Different Data Size*

****

**Iteration Threshold = 100**

| Datasize\Proc | 1    | 2    | 4    | 8    | 16   | 32   | 64   | 96   | 128  |
| ------------- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| 400           | 1.00 | 1.01 | 0.98 | 0.98 | 1.00 | 0.99 | 0.99 | 0.99 | 0.99 |
| 800           | 1.00 | 1.01 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.03 |
| 1600          | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 0.99 | 1.00 | 1.00 |
| 3200          | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 0.99 | 1.01 | 1.00 |
| 6400          | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 |

**Iteration Threshold = 500**

| Datasize\Proc | 1    | 2    | 4    | 8    | 16   | 32   | 64   | 96   | 128  |
| ------------- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| 400           | 1.00 | 1.01 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 0.99 |
| 800           | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.01 | 1.00 |
| 1600          | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 |
| 3200          | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 |
| 6400          | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 |

**Iteration Threshold = 1000**

| Datasize\Proc | 1    | 2    | 4    | 8    | 16   | 32   | 64   | 96   | 128  |
| ------------- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| 400           | 1.00 | 1.02 | 1.00 | 1.00 | 1.02 | 0.99 | 1.00 | 1.01 | 1.00 |
| 800           | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 |
| 1600          | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 |
| 3200          | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 |
| 6400          | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 |



##### Analysis

​	The speedups with all those configurations are **not significant**. The possible reason is that, there are some processes in this implementation take the heaviest work, which becomes the **bottleneck** of the computation. Although we add more processes to perform the job, the processes who take the heaviest work still have too much burden. Those processes slow down the overall performance of the program. Besides, due to the overhead of calculating local job (start index and size) and gathering local results into a global result, some running on multi-core are even slower than the sequential program.

​	Therefore, the performance of MPI-Static allocation is not good. The main reason is the imbalance of workload distribution, leading to the inefficiency of multi-core running.



#### Experiment 2: MPI-Dynamic

##### *The **Speed** with Different Data Size and Number of Cores (Pixels / Second)*

****

**Iteration Threshold = 100**

| Datasize\Proc | 1    | 2        | 4        | 8        | 16       | 32       | 64       | 96       | 128      |
| ------------- | ---- | -------- | -------- | -------- | -------- | -------- | -------- | -------- | -------- |
| 400           | /    | 4.89E+07 | 1.54E+08 | 1.78E+08 | 1.17E+08 | 8.52E+07 | 7.64E+06 | 7.06E+06 | 4.59E+06 |
| 800           | /    | 1.95E+07 | 5.84E+07 | 1.09E+08 | 1.19E+08 | 1.18E+08 | 1.74E+07 | 2.35E+07 | 1.60E+07 |
| 1600          | /    | 2.05E+07 | 6.08E+07 | 1.34E+08 | 2.09E+08 | 2.11E+08 | 3.65E+07 | 7.21E+07 | 5.41E+07 |
| 3200          | /    | 2.08E+07 | 6.19E+07 | 1.42E+08 | 2.72E+08 | 3.81E+08 | 1.09E+08 | 2.18E+08 | 1.16E+08 |
| 6400          | /    | 2.08E+07 | 6.23E+07 | 1.45E+08 | 2.90E+08 | 4.65E+08 | 3.16E+08 | 4.55E+08 | 2.99E+08 |

**Iteration Threshold = 500**

| Datasize\Proc | 1    | 2        | 4        | 8        | 16       | 32       | 64       | 96       | 128      |
| ------------- | ---- | -------- | -------- | -------- | -------- | -------- | -------- | -------- | -------- |
| 400           | /    | 5.19E+07 | 1.53E+08 | 1.76E+08 | 1.21E+08 | 8.48E+07 | 7.66E+06 | 5.73E+06 | 4.45E+06 |
| 800           | /    | 5.37E+06 | 1.54E+07 | 2.67E+07 | 2.70E+07 | 2.69E+07 | 1.89E+07 | 2.42E+07 | 1.59E+07 |
| 1600          | /    | 5.43E+06 | 1.62E+07 | 3.32E+07 | 5.06E+07 | 5.09E+07 | 3.81E+07 | 4.95E+07 | 4.98E+07 |
| 3200          | /    | 5.45E+06 | 1.63E+07 | 3.68E+07 | 6.80E+07 | 9.69E+07 | 5.89E+07 | 8.50E+07 | 8.45E+07 |
| 6400          | /    | 5.45E+06 | 1.63E+07 | 3.80E+07 | 7.58E+07 | 1.37E+08 | 1.36E+08 | 1.67E+08 | 1.49E+08 |

**Iteration Threshold = 1000**

| Datasize\Proc | 1    | 2        | 4        | 8        | 16       | 32       | 64       | 96       | 128      |
| ------------- | ---- | -------- | -------- | -------- | -------- | -------- | -------- | -------- | -------- |
| 400           | /    | 4.86E+07 | 1.53E+08 | 1.51E+08 | 1.21E+08 | 8.55E+07 | 7.73E+06 | 6.84E+06 | 4.61E+06 |
| 800           | /    | 2.82E+06 | 7.89E+06 | 1.37E+07 | 1.38E+07 | 1.38E+07 | 1.38E+07 | 1.38E+07 | 1.38E+07 |
| 1600          | /    | 2.84E+06 | 8.33E+06 | 1.72E+07 | 2.61E+07 | 2.61E+07 | 2.34E+07 | 2.59E+07 | 2.59E+07 |
| 3200          | /    | 2.84E+06 | 8.52E+06 | 1.89E+07 | 3.51E+07 | 5.00E+07 | 3.92E+07 | 4.67E+07 | 4.65E+07 |
| 6400          | /    | 2.84E+06 | 8.53E+06 | 1.99E+07 | 3.91E+07 | 7.12E+07 | 8.38E+07 | 9.12E+07 | 8.58E+07 |



##### *The **Speedup** with Different Data Size*

****

**Iteration Threshold = 100**

| Datasize\Proc | 1    | 2    | 4    | 8    | 16    | 32    | 64    | 96    | 128   |
| ------------- | ---- | ---- | ---- | ---- | ----- | ----- | ----- | ----- | ----- |
| 400           | 1.00 | 0.48 | 1.52 | 1.75 | 1.15  | 0.84  | 0.08  | 0.07  | 0.05  |
| 800           | 1.00 | 0.96 | 2.87 | 5.34 | 5.85  | 5.81  | 0.86  | 1.15  | 0.79  |
| 1600          | 1.00 | 0.98 | 2.91 | 6.40 | 10.03 | 10.13 | 1.75  | 3.45  | 2.59  |
| 3200          | 1.00 | 1.04 | 3.10 | 7.11 | 13.62 | 19.10 | 5.47  | 10.91 | 5.80  |
| 6400          | 1.00 | 1.13 | 3.38 | 7.84 | 15.74 | 25.20 | 17.14 | 24.69 | 16.21 |

![image-20211101204604037](CSC4005 Assignment2-Report.assets/image-20211101204604037.png)

**Iteration Threshold = 500**

| Datasize\Proc | 1    | 2    | 4    | 8    | 16   | 32   | 64   | 96   | 128  |
| ------------- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| 400           | 1.00 | 0.51 | 1.50 | 1.73 | 1.19 | 0.84 | 0.08 | 0.06 | 0.04 |
| 800           | 1.00 | 0.26 | 0.75 | 1.31 | 1.33 | 1.32 | 0.93 | 1.19 | 0.78 |
| 1600          | 1.00 | 0.26 | 0.78 | 1.59 | 2.42 | 2.44 | 1.83 | 2.37 | 2.38 |
| 3200          | 1.00 | 0.27 | 0.82 | 1.84 | 3.40 | 4.85 | 2.95 | 4.26 | 4.23 |
| 6400          | 1.00 | 0.30 | 0.89 | 2.06 | 4.11 | 7.43 | 7.38 | 9.03 | 8.06 |

![image-20211101204442361](CSC4005 Assignment2-Report.assets/image-20211101204442361.png)

**Iteration Threshold = 1000**

| Datasize\Proc | 1    | 2    | 4    | 8    | 16   | 32   | 64   | 96   | 128  |
| ------------- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| 400           | 1.00 | 0.48 | 1.50 | 1.49 | 1.20 | 0.84 | 0.08 | 0.07 | 0.05 |
| 800           | 1.00 | 0.14 | 0.39 | 0.67 | 0.68 | 0.68 | 0.68 | 0.68 | 0.68 |
| 1600          | 1.00 | 0.14 | 0.40 | 0.82 | 1.25 | 1.25 | 1.12 | 1.24 | 1.24 |
| 3200          | 1.00 | 0.14 | 0.43 | 0.95 | 1.76 | 2.50 | 1.96 | 2.34 | 2.33 |
| 6400          | 1.00 | 0.15 | 0.46 | 1.08 | 2.12 | 3.86 | 4.55 | 4.94 | 4.65 |

![image-20211101204646879](CSC4005 Assignment2-Report.assets/image-20211101204646879.png)

##### Analysis

​	Overall, the speedups with all those configurations are **more significant** than the static allocation version. In some of those configurations, some tests even achieved super-linear speedup. The possible reason is that, it relieves the imbalance of workload distribution among all processes. 

​	According to the table, it will be **easier to achieve speedup** when the core number or the data size is relatively large, or the iteration limit is relatively small. However, the overall speedup is not very good when the core number or data size is small. Here are the reasons.

​	**When the core number is large**, there will be **more processes execute the computation in parallel**. Since the dynamic allocation effectively eliminate the imbalance of workload allocation, the computation bottleneck of the static version can be eliminated. Therefore, the parallelism becomes more available on multi-cores, making the speedup get higher with more cores. 

​	**When the data size is large,** the **extra overhead** for root to **communicate with other processes** and for all processes to allocate jobs **become relative small**, compared with the computation consumption. 

​	When the **iteration limit** is small, the imbalance among different jobs is small. Since the allocation **granularity is fixed** (each job has a fixed size), the larger the iteration limit, the larger the imbalance. Although we adopt the dynamic allocation, there is still **possibility** that some of the processes get a heavy job that drag down the overall performance. To **solve this problem**, we can modify our granularity according to the iteration limit. **For larger iteration limit, we set the granularity smaller.**

​	There are also some **exceptions**, which can also be explained.

​	When the number of cores $n$ are $2$, the **speedup decreases as the data size gets larger**. For $n=2$, it is very intuitive. When there are two processes, there can be $1$ master and $1$ slave, which is similar to the sequential version. Since the existence of master and dynamic allocation, we add extra overhead when **initializing the job queue and communicate with the slave processes**. If the data size is getting larger, the job queue will also get larger, which makes the initialization more expensive and the communication more frequent. Therefore, when $n=2$, the speedup is less than $1$​ and gets smaller with an increasing data size. 

​	When the data size is $400$​, the program can achieve better speedup than larger data size on some configurations such as $n=4,8,16$ and iteration limit $k=500, 1000$​​. A possible reason is that the large data size and large iteration limit introduce more imbalance among all the jobs, which cannot be overcome by small number of cores. Thus, when the core number is small and the iteration limit is small, the small data size happens to relieve the imbalance, making the overall performance better.



#### Experiment 3: Pthread-Static

Here are some of the results of execution time in this version.

**NOTICE !**: Pthread program **cannot run on distributed computers**. When the number of threads exceed $32$, there will be thread switch. The tests performed on over $32$​​ threads are just for a unite view of MPI and Pthread.

##### *The **Speed** with Different Data Size and Number of Cores (Pixels / Second)*

****

**Iteration Threshold = 100**

| Datasize\Proc | 1        | 2        | 4        | 8        | 16       | 32       | 64       | 96       | 128      |
| ------------- | -------- | -------- | -------- | -------- | -------- | -------- | -------- | -------- | -------- |
| 400           | 1.06E+08 | 1.93E+08 | 2.92E+08 | 3.54E+08 | 9.56E+07 | 1.45E+08 | 1.18E+08 | 1.15E+08 | 6.45E+07 |
| 800           | 2.10E+07 | 2.30E+07 | 2.64E+07 | 3.91E+07 | 5.72E+07 | 9.82E+07 | 1.56E+08 | 1.66E+08 | 1.71E+08 |
| 1600          | 2.09E+07 | 2.33E+07 | 2.46E+07 | 3.58E+07 | 5.87E+07 | 1.04E+08 | 1.90E+08 | 2.49E+08 | 2.92E+08 |
| 3200          | 1.96E+07 | 2.52E+07 | 2.67E+07 | 3.35E+07 | 5.88E+07 | 1.05E+08 | 2.04E+08 | 2.87E+08 | 3.61E+08 |
| 6400          | 1.82E+07 | 2.57E+07 | 2.83E+07 | 3.60E+07 | 5.77E+07 | 1.15E+08 | 2.02E+08 | 2.77E+08 | 3.57E+08 |

**Iteration Threshold = 500**

| Datasize\Proc | 1        | 2        | 4        | 8        | 16       | 32       | 64       | 96       | 128      |
| ------------- | -------- | -------- | -------- | -------- | -------- | -------- | -------- | -------- | -------- |
| 400           | 1.07E+08 | 1.92E+08 | 2.94E+08 | 3.38E+08 | 2.40E+08 | 1.36E+08 | 1.14E+08 | 6.61E+07 | 1.15E+08 |
| 800           | 5.47E+06 | 5.61E+06 | 6.15E+06 | 8.80E+06 | 1.27E+07 | 2.23E+07 | 3.96E+07 | 4.85E+07 | 4.84E+07 |
| 1600          | 5.47E+06 | 5.62E+06 | 5.69E+06 | 7.92E+06 | 1.27E+07 | 2.26E+07 | 4.41E+07 | 5.92E+07 | 7.58E+07 |
| 3200          | 5.38E+06 | 6.05E+06 | 6.15E+06 | 7.40E+06 | 1.28E+07 | 2.27E+07 | 4.42E+07 | 6.38E+07 | 8.16E+07 |
| 6400          | 5.26E+06 | 6.59E+06 | 6.76E+06 | 8.04E+06 | 1.25E+07 | 2.50E+07 | 4.49E+07 | 6.17E+07 | 8.13E+07 |

**Iteration Threshold = 1000**

| Datasize\Proc | 1        | 2        | 4        | 8        | 16       | 32       | 64       | 96       | 128      |
| ------------- | -------- | -------- | -------- | -------- | -------- | -------- | -------- | -------- | -------- |
| 400           | 1.05E+08 | 1.92E+08 | 3.05E+08 | 3.43E+08 | 2.45E+08 | 1.38E+08 | 1.16E+08 | 1.13E+08 | 1.16E+08 |
| 800           | 2.85E+06 | 2.89E+06 | 3.15E+06 | 4.49E+06 | 6.37E+06 | 1.14E+07 | 2.06E+07 | 2.56E+07 | 2.56E+07 |
| 1600          | 2.85E+06 | 2.89E+06 | 2.91E+06 | 4.03E+06 | 6.45E+06 | 1.15E+07 | 2.25E+07 | 3.04E+07 | 3.94E+07 |
| 3200          | 2.82E+06 | 3.12E+06 | 3.15E+06 | 3.76E+06 | 6.49E+06 | 1.15E+07 | 2.27E+07 | 3.24E+07 | 4.14E+07 |
| 6400          | 2.79E+06 | 3.43E+06 | 3.48E+06 | 4.09E+06 | 6.34E+06 | 1.26E+07 | 2.28E+07 | 3.13E+07 | 4.14E+07 |



##### *The **Speedup** with Different Data Size*

****

**Iteration Threshold = 100**

| Datasize\Proc | 1    | 2    | 4    | 8    | 16   | 32   | 64    | 96    | 128   |
| ------------- | ---- | ---- | ---- | ---- | ---- | ---- | ----- | ----- | ----- |
| 400           | 1.00 | 1.82 | 2.75 | 3.33 | 0.90 | 1.37 | 1.11  | 1.08  | 0.61  |
| 800           | 1.00 | 1.10 | 1.26 | 1.86 | 2.73 | 4.68 | 7.44  | 7.94  | 8.13  |
| 1600          | 1.00 | 1.12 | 1.17 | 1.71 | 2.81 | 4.95 | 9.09  | 11.91 | 13.97 |
| 3200          | 1.00 | 1.28 | 1.36 | 1.71 | 3.00 | 5.33 | 10.38 | 14.62 | 18.39 |
| 6400          | 1.00 | 1.41 | 1.55 | 1.98 | 3.17 | 6.30 | 11.10 | 15.23 | 19.64 |

![image-20211101210744493](CSC4005 Assignment2-Report.assets/image-20211101210744493.png)

**Iteration Threshold = 500**

| Datasize\Proc | 1    | 2    | 4    | 8    | 16   | 32   | 64   | 96    | 128   |
| ------------- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ----- | ----- |
| 400           | 1.00 | 1.80 | 2.75 | 3.17 | 2.25 | 1.28 | 1.07 | 0.62  | 1.07  |
| 800           | 1.00 | 1.03 | 1.13 | 1.61 | 2.32 | 4.09 | 7.25 | 8.86  | 8.85  |
| 1600          | 1.00 | 1.03 | 1.04 | 1.45 | 2.33 | 4.14 | 8.07 | 10.83 | 13.87 |
| 3200          | 1.00 | 1.13 | 1.14 | 1.37 | 2.38 | 4.21 | 8.22 | 11.85 | 15.16 |
| 6400          | 1.00 | 1.25 | 1.28 | 1.53 | 2.38 | 4.75 | 8.53 | 11.73 | 15.44 |

![image-20211101210701305](CSC4005 Assignment2-Report.assets/image-20211101210701305.png)

**Iteration Threshold = 1000**

| Datasize\Proc | 1    | 2    | 4    | 8    | 16   | 32   | 64   | 96    | 128   |
| ------------- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ----- | ----- |
| 400           | 1.00 | 1.83 | 2.90 | 3.27 | 2.33 | 1.32 | 1.10 | 1.07  | 1.10  |
| 800           | 1.00 | 1.01 | 1.10 | 1.57 | 2.23 | 4.00 | 7.22 | 8.97  | 8.96  |
| 1600          | 1.00 | 1.01 | 1.02 | 1.41 | 2.26 | 4.03 | 7.89 | 10.66 | 13.82 |
| 3200          | 1.00 | 1.11 | 1.11 | 1.33 | 2.30 | 4.07 | 8.05 | 11.45 | 14.66 |
| 6400          | 1.00 | 1.23 | 1.24 | 1.47 | 2.27 | 4.53 | 8.16 | 11.19 | 14.82 |

![image-20211101210555418](CSC4005 Assignment2-Report.assets/image-20211101210555418.png)



##### Analysis

****

​	The speedup of this implementation is more significant than the MPI-Static implementation. According to the speedup charts, we can conclude that **with more cores, the speedup increases and with larger data size, the speedup is more available.** It is easy to be aware that when the data size is large, the extra overhead of creating threads and distributing work become relatively small to the computation cost. For example, when **the data size is small (400)**, the more cores we used, the less speedup we achieved. Since the data size is small, each thread only computes small size of data, costing time $t1$​​​. And the main thread should spend time $t2$​​ to create $n$​​​ peer threads. And the sequential consumes $t$​ to do all the computation. However, although $t1<t$​, $t1+t2>t$​. But with a larger data size, such $6400$​, $t$​ is much larger than $t1$​, making $t1+t2<t$ and a better speedup.



#### Experiment 4: Pthread-Dynamic

**NOTICE !**: Pthread program **cannot run on distributed computers**. When the number of threads exceed $32$, there will be thread switch. The tests performed on over $32$ threads are just for a unite view of MPI and Pthread.

##### *The **Speed** with Different Data Size and Number of Cores (Pixels / Second)*

****

**Iteration Threshold = 100**

| Datasize\Proc | 1        | 2        | 4        | 8        | 16       | 32       | 64       | 96       | 128      |
| ------------- | -------- | -------- | -------- | -------- | -------- | -------- | -------- | -------- | -------- |
| 400           | 1.08E+08 | 1.95E+08 | 2.97E+08 | 3.59E+08 | 2.46E+08 | 1.99E+08 | 2.11E+08 | 2.08E+08 | 2.10E+08 |
| 800           | 2.13E+07 | 4.23E+07 | 8.33E+07 | 1.20E+08 | 1.19E+08 | 1.17E+08 | 1.13E+08 | 1.13E+08 | 1.00E+08 |
| 1600          | 2.15E+07 | 4.29E+07 | 8.54E+07 | 1.66E+08 | 2.35E+08 | 2.36E+08 | 2.24E+08 | 2.22E+08 | 2.21E+08 |
| 3200          | 2.16E+07 | 4.31E+07 | 8.58E+07 | 1.71E+08 | 3.11E+08 | 4.53E+08 | 4.57E+08 | 4.35E+08 | 4.15E+08 |
| 6400          | 2.13E+07 | 4.31E+07 | 8.62E+07 | 1.72E+08 | 3.42E+08 | 6.21E+08 | 5.85E+08 | 5.44E+08 | 5.37E+08 |

**Iteration Threshold = 500**

| Datasize\Proc | 1        | 2        | 4        | 8        | 16       | 32       | 64       | 96       | 128      |
| ------------- | -------- | -------- | -------- | -------- | -------- | -------- | -------- | -------- | -------- |
| 400           | 1.08E+08 | 1.96E+08 | 3.07E+08 | 3.69E+08 | 2.68E+08 | 2.11E+08 | 2.11E+08 | 2.06E+08 | 2.03E+08 |
| 800           | 5.49E+06 | 1.05E+07 | 2.07E+07 | 2.72E+07 | 2.72E+07 | 2.70E+07 | 2.68E+07 | 2.68E+07 | 2.68E+07 |
| 1600          | 5.51E+06 | 1.10E+07 | 1.99E+07 | 3.92E+07 | 5.21E+07 | 5.22E+07 | 5.15E+07 | 5.14E+07 | 5.14E+07 |
| 3200          | 5.49E+06 | 1.10E+07 | 2.18E+07 | 4.07E+07 | 7.34E+07 | 1.01E+08 | 9.87E+07 | 9.58E+07 | 9.87E+07 |
| 6400          | 5.49E+06 | 1.10E+07 | 2.20E+07 | 4.29E+07 | 8.09E+07 | 1.47E+08 | 1.32E+08 | 1.33E+08 | 1.38E+08 |

**Iteration Threshold = 1000**

| Datasize\Proc | 1        | 2        | 4        | 8        | 16       | 32       | 64       | 96       | 128      |
| ------------- | -------- | -------- | -------- | -------- | -------- | -------- | -------- | -------- | -------- |
| 400           | 1.07E+08 | 1.95E+08 | 2.98E+08 | 3.69E+08 | 2.53E+08 | 2.16E+08 | 2.07E+08 | 9.02E+07 | 2.12E+08 |
| 800           | 2.86E+06 | 5.37E+06 | 1.07E+07 | 1.39E+07 | 1.38E+07 | 1.38E+07 | 1.38E+07 | 1.38E+07 | 1.38E+07 |
| 1600          | 2.86E+06 | 5.72E+06 | 1.02E+07 | 2.01E+07 | 2.65E+07 | 2.65E+07 | 2.63E+07 | 2.63E+07 | 2.63E+07 |
| 3200          | 2.86E+06 | 5.71E+06 | 1.12E+07 | 2.09E+07 | 3.77E+07 | 5.11E+07 | 5.10E+07 | 5.07E+07 | 5.04E+07 |
| 6400          | 2.86E+06 | 5.71E+06 | 1.14E+07 | 2.19E+07 | 4.15E+07 | 7.55E+07 | 6.97E+07 | 6.53E+07 | 7.10E+07 |



##### *The **Speedup** with Different Data Size*

****

**Iteration Threshold = 100**

| Datasize\Proc | 1    | 2        | 4        | 8        | 16       | 32       | 64       | 96       | 128      |
| ------------- | ---- | -------- | -------- | -------- | -------- | -------- | -------- | -------- | -------- |
| 400           | 1    | 1.81197  | 2.752947 | 3.330263 | 2.279718 | 1.850051 | 1.957829 | 1.927761 | 1.94619  |
| 800           | 1    | 1.985929 | 3.914374 | 5.640561 | 5.599887 | 5.473123 | 5.306249 | 5.306108 | 4.697414 |
| 1600          | 1    | 1.996429 | 3.969176 | 7.72093  | 10.92362 | 10.95305 | 10.42951 | 10.31452 | 10.2743  |
| 3200          | 1    | 1.997751 | 3.979663 | 7.932403 | 14.42236 | 20.99411 | 21.2113  | 20.18756 | 19.22197 |
| 6400          | 1    | 2.020734 | 4.040001 | 8.051141 | 16.03283 | 29.08474 | 27.43152 | 25.47867 | 25.17587 |

![image-20211101213630734](CSC4005 Assignment2-Report.assets/image-20211101213630734.png)

**Iteration Threshold = 500**

| Datasize\Proc | 1    | 2    | 4    | 8    | 16    | 32    | 64    | 96    | 128   |
| ------------- | ---- | ---- | ---- | ---- | ----- | ----- | ----- | ----- | ----- |
| 400           | 1.00 | 1.82 | 2.86 | 3.43 | 2.49  | 1.96  | 1.96  | 1.91  | 1.89  |
| 800           | 1.00 | 1.91 | 3.77 | 4.95 | 4.94  | 4.91  | 4.89  | 4.88  | 4.88  |
| 1600          | 1.00 | 2.00 | 3.62 | 7.11 | 9.46  | 9.49  | 9.36  | 9.34  | 9.33  |
| 3200          | 1.00 | 2.00 | 3.98 | 7.41 | 13.38 | 18.34 | 17.98 | 17.45 | 17.98 |
| 6400          | 1.00 | 2.00 | 4.01 | 7.81 | 14.74 | 26.80 | 24.01 | 24.21 | 25.11 |

![image-20211101213701672](CSC4005 Assignment2-Report.assets/image-20211101213701672.png)

**Iteration Threshold = 1000**

| Datasize\Proc | 1    | 2    | 4    | 8    | 16    | 32    | 64    | 96    | 128   |
| ------------- | ---- | ---- | ---- | ---- | ----- | ----- | ----- | ----- | ----- |
| 400           | 1.00 | 1.81 | 2.77 | 3.43 | 2.35  | 2.01  | 1.93  | 0.84  | 1.98  |
| 800           | 1.00 | 1.88 | 3.74 | 4.85 | 4.84  | 4.83  | 4.81  | 4.81  | 4.81  |
| 1600          | 1.00 | 2.00 | 3.57 | 7.03 | 9.25  | 9.26  | 9.20  | 9.19  | 9.18  |
| 3200          | 1.00 | 2.00 | 3.92 | 7.31 | 13.19 | 17.88 | 17.86 | 17.75 | 17.63 |
| 6400          | 1.00 | 2.00 | 4.00 | 7.68 | 14.52 | 26.43 | 24.39 | 22.87 | 24.86 |

![image-20211101213735716](CSC4005 Assignment2-Report.assets/image-20211101213735716.png)

##### Analysis

****

​	The speedup of this implementation is more significant than the MPI-Static implementation. According to the speedup charts, we can conclude that **with more cores, the speedup increases and with larger data size, the speedup is more available.** It is easy to be aware that when the data size is large, the extra overhead of creating threads and distributing work become relatively small to the computation cost. For example, when **the data size is small (400)**, the more cores we used, the less speedup we achieved. Since the data size is small, each thread only computes small size of data, costing time $t1$. And the main thread should spend time $t2$ to create $n$ peer threads. And the sequential consumes $t$ to do all the computation. However, although $t1<t$, $t1+t2>t$. But with a larger data size, such $6400$, $t$ is much larger than $t1$, making $t1+t2<t$​ and a better speedup.

​	And we can notice that for each of the data size, the speedup stops increasing or even decreasing if we reach some configuration and continue to add more cores for it. A possible reason is that the time cost decreased by parallel computation is less than the time consumed to create more peer threads.



#### Comparison Between MPI-Static & MPI-Dynamic

​	To compare the performance between two MPI implementations, we derive the following two charts:

##### Difference on Speed (Pixels / Second)

****

*Speed of Static - Speed of Dynamic*

k = 100

| Datasize\Proc | 2         | 4         | 8         | 16        | 32        | 64        | 96        | 128       |
| ------------- | --------- | --------- | --------- | --------- | --------- | --------- | --------- | --------- |
| 400           | 5.34E+07  | -5.50E+07 | -7.78E+07 | -1.54E+07 | 1.57E+07  | 9.28E+07  | 9.35E+07  | 9.61E+07  |
| 800           | 9.84E+05  | -3.80E+07 | -8.84E+07 | -9.87E+07 | -9.79E+07 | 2.93E+06  | -3.06E+06 | 4.88E+06  |
| 1600          | 3.20E+05  | -4.00E+07 | -1.13E+08 | -1.88E+08 | -1.91E+08 | -1.57E+07 | -5.12E+07 | -3.33E+07 |
| 3200          | -8.80E+05 | -4.20E+07 | -1.22E+08 | -2.52E+08 | -3.61E+08 | -8.94E+07 | -1.98E+08 | -9.57E+07 |
| 6400          | -2.38E+06 | -4.39E+07 | -1.26E+08 | -2.72E+08 | -4.46E+08 | -2.98E+08 | -4.37E+08 | -2.81E+08 |

k = 500

| Datasize\Proc | 2         | 4         | 8         | 16        | 32        | 64        | 96        | 128       |
| ------------- | --------- | --------- | --------- | --------- | --------- | --------- | --------- | --------- |
| 400           | 4.90E+07  | -5.26E+07 | -7.57E+07 | -2.08E+07 | 1.54E+07  | 9.27E+07  | 9.40E+07  | 9.53E+07  |
| 800           | 7.19E+04  | -9.93E+06 | -2.12E+07 | -2.16E+07 | -2.15E+07 | -1.35E+07 | -1.87E+07 | -1.04E+07 |
| 1600          | 2.50E+04  | -1.08E+07 | -2.77E+07 | -4.52E+07 | -4.54E+07 | -3.27E+07 | -4.40E+07 | -4.43E+07 |
| 3200          | -5.21E+04 | -1.09E+07 | -3.14E+07 | -6.26E+07 | -9.15E+07 | -5.35E+07 | -7.96E+07 | -7.91E+07 |
| 6400          | -1.75E+05 | -1.11E+07 | -3.28E+07 | -7.06E+07 | -1.32E+08 | -1.31E+08 | -1.61E+08 | -1.43E+08 |

k = 1000

| Datasize\Proc | 2         | 4         | 8         | 16        | 32        | 64        | 96        | 128       |
| ------------- | --------- | --------- | --------- | --------- | --------- | --------- | --------- | --------- |
| 400           | 5.35E+07  | -5.22E+07 | -5.10E+07 | -1.98E+07 | 1.38E+07  | 9.27E+07  | 9.43E+07  | 9.53E+07  |
| 800           | 2.18E+04  | -5.05E+06 | -1.09E+07 | -1.10E+07 | -1.10E+07 | -1.09E+07 | -1.09E+07 | -1.09E+07 |
| 1600          | 9.44E+03  | -5.49E+06 | -1.43E+07 | -2.33E+07 | -2.33E+07 | -2.06E+07 | -2.31E+07 | -2.31E+07 |
| 3200          | -1.52E+04 | -5.69E+06 | -1.61E+07 | -3.23E+07 | -4.72E+07 | -3.64E+07 | -4.39E+07 | -4.37E+07 |
| 6400          | -5.03E+04 | -5.74E+06 | -1.71E+07 | -3.63E+07 | -6.84E+07 | -8.10E+07 | -8.84E+07 | -8.30E+07 |



##### Difference on Speedup

****

*Speedup of Dynamic - Speedup of Static*

k = 100

| Datasize\Proc | 1    | 2    | 4    | 8    | 16    | 32    | 64    | 96    | 128   |
| ------------- | ---- | ---- | ---- | ---- | ----- | ----- | ----- | ----- | ----- |
| 400           | 1.00 | 0.48 | 1.52 | 1.75 | 1.15  | 0.84  | 0.08  | 0.07  | 0.05  |
| 800           | 1.00 | 0.96 | 2.87 | 5.34 | 5.85  | 5.81  | 0.86  | 1.15  | 0.79  |
| 1600          | 1.00 | 0.98 | 2.91 | 6.40 | 10.03 | 10.13 | 1.75  | 3.45  | 2.59  |
| 3200          | 1.00 | 1.04 | 3.10 | 7.11 | 13.62 | 19.10 | 5.47  | 10.91 | 5.80  |
| 6400          | 1.00 | 1.13 | 3.38 | 7.84 | 15.74 | 25.20 | 17.14 | 24.69 | 16.21 |

k = 500

| Datasize\Proc | 1    | 2    | 4    | 8    | 16   | 32   | 64   | 96   | 128  |
| ------------- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| 400           | 1.00 | 0.51 | 1.50 | 1.73 | 1.19 | 0.84 | 0.08 | 0.06 | 0.04 |
| 800           | 1.00 | 0.26 | 0.75 | 1.31 | 1.33 | 1.32 | 0.93 | 1.19 | 0.78 |
| 1600          | 1.00 | 0.26 | 0.78 | 1.59 | 2.42 | 2.44 | 1.83 | 2.37 | 2.38 |
| 3200          | 1.00 | 0.27 | 0.82 | 1.84 | 3.40 | 4.85 | 2.95 | 4.26 | 4.23 |
| 6400          | 1.00 | 0.30 | 0.89 | 2.06 | 4.11 | 7.43 | 7.38 | 9.03 | 8.06 |

k = 1000

| Datasize\Proc | 1    | 2    | 4    | 8    | 16   | 32   | 64   | 96   | 128  |
| ------------- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| 400           | 1.00 | 0.48 | 1.50 | 1.49 | 1.20 | 0.84 | 0.08 | 0.07 | 0.05 |
| 800           | 1.00 | 0.14 | 0.39 | 0.67 | 0.68 | 0.68 | 0.68 | 0.68 | 0.68 |
| 1600          | 1.00 | 0.14 | 0.40 | 0.82 | 1.25 | 1.25 | 1.12 | 1.24 | 1.24 |
| 3200          | 1.00 | 0.14 | 0.43 | 0.95 | 1.76 | 2.50 | 1.96 | 2.34 | 2.33 |
| 6400          | 1.00 | 0.15 | 0.46 | 1.08 | 2.12 | 3.86 | 4.55 | 4.94 | 4.65 |



##### Analysis

****

​	In general, the **speed of dynamic allocation in MPI is better** than static allocation. Although the dynamic allocation uses less processes to do the computation, it helps **reduce the imbalance** among local work and thus improve the parallelism, which makes the performance better.  However, the static program cannot overcome its bottleneck. The process who takes the heaviest job drags down the speed, and makes no speedup on multi-cores.



#### Comparison Between Pthread-Static & Pthread-Dynamic

##### Difference on Speed (Pixels / Second)

****

*Speed of Static - Speed of Dynamic*

k = 100

| Datasize\Proc | 1         | 2         | 4         | 8         | 16        | 32        | 64        | 96        | 128       |
| ------------- | --------- | --------- | --------- | --------- | --------- | --------- | --------- | --------- | --------- |
| 400           | -1.57E+06 | -1.99E+06 | -4.51E+06 | -5.44E+06 | -1.50E+08 | -5.41E+07 | -9.28E+07 | -9.34E+07 | -1.45E+08 |
| 800           | -3.23E+05 | -1.93E+07 | -5.69E+07 | -8.10E+07 | -6.21E+07 | -1.84E+07 | 4.30E+07  | 5.34E+07  | 7.05E+07  |
| 1600          | -5.75E+05 | -1.96E+07 | -6.08E+07 | -1.30E+08 | -1.76E+08 | -1.32E+08 | -3.39E+07 | 2.74E+07  | 7.15E+07  |
| 3200          | -1.93E+06 | -1.79E+07 | -5.91E+07 | -1.38E+08 | -2.52E+08 | -3.48E+08 | -2.54E+08 | -1.48E+08 | -5.35E+07 |
| 6400          | -3.14E+06 | -1.75E+07 | -5.80E+07 | -1.36E+08 | -2.84E+08 | -5.06E+08 | -3.83E+08 | -2.66E+08 | -1.80E+08 |

k = 500

| Datasize\Proc | 1         | 2         | 4         | 8         | 16        | 32        | 64        | 96        | 128       |
| ------------- | --------- | --------- | --------- | --------- | --------- | --------- | --------- | --------- | --------- |
| 400           | -7.90E+05 | -3.88E+06 | -1.37E+07 | -3.05E+07 | -2.77E+07 | -7.46E+07 | -9.64E+07 | -1.40E+08 | -8.86E+07 |
| 800           | -2.87E+04 | -4.86E+06 | -1.46E+07 | -1.84E+07 | -1.45E+07 | -4.65E+06 | 1.28E+07  | 2.16E+07  | 2.16E+07  |
| 1600          | -3.95E+04 | -5.38E+06 | -1.42E+07 | -3.13E+07 | -3.94E+07 | -2.96E+07 | -7.43E+06 | 7.80E+06  | 2.45E+07  |
| 3200          | -1.08E+05 | -4.95E+06 | -1.57E+07 | -3.33E+07 | -6.06E+07 | -7.80E+07 | -5.44E+07 | -3.20E+07 | -1.71E+07 |
| 6400          | -2.25E+05 | -4.39E+06 | -1.52E+07 | -3.48E+07 | -6.84E+07 | -1.22E+08 | -8.68E+07 | -7.11E+07 | -5.65E+07 |

k = 1000

| Datasize\Proc | 1         | 2         | 4         | 8         | 16        | 32        | 64        | 96        | 128       |
| ------------- | --------- | --------- | --------- | --------- | --------- | --------- | --------- | --------- | --------- |
| 400           | -2.39E+06 | -2.63E+06 | 7.33E+06  | -2.54E+07 | -8.26E+06 | -7.77E+07 | -9.14E+07 | 2.24E+07  | -9.62E+07 |
| 800           | -8.08E+03 | -2.48E+06 | -7.54E+06 | -9.37E+06 | -7.46E+06 | -2.40E+06 | 6.84E+06  | 1.18E+07  | 1.18E+07  |
| 1600          | -1.03E+04 | -2.83E+06 | -7.30E+06 | -1.61E+07 | -2.00E+07 | -1.50E+07 | -3.83E+06 | 4.10E+06  | 1.31E+07  |
| 3200          | -3.20E+04 | -2.59E+06 | -8.04E+06 | -1.71E+07 | -3.12E+07 | -3.96E+07 | -2.83E+07 | -1.84E+07 | -8.96E+06 |
| 6400          | -6.29E+04 | -2.28E+06 | -7.94E+06 | -1.79E+07 | -3.51E+07 | -6.28E+07 | -4.69E+07 | -3.41E+07 | -2.96E+07 |



##### Difference on Speedup

****

*Speedup of Dynamic - Speedup of Static*

k = 100

| Datasize\Proc | 2        | 4        | 8        | 16       | 32       | 64       | 96       | 128      |
| ------------- | -------- | -------- | -------- | -------- | -------- | -------- | -------- | -------- |
| 400           | -0.00813 | 0.001636 | 0.001893 | 1.379555 | 0.481482 | 0.844699 | 0.850104 | 1.339582 |
| 800           | 0.890837 | 2.654706 | 3.777711 | 2.873748 | 0.791613 | -2.13174 | -2.62897 | -3.43391 |
| 1600          | 0.881032 | 2.794193 | 6.012112 | 8.118239 | 6.004934 | 1.335291 | -1.59421 | -3.69853 |
| 3200          | 0.715978 | 2.618476 | 6.223695 | 11.42616 | 15.66862 | 10.8346  | 5.570498 | 0.831282 |
| 6400          | 0.610711 | 2.487659 | 6.07222  | 12.86011 | 22.78779 | 16.33318 | 10.24626 | 5.53782  |

![image-20211101221448740](CSC4005 Assignment2-Report.assets/image-20211101221448740.png)

k = 500

| Datasize\Proc | 2        | 4        | 8        | 16       | 32       | 64       | 96       | 128      |
| ------------- | -------- | -------- | -------- | -------- | -------- | -------- | -------- | -------- |
| 400           | 0.022855 | 0.107318 | 0.259984 | 0.24099  | 0.683742 | 0.887715 | 1.295338 | 0.815603 |
| 800           | 0.878833 | 2.644126 | 3.337113 | 2.626799 | 0.824406 | -2.367   | -3.98063 | -3.97296 |
| 1600          | 0.969429 | 2.575802 | 5.665427 | 7.139629 | 5.346641 | 1.291538 | -1.49492 | -4.54114 |
| 3200          | 0.878797 | 2.836116 | 6.038292 | 10.99356 | 14.12443 | 9.758591 | 5.596602 | 2.820627 |
| 6400          | 0.748723 | 2.723404 | 6.281077 | 12.36533 | 22.05532 | 15.47105 | 12.47783 | 9.667575 |

![image-20211101221433674](CSC4005 Assignment2-Report.assets/image-20211101221433674.png)

k = 1000

| Datasize\Proc | 2        | 4        | 8        | 16       | 32       | 64       | 96       | 128      |
| ------------- | -------- | -------- | -------- | -------- | -------- | -------- | -------- | -------- |
| 400           | -0.01627 | -0.13272 | 0.164122 | 0.025042 | 0.693841 | 0.826296 | -0.23228 | 0.870769 |
| 800           | 0.86411  | 2.631906 | 3.273501 | 2.603526 | 0.827317 | -2.41054 | -4.15477 | -4.14835 |
| 1600          | 0.984283 | 2.548215 | 5.619406 | 6.989531 | 5.233588 | 1.31052  | -1.47149 | -4.63308 |
| 3200          | 0.892524 | 2.802723 | 5.981567 | 10.88809 | 13.81802 | 9.807086 | 6.297382 | 2.971137 |
| 6400          | 0.771546 | 2.75257  | 6.218633 | 12.24953 | 21.89855 | 16.2321  | 11.67784 | 10.04272 |

![image-20211101221524941](CSC4005 Assignment2-Report.assets/image-20211101221524941.png)



##### Analysis

****

​	In general, the **speed of dynamic allocation in Pthread is better** than static allocation. Although the dynamic allocation uses less cores to do the computation, it helps **reduce the imbalance** among local work and thus improve the parallelism, which makes the performance better.  However, the static program cannot overcome its bottleneck. The process who takes the heaviest job drags down the speed, and makes no speedup on multi-cores.

​	However, **when the data size is small, static allocation may have better speedup than the static allocation**.  The **possible reason** is that when the data size is small, the job unit in dynamic allocation is small. And in the Pthread dynamic allocation implementation, the **take** operations must be performed sequentially due to the **mutex**. Therefore, the peer threads may frequently queue to perform **take** operations. However, as the data size is small, the imbalance in static allocation gets small. All its peer threads can perform their local job independently without queueing to get new job. Therefore, it makes the speedup difference **exceptions**. 



#### Comparison Between MPI and Pthread

​	We have found that the dynamic version of both MPI and Pthread is better. Here we compare the execution speed of these two dynamic implementation.

k = 100

| Datasize\Proc | 2        | 4        | 8        | 16       | 32        | 64       | 96       | 128      |
| ------------- | -------- | -------- | -------- | -------- | --------- | -------- | -------- | -------- |
| 400           | 1.47E+08 | 1.42E+08 | 1.81E+08 | 1.29E+08 | 1.14E+08  | 2.03E+08 | 2.01E+08 | 2.05E+08 |
| 800           | 2.28E+07 | 2.50E+07 | 1.14E+07 | 6.80E+04 | -1.73E+06 | 9.55E+07 | 8.95E+07 | 8.40E+07 |
| 1600          | 2.25E+07 | 2.46E+07 | 3.23E+07 | 2.55E+07 | 2.41E+07  | 1.88E+08 | 1.50E+08 | 1.67E+08 |
| 3200          | 2.23E+07 | 2.39E+07 | 2.91E+07 | 3.90E+07 | 7.14E+07  | 3.48E+08 | 2.17E+08 | 2.99E+08 |
| 6400          | 2.23E+07 | 2.39E+07 | 2.71E+07 | 5.17E+07 | 1.56E+08  | 2.69E+08 | 8.83E+07 | 2.38E+08 |

k = 500

| Datasize\Proc | 2        | 4        | 8        | 16       | 32       | 64        | 96        | 128       |
| ------------- | -------- | -------- | -------- | -------- | -------- | --------- | --------- | --------- |
| 400           | 1.44E+08 | 1.55E+08 | 1.93E+08 | 1.47E+08 | 1.26E+08 | 2.03E+08  | 2.00E+08  | 1.99E+08  |
| 800           | 5.10E+06 | 5.35E+06 | 5.29E+05 | 1.33E+05 | 7.87E+04 | 7.93E+06  | 2.62E+06  | 1.09E+07  |
| 1600          | 5.56E+06 | 3.70E+06 | 6.02E+06 | 1.50E+06 | 1.33E+06 | 1.34E+07  | 1.94E+06  | 1.60E+06  |
| 3200          | 5.55E+06 | 5.53E+06 | 3.89E+06 | 5.46E+06 | 3.80E+06 | 3.98E+07  | 1.07E+07  | 1.42E+07  |
| 6400          | 5.54E+06 | 5.66E+06 | 4.81E+06 | 5.09E+06 | 1.01E+07 | -4.31E+06 | -3.37E+07 | -1.08E+07 |

k = 1000

| Datasize\Proc | 2        | 4        | 8        | 16       | 32       | 64        | 96        | 128       |
| ------------- | -------- | -------- | -------- | -------- | -------- | --------- | --------- | --------- |
| 400           | 1.46E+08 | 1.45E+08 | 2.17E+08 | 1.32E+08 | 1.31E+08 | 1.99E+08  | 8.33E+07  | 2.08E+08  |
| 800           | 2.55E+06 | 2.79E+06 | 1.64E+05 | 1.20E+04 | 1.77E+04 | -4.30E+03 | 3.30E+03  | -2.80E+04 |
| 1600          | 2.88E+06 | 1.88E+06 | 2.97E+06 | 3.64E+05 | 3.74E+05 | 2.93E+06  | 3.81E+05  | 3.64E+05  |
| 3200          | 2.87E+06 | 2.67E+06 | 1.99E+06 | 2.54E+06 | 1.10E+06 | 1.18E+07  | 4.01E+06  | 3.88E+06  |
| 6400          | 2.86E+06 | 2.89E+06 | 2.06E+06 | 2.33E+06 | 4.26E+06 | -1.42E+07 | -2.58E+07 | -1.48E+07 |

##### Analysis

****

​	(1) In general, the Pthread-Dynamic implementation is better on speed. The reason is that Pthread threads can share the same memory of a process, reducing the overhead to copy local results between different memory space, while the MPI program needs MPI_Send() and MPI_Recv() for memory copy.

​	(2) When the iteration limit $k$​ is large and the core number is large, the MPI-Dynamic implementation may become better than the Pthread one on speed. A reason is that,  when the core number is large, competition to get new allocations may be fierce. MPI program reduces the queueing time for a process to get new jobs by the use of master process. Another reason is that, **Pthread programs cannot be spread over distributed computers**. When the iteration limit is large, Pthread cannot be truly parallelized,  making **thread switching frequent.**





## 5. Conclusion

​	We have implemented $4$ versions of Mandelbrot Set Computation: MPI-Static, MPI-Dynamic, Pthread-Static, Pthread-Dynamic. 

​	According to the experiment results, the dynamic allocation implementation is better than the static allocation implementation. The reason is that dynamic allocation solve the imbalance of workload, while the static allocation suffers from this bottleneck (the heaviest workload decide the execution time).

​	And we found **in general, the Pthread program is better** than MPI program on execution speed, if with **full parallel hardware support**. The reason is that different threads can share their memory in the same process, eliminating the time for memory copying. 

​	However, when we are eager to expose **more parallelism by adding more processors** / more threads, **MPI program may become a better choice**. Since Pthread program cannot run on distributed computers, the hardware support for parallelism on one computer is limited. However, MPI can fully utilizes the distributed computers, which can easily scale up / out. Therefore, when we need large-scale parallelism and extremely big data size, we must choose MPI to implement it.





## 6. Appendix - Functionality

​	We test the four programs in different configurations (cores allocated) and different data size. The functionality of them is good. Here are some screenshots to demonstrate their good functionalities.

##### MPI-Static

****

1 core:

<img src="CSC4005 Assignment2-Report.assets/image-20211101132406446.png" alt="image-20211101132406446" style="zoom:33%;" />

<img src="CSC4005 Assignment2-Report.assets/image-20211101132514726.png" alt="image-20211101132514726" style="zoom:33%;" />

2-core:

<img src="CSC4005 Assignment2-Report.assets/image-20211101133226401.png" alt="image-20211101133226401" style="zoom:33%;" />

<img src="CSC4005 Assignment2-Report.assets/image-20211101133448891.png" alt="image-20211101133448891" style="zoom:33%;" />

32-core:

<img src="CSC4005 Assignment2-Report.assets/image-20211101134934215.png" alt="image-20211101134934215" style="zoom:33%;" />

<img src="CSC4005 Assignment2-Report.assets/image-20211101135035437.png" alt="image-20211101135035437" style="zoom:33%;" />



64-core:

<img src="CSC4005 Assignment2-Report.assets/image-20211101135922903.png" alt="image-20211101135922903" style="zoom:33%;" />

<img src="CSC4005 Assignment2-Report.assets/image-20211101140000658.png" alt="image-20211101140000658" style="zoom:33%;" />

96-core:

<img src="CSC4005 Assignment2-Report.assets/image-20211101140944852.png" alt="image-20211101140944852" style="zoom:33%;" />

<img src="CSC4005 Assignment2-Report.assets/image-20211101141024039.png" alt="image-20211101141024039" style="zoom:33%;" />

128-core:

<img src="CSC4005 Assignment2-Report.assets/image-20211101141408342.png" alt="image-20211101141408342" style="zoom:33%;" />

<img src="CSC4005 Assignment2-Report.assets/image-20211101141447293.png" alt="image-20211101141447293" style="zoom:33%;" />



##### MPI-Dynamic

****

2-core:

<img src="CSC4005 Assignment2-Report.assets/image-20211101133327886.png" alt="image-20211101133327886" style="zoom:33%;" />

<img src="CSC4005 Assignment2-Report.assets/image-20211101133513629.png" alt="image-20211101133513629" style="zoom:33%;" />

32-core:

<img src="CSC4005 Assignment2-Report.assets/image-20211101135116177.png" alt="image-20211101135116177" style="zoom: 33%;" />

<img src="CSC4005 Assignment2-Report.assets/image-20211101135147892.png" alt="image-20211101135147892" style="zoom:33%;" />

64-core:

<img src="CSC4005 Assignment2-Report.assets/image-20211101140035016.png" alt="image-20211101140035016" style="zoom:33%;" />

<img src="CSC4005 Assignment2-Report.assets/image-20211101140110871.png" alt="image-20211101140110871" style="zoom:33%;" />

96-core:

<img src="CSC4005 Assignment2-Report.assets/image-20211101141056897.png" alt="image-20211101141056897" style="zoom:33%;" />

<img src="CSC4005 Assignment2-Report.assets/image-20211101141139698.png" alt="image-20211101141139698" style="zoom:33%;" />

128-core:

<img src="CSC4005 Assignment2-Report.assets/image-20211101141517345.png" alt="image-20211101141517345" style="zoom:33%;" />

<img src="CSC4005 Assignment2-Report.assets/image-20211101141545780.png" alt="image-20211101141545780" style="zoom:33%;" />



##### Pthread-Static

****

1-thread:

<img src="CSC4005 Assignment2-Report.assets/image-20211101132949127.png" alt="image-20211101132949127" style="zoom:33%;" />

<img src="CSC4005 Assignment2-Report.assets/image-20211101133033246.png" alt="image-20211101133033246" style="zoom:33%;" />

2-thread:

<img src="CSC4005 Assignment2-Report.assets/image-20211101133545775.png" alt="image-20211101133545775" style="zoom:33%;" />

<img src="CSC4005 Assignment2-Report.assets/image-20211101133614801.png" alt="image-20211101133614801" style="zoom:33%;" />

32-thread:

<img src="CSC4005 Assignment2-Report.assets/image-20211101135227893.png" alt="image-20211101135227893" style="zoom:33%;" />

<img src="CSC4005 Assignment2-Report.assets/image-20211101135254915.png" alt="image-20211101135254915" style="zoom:33%;" />

64-thread:

<img src="CSC4005 Assignment2-Report.assets/image-20211101140145952.png" alt="image-20211101140145952" style="zoom:33%;" />

<img src="CSC4005 Assignment2-Report.assets/image-20211101140214769.png" alt="image-20211101140214769" style="zoom:33%;" />

96-thread:

<img src="CSC4005 Assignment2-Report.assets/image-20211101141202927.png" alt="image-20211101141202927" style="zoom:33%;" />

<img src="CSC4005 Assignment2-Report.assets/image-20211101141228382.png" alt="image-20211101141228382" style="zoom:33%;" />

128-thread:

<img src="CSC4005 Assignment2-Report.assets/image-20211101141606670.png" alt="image-20211101141606670" style="zoom:33%;" />

<img src="CSC4005 Assignment2-Report.assets/image-20211101141713559.png" alt="image-20211101141713559" style="zoom:33%;" />



##### Pthread-Dynamic

****

1-thread:

<img src="CSC4005 Assignment2-Report.assets/image-20211101133110651.png" alt="image-20211101133110651" style="zoom:33%;" />

<img src="CSC4005 Assignment2-Report.assets/image-20211101133137665.png" alt="image-20211101133137665" style="zoom:33%;" />

2-thread:

<img src="CSC4005 Assignment2-Report.assets/image-20211101133640268.png" alt="image-20211101133640268" style="zoom:33%;" />

<img src="CSC4005 Assignment2-Report.assets/image-20211101133704789.png" alt="image-20211101133704789" style="zoom:33%;" />

32-thread:

<img src="CSC4005 Assignment2-Report.assets/image-20211101135347604.png" alt="image-20211101135347604" style="zoom:33%;" />

<img src="CSC4005 Assignment2-Report.assets/image-20211101135428248.png" alt="image-20211101135428248" style="zoom:33%;" />

64-thread:

<img src="CSC4005 Assignment2-Report.assets/image-20211101140433363.png" alt="image-20211101140433363" style="zoom:33%;" />

<img src="CSC4005 Assignment2-Report.assets/image-20211101140504706.png" alt="image-20211101140504706" style="zoom:33%;" />

96-thread:

<img src="CSC4005 Assignment2-Report.assets/image-20211101141247813.png" alt="image-20211101141247813" style="zoom:33%;" />

<img src="CSC4005 Assignment2-Report.assets/image-20211101141322628.png" alt="image-20211101141322628" style="zoom:33%;" />

128-thread:

<img src="CSC4005 Assignment2-Report.assets/image-20211101141729226.png" alt="image-20211101141729226" style="zoom:33%;" />

<img src="CSC4005 Assignment2-Report.assets/image-20211101141756239.png" alt="image-20211101141756239" style="zoom:33%;" />

