#include <sirius.h>
#include <thread>
#include <wave_functions.h>

using namespace sirius;

//void test1(vector3d<int> const& dims__, double cutoff__, int num_bands__, std::vector<int> mpi_grid_dims__)
//{
//    Communicator comm(MPI_COMM_WORLD);
//    MPI_grid mpi_grid(mpi_grid_dims__, comm);
//
//    matrix3d<double> M;
//    M(0, 0) = M(1, 1) = M(2, 2) = 1.0;
//    
//    FFT3D fft(dims__, Platform::max_num_threads(), mpi_grid.communicator(1 << 1), CPU);
//    MEMORY_USAGE_INFO();
//
//    Gvec gvec(vector3d<double>(0, 0, 0), M, cutoff__, fft.fft_grid(), fft.comm(), mpi_grid.communicator(1 << 0).size(), false, false);
//    MEMORY_USAGE_INFO();
//    if (comm.rank() == 0)
//    {
//        printf("num_gvec_loc: %i\n", gvec.num_gvec(comm.rank()));
//        printf("local size of wf: %f GB\n", sizeof(double_complex) * num_bands__ * gvec.num_gvec(comm.rank()) / double(1 << 30));
//    }
//
//    Wave_functions psi_in(num_bands__, gvec, mpi_grid, true);
//    Wave_functions psi_out(num_bands__, gvec, mpi_grid, true);
//    
//    for (int i = 0; i < num_bands__; i++)
//    {
//        for (int j = 0; j < psi_in.num_gvec_loc(); j++)
//        {
//            psi_in(j, i) = type_wrapper<double_complex>::random();
//        }
//    }
//    if (comm.rank() == 0)
//    {
//        printf("local size of wf: %f GB\n", num_bands__ * gvec.num_gvec(comm.rank()) / double(1 << 30));
//    }
    //Timer t1("swap", comm);
    //psi_in.swap_forward(0, num_bands__);
    //MEMORY_USAGE_INFO();
    //for (int i = 0; i < psi_in.spl_num_swapped().local_size(); i++)
    //{
    //    std::memcpy(psi_out[i], psi_in[i], gvec.num_gvec_fft() * sizeof(double_complex));
    //}
    //psi_out.swap_backward(0, num_bands__);
    //t1.stop();
//    Timer t1("swap", comm);
//    psi_in.swap_forward(0, num_bands__);
//    for (int i = 0; i < psi_in.spl_num_swapped().local_size(); i++)
//    {
//        std::memcpy(psi_out[i], psi_in[i], gvec.num_gvec_fft() * sizeof(double_complex));
//    }
//    psi_out.swap_backward(0, num_bands__);
//    t1.stop();
//
//    double diff = 0;
//    for (int i = 0; i < num_bands__; i++)
//    {
//        for (int j = 0; j < psi_in.num_gvec_loc(); j++)
//        {
//            double d = std::abs(psi_in(j, i) - psi_out(j, i));
//            diff += d;
//        }
//    }
//    printf("diff: %18.12f\n", diff);
//
//    comm.barrier();
//}

void test2(std::vector<int> mpi_grid_dims__)
{
//    BLACS_grid blacs_grid(mpi_comm_world, mpi_grid_dims__[0], mpi_grid_dims__[1]);
//    BLACS_grid blacs_grid_slice(mpi_comm_world, 1, mpi_grid_dims__[0] * mpi_grid_dims__[1]);
//
//    int N = 10019;
//    int n = 123;
//    Wave_functions<true> wf1(N, n, 8, blacs_grid, blacs_grid_slice);
//    for (int i = 0; i < wf1.coeffs().num_cols_local(); i++)
//    {
//        for (int j = 0; j < wf1.coeffs().num_rows_local(); j++)
//        {
//            wf1.coeffs()(j, i) = type_wrapper<double_complex>::random();
//        }
//    }
//    auto h = wf1.coeffs().panel().hash();
//
//    wf1.swap_forward(0, n);
//    wf1.coeffs().zero();
//    wf1.swap_backward(0, n);
//
    //if (wf1.coeffs().panel().hash() != h) TERMINATE("wrong hash");
}

//void test3(std::vector<int> mpi_grid_dims__, double cutoff__)
//{
//    MPI_grid mpi_grid(mpi_grid_dims__, mpi_comm_world());
//
//    vector3d<int> fft_grid_dims(3, 3, 3);
//    FFT3D_grid fft_grid(fft_grid_dims);
//
//    matrix3d<double> M;
//    M(0, 0) = M(1, 1) = M(2, 2) = 1.0;
//
//    Gvec gvec(vector3d<double>(0, 0, 0), M, cutoff__, fft_grid, mpi_grid.communicator(1 << 0),
//              mpi_grid.dimension_size(1), false, false);
//    
//    int nwf = 8;
//    Wave_functions<false> wf(nwf, nwf, gvec, mpi_grid, CPU);
//    for (int i = 0; i < nwf; i++)
//    {
//        for (int ig = 0; ig < gvec.num_gvec(mpi_comm_world().rank()); ig++) wf(ig, i) = gvec.offset_gvec(mpi_comm_world().rank()) + ig;
//    }
//
//    for (int r = 0; r < mpi_comm_world().size(); r++)
//    {
//        if (r == mpi_comm_world().rank())
//        {
//            printf("rank: %i\n", r);
//            for (int ig = 0; ig < gvec.num_gvec(mpi_comm_world().rank()); ig++) 
//            {
//                for (int i = 0; i < nwf; i++) printf("%4.f ", wf(ig, i).real());
//                printf("\n");
//            }
//        }
//        mpi_comm_world().barrier();
//    }
//
//    wf.swap_forward(0, nwf);
//
//    for (int r = 0; r < mpi_grid.dimension_size(0); r++)
//    {
//        for (int c = 0; c < mpi_grid.dimension_size(1); c++)
//        {
//            if (r == mpi_grid.communicator(1 << 0).rank() && c == mpi_grid.communicator(1 << 1).rank())
//            {
//                printf("rank: %i %i, row: %i col: %i\n", mpi_comm_world().rank(), mpi_grid.communicator().rank(), r, c);
//                for (int ig = 0; ig < gvec.num_gvec_fft(); ig++) 
//                {
//                    for (int i = 0; i < wf.spl_num_swapped().local_size(); i++) printf("%4.f ", wf[i][ig].real());
//                    printf("\n");
//                }
//            }
//            mpi_comm_world().barrier();
//        }
//    }
//}

void test4(std::vector<int> mpi_grid_dims__, double cutoff__, int nwf__)
{
    MPI_grid mpi_grid1(mpi_grid_dims__, mpi_comm_world());
    MPI_grid mpi_grid2({1, mpi_comm_world().size()}, mpi_comm_world());

    matrix3d<double> M = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};

    FFT3D_grid fft_grid(cutoff__, M);

    Gvec gvec(vector3d<double>(0, 0, 0), M, cutoff__, fft_grid, mpi_comm_world().size(), false, false);

    Gvec_FFT_distribution gvec_fft_distr1(gvec, mpi_grid1);
    Gvec_FFT_distribution gvec_fft_distr2(gvec, mpi_grid2);

    Wave_functions<false> wf(gvec.num_gvec(mpi_comm_world().rank()), nwf__, CPU);
    for (int i = 0; i < nwf__; i++)
    {
        for (int ig = 0; ig < wf.num_gvec_loc(); ig++) wf(ig, i) = type_wrapper<double_complex>::random();
    }

    auto h1 = mdarray<double_complex, 2>(&wf(0, 0), wf.num_gvec_loc(), nwf__).hash();

    wf.swap_forward(0, nwf__, gvec_fft_distr1);
    wf.swap_backward(0, nwf__, gvec_fft_distr1);

    auto h2 = mdarray<double_complex, 2>(&wf(0, 0), wf.num_gvec_loc(), nwf__).hash();

    if (h1 != h2)
    {
        TERMINATE("swap failed");
    }
    else
    {
        printf("OK first swap\n");
    }

    printf("swap forward\n");
    wf.swap_forward(0, nwf__, gvec_fft_distr2);
    printf("swap backward\n");
    wf.swap_backward(0, nwf__, gvec_fft_distr2);

    auto h3 = mdarray<double_complex, 2>(&wf(0, 0), wf.num_gvec_loc(), nwf__).hash();

    if (h1 != h3)
    {
        TERMINATE("swap failed");
    }
}

int main(int argn, char **argv)
{
    cmd_args args;
    args.register_key("--dims=", "{vector3d<int>} FFT dimensions");
    args.register_key("--cutoff=", "{double} cutoff radius in G-space");
    args.register_key("--num_bands=", "{int} number of bands");
    args.register_key("--mpi_grid=", "{vector2d<int>} MPI grid");

    args.parse_args(argn, argv);
    if (args.exist("help"))
    {
        printf("Usage: %s [options]\n", argv[0]);
        args.print_help();
        return 0;
    }

    //vector3d<int> dims = args.value< vector3d<int> >("dims");
    double cutoff = args.value<double>("cutoff", 1);
    int num_bands = args.value<int>("num_bands", 50);
    std::vector<int> mpi_grid = args.value< std::vector<int> >("mpi_grid", {1, 1});

    //Platform::initialize(1);

    ////test1(dims, cutoff, num_bands, mpi_grid);
    //test2(mpi_grid);
    
    //Timer::print();
    sirius::initialize(true);

    //test3(mpi_grid, cutoff);
    test4(mpi_grid, cutoff, num_bands);

    sirius::finalize();
}
