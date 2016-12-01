#include <unistd.h>
#include <cublas_v2.h>
#include "cuda_common.h"

cublasHandle_t cublas_null_stream_handle;
std::vector<cublasHandle_t> cublas_handles;

void cublas_error_message(cublasStatus_t status)
{
    switch (status) {
        case CUBLAS_STATUS_NOT_INITIALIZED: {
            printf("the library was not initialized\n");
            break;
        }
        case CUBLAS_STATUS_INVALID_VALUE: {
            printf("the parameters m,n,k<0\n");
            break;
        }
        case CUBLAS_STATUS_ARCH_MISMATCH: {
            printf("the device does not support double-precision\n");
            break;
        }
        case CUBLAS_STATUS_EXECUTION_FAILED: {
            printf("the function failed to launch on the GPU\n");
            break;
        }
        default: {
            printf("cublas status unknown");
        }
    }
}

#ifdef NDEBUG
    #define CALL_CUBLAS(func__, args__)                                                 \
    {                                                                                   \
        cublasStatus_t status;                                                          \
        if ((status = func__ args__) != CUBLAS_STATUS_SUCCESS)                          \
        {                                                                               \
            cublas_error_message(status);                                               \
            char nm[1024];                                                              \
            gethostname(nm, 1024);                                                      \
            printf("hostname: %s\n", nm);                                               \
            printf("Error in %s at line %i of file %s\n", #func__, __LINE__, __FILE__); \
            stack_backtrace();                                                          \
        }                                                                               \
    }
#else
    #define CALL_CUBLAS(func__, args__)                                                 \
    {                                                                                   \
        cublasStatus_t status;                                                          \
        func__ args__;                                                                  \
        cudaDeviceSynchronize();                                                        \
        status = cublasGetError();                                                      \
        if (status != CUBLAS_STATUS_SUCCESS)                                            \
        {                                                                               \
            cublas_error_message(status);                                               \
            char nm[1024];                                                              \
            gethostname(nm, 1024);                                                      \
            printf("hostname: %s\n", nm);                                               \
            printf("Error in %s at line %i of file %s\n", #func__, __LINE__, __FILE__); \
            stack_backtrace();                                                          \
        }                                                                               \
    }
#endif

extern "C" void cublas_create_handles(int num_handles)
{
    CALL_CUBLAS(cublasCreate, (&cublas_null_stream_handle));
    
    cublas_handles = std::vector<cublasHandle_t>(num_handles);
    for (int i = 0; i < num_handles; i++)
    {
        CALL_CUBLAS(cublasCreate, (&cublas_handles[i]));

        CALL_CUBLAS(cublasSetStream, (cublas_handles[i], cuda_stream_by_id(i)));
    }
}

extern "C" void cublas_destroy_handles(int num_handles)
{
    CALL_CUBLAS(cublasDestroy, (cublas_null_stream_handle));
    for (int i = 0; i < num_handles; i++)
    {
        CALL_CUBLAS(cublasDestroy, (cublas_handles[i]));
    }
}

cublasHandle_t cublas_handle_by_id(int id__)
{
    return (id__ == -1) ? cublas_null_stream_handle : cublas_handles[id__];
}



extern "C" void cublas_zgemv(int transa, int32_t m, int32_t n, cuDoubleComplex* alpha, cuDoubleComplex* a, int32_t lda, 
                             cuDoubleComplex* x, int32_t incx, cuDoubleComplex* beta, cuDoubleComplex* y, int32_t incy, 
                             int stream_id)
{
    const cublasOperation_t trans[] = {CUBLAS_OP_N, CUBLAS_OP_T, CUBLAS_OP_C};

    CALL_CUBLAS(cublasZgemv, (cublas_handle_by_id(stream_id), trans[transa], m, n, alpha, a, lda, x, incx, beta, y, incy));
}

extern "C" void cublas_zgemm(int transa, int transb, int32_t m, int32_t n, int32_t k, 
                             cuDoubleComplex* alpha, cuDoubleComplex* a, int32_t lda, cuDoubleComplex* b, 
                             int32_t ldb, cuDoubleComplex* beta, cuDoubleComplex* c, int32_t ldc, int stream_id)
{
    const cublasOperation_t trans[] = {CUBLAS_OP_N, CUBLAS_OP_T, CUBLAS_OP_C};
    
    CALL_CUBLAS(cublasZgemm, (cublas_handle_by_id(stream_id), trans[transa], trans[transb], m, n, k, alpha, a, lda, b, ldb, beta, c, ldc));
}

extern "C" void cublas_dgemm(int transa, int transb, int32_t m, int32_t n, int32_t k, 
                             double* alpha, double* a, int32_t lda, double* b, 
                             int32_t ldb, double* beta, double* c, int32_t ldc, int stream_id)
{
    const cublasOperation_t trans[] = {CUBLAS_OP_N, CUBLAS_OP_T, CUBLAS_OP_C};
    
    CALL_CUBLAS(cublasDgemm, (cublas_handle_by_id(stream_id), trans[transa], trans[transb], m, n, k, alpha, a, lda, b, ldb, beta, c, ldc));
}

extern "C" void cublas_dtrmm(char side__, char uplo__, char transa__, char diag__, int m__, int n__,
                             double* alpha__, double* A__, int lda__, double* B__, int ldb__)
{
    if (!(side__ == 'L' || side__ == 'R'))
    {
        printf("cublas_dtrmm: wrong side\n");
        exit(-1);
    }
    cublasSideMode_t side = (side__ == 'L') ? CUBLAS_SIDE_LEFT : CUBLAS_SIDE_RIGHT;

    if (!(uplo__ == 'U' || uplo__ == 'L'))
    {
        printf("cublas_dtrmm: wrong uplo\n");
        exit(-1);
    }
    cublasFillMode_t uplo = (uplo__ == 'U') ? CUBLAS_FILL_MODE_UPPER : CUBLAS_FILL_MODE_LOWER;

    if (!(transa__ == 'N' || transa__ == 'T' || transa__ == 'C'))
    {
        printf("cublas_dtrmm: wrong transa\n");
        exit(-1);
    }
    cublasOperation_t transa = CUBLAS_OP_N;
    if (transa__ == 'T') transa = CUBLAS_OP_T;
    if (transa__ == 'C') transa = CUBLAS_OP_C;

    if (!(diag__ == 'N' || diag__ == 'U'))
    {
        printf("cublas_dtrmm: wrong diag\n");
        exit(-1);
    }
    cublasDiagType_t diag = (diag__ == 'N') ? CUBLAS_DIAG_NON_UNIT : CUBLAS_DIAG_UNIT;

    CALL_CUBLAS(cublasDtrmm, (cublas_null_stream_handle, side, uplo, transa, diag, m__, n__, alpha__, A__, lda__, B__, ldb__, B__, ldb__));
}

extern "C" void cublas_ztrmm(char side__, char uplo__, char transa__, char diag__, int m__, int n__,
                             cuDoubleComplex* alpha__, cuDoubleComplex* A__, int lda__, cuDoubleComplex* B__, int ldb__)
{
    if (!(side__ == 'L' || side__ == 'R'))
    {
        printf("cublas_ztrmm: wrong side\n");
        exit(-1);
    }
    cublasSideMode_t side = (side__ == 'L') ? CUBLAS_SIDE_LEFT : CUBLAS_SIDE_RIGHT;

    if (!(uplo__ == 'U' || uplo__ == 'L'))
    {
        printf("cublas_ztrmm: wrong uplo\n");
        exit(-1);
    }
    cublasFillMode_t uplo = (uplo__ == 'U') ? CUBLAS_FILL_MODE_UPPER : CUBLAS_FILL_MODE_LOWER;

    if (!(transa__ == 'N' || transa__ == 'T' || transa__ == 'C'))
    {
        printf("cublas_ztrmm: wrong transa\n");
        exit(-1);
    }
    cublasOperation_t transa = CUBLAS_OP_N;
    if (transa__ == 'T') transa = CUBLAS_OP_T;
    if (transa__ == 'C') transa = CUBLAS_OP_C;

    if (!(diag__ == 'N' || diag__ == 'U'))
    {
        printf("cublas_ztrmm: wrong diag\n");
        exit(-1);
    }
    cublasDiagType_t diag = (diag__ == 'N') ? CUBLAS_DIAG_NON_UNIT : CUBLAS_DIAG_UNIT;

    CALL_CUBLAS(cublasZtrmm, (cublas_null_stream_handle, side, uplo, transa, diag, m__, n__, alpha__, A__, lda__, B__, ldb__, B__, ldb__));
}

extern "C" void cublas_dger(int m, int n, double* alpha, double* x, int incx, double* y, int incy, double* A, int lda, int stream_id)
{
    CALL_CUBLAS(cublasDger, (cublas_handle_by_id(stream_id), m, n, alpha, x, incx, y, incy, A, lda));
}

extern "C" void cublas_zgeru(int                    m,
                             int                    n,
                             cuDoubleComplex const* alpha,
                             cuDoubleComplex const* x,
                             int                    incx,
                             cuDoubleComplex const* y,
                             int                    incy,
                             cuDoubleComplex*       A,
                             int                    lda, 
                             int                    stream_id)
{
    CALL_CUBLAS(cublasZgeru, (cublas_handle_by_id(stream_id), m, n, alpha, x, incx, y, incy, A, lda));
}

extern "C" void cublas_zaxpy(int                    n__,
                             cuDoubleComplex const* alpha__,
                             cuDoubleComplex const* x__,
                             int                    incx__,
                             cuDoubleComplex*       y__,
                             int                    incy__)
{
    CALL_CUBLAS(cublasZaxpy, (cublas_null_stream_handle, n__, alpha__, x__, incx__, y__, incy__));
}

