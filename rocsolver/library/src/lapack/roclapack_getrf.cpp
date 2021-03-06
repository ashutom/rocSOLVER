/* ************************************************************************
 * Copyright 2019-2020 Advanced Micro Devices, Inc.
 * ************************************************************************ */

#include "roclapack_getrf.hpp"

template <typename T, typename U>
rocblas_status rocsolver_getrf_impl(rocblas_handle handle, const rocblas_int m,
                                        const rocblas_int n, U A, const rocblas_int lda,
                                        rocblas_int *ipiv, rocblas_int* info) {
    if(!handle)
        return rocblas_status_invalid_handle;
    
    //logging is missing ???    

    // argument checking
    if (m < 0 || n < 0 || lda < m) 
        return rocblas_status_invalid_size;
    if (!A || !ipiv || !info)
        return rocblas_status_invalid_pointer;

    rocblas_stride strideA = 0;
    rocblas_stride strideP = 0;
    rocblas_int batch_count = 1;

    // memory managment
    size_t size_1;  //size of constants
    size_t size_2;
    size_t size_3;
    rocsolver_getrf_getMemorySize<T>(m,n,batch_count,&size_1,&size_2,&size_3);

    // (TODO) MEMORY SIZE QUERIES AND ALLOCATIONS TO BE DONE WITH ROCBLAS HANDLE
    void *scalars, *pivotGPU, *iinfo;
    hipMalloc(&scalars,size_1);
    hipMalloc(&pivotGPU,size_2);
    hipMalloc(&iinfo,size_3);
    if (!scalars || (size_2 && !pivotGPU) || (size_3 && !iinfo))
        return rocblas_status_memory_error;

    // scalars constants for rocblas functions calls
    // (to standarize and enable re-use, size_1 always equals 3)
    std::vector<T> sca(size_1);
    sca[0] = -1;
    sca[1] = 0;
    sca[2] = 1;
    RETURN_IF_HIP_ERROR(hipMemcpy(scalars, sca.data(), sizeof(T)*size_1, hipMemcpyHostToDevice));

    // execution
    rocblas_status status =
           rocsolver_getrf_template<false,false,T>(handle,m,n,
                                                    A,0,    //The matrix is shifted 0 entries (will work on the entire matrix)
                                                    lda,strideA,
                                                    ipiv,0, //the vector is shifted 0 entries (will work on the entire vector)
                                                    strideP,
                                                    info,batch_count,
                                                    (T*)scalars,
                                                    (T*)pivotGPU,
                                                    (rocblas_int*)iinfo);

    hipFree(scalars);
    hipFree(pivotGPU);
    hipFree(iinfo);
    return status;
}


/*
 * ===========================================================================
 *    C wrapper
 * ===========================================================================
 */

extern "C" {

ROCSOLVER_EXPORT rocblas_status rocsolver_sgetrf(rocblas_handle handle, const rocblas_int m, const rocblas_int n,
                 float *A, const rocblas_int lda, rocblas_int *ipiv, rocblas_int* info) 
{
    return rocsolver_getrf_impl<float>(handle, m, n, A, lda, ipiv, info);
}

ROCSOLVER_EXPORT rocblas_status rocsolver_dgetrf(rocblas_handle handle, const rocblas_int m, const rocblas_int n,
                 double *A, const rocblas_int lda, rocblas_int *ipiv, rocblas_int* info) 
{
    return rocsolver_getrf_impl<double>(handle, m, n, A, lda, ipiv, info);
}

ROCSOLVER_EXPORT rocblas_status rocsolver_cgetrf(rocblas_handle handle, const rocblas_int m, const rocblas_int n,
                 rocblas_float_complex *A, const rocblas_int lda, rocblas_int *ipiv, rocblas_int* info) 
{
    return rocsolver_getrf_impl<rocblas_float_complex>(handle, m, n, A, lda, ipiv, info);
}

ROCSOLVER_EXPORT rocblas_status rocsolver_zgetrf(rocblas_handle handle, const rocblas_int m, const rocblas_int n,
                 rocblas_double_complex *A, const rocblas_int lda, rocblas_int *ipiv, rocblas_int* info) 
{
    return rocsolver_getrf_impl<rocblas_double_complex>(handle, m, n, A, lda, ipiv, info);
}

} //extern C
