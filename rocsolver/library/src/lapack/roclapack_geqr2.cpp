/* ************************************************************************
 * Copyright 2019-2020 Advanced Micro Devices, Inc.
 * ************************************************************************ */

#include "roclapack_geqr2.hpp"

template <typename T, typename U>
rocblas_status rocsolver_geqr2_impl(rocblas_handle handle, const rocblas_int m,
                                        const rocblas_int n, U A, const rocblas_int lda,
                                        T* ipiv) 
{ 
    if(!handle)
        return rocblas_status_invalid_handle;
    
    //logging is missing ???    
    
    // argument checking
    if (!A || !ipiv)
        return rocblas_status_invalid_pointer;
    if (m < 0 || n < 0 || lda < m)
        return rocblas_status_invalid_size;

    rocblas_stride strideA = 0;
    rocblas_stride stridep = 0;
    rocblas_int batch_count = 1;

    // memory managment
    size_t size_1;  //size of constants
    size_t size_2;  //size of workspace
    size_t size_3;  //size of array of pointers to workspace
    size_t size_4;
    rocsolver_geqr2_getMemorySize<T,false>(m,n,batch_count,&size_1,&size_2,&size_3,&size_4);

    // (TODO) MEMORY SIZE QUERIES AND ALLOCATIONS TO BE DONE WITH ROCBLAS HANDLE
    void *scalars, *work, *workArr, *diag;
    hipMalloc(&scalars,size_1);
    hipMalloc(&work,size_2);
    hipMalloc(&workArr,size_3);
    hipMalloc(&diag,size_4);
    if (!scalars || (size_2 && !work) || (size_3 && !workArr) || (size_4 && !diag))
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
           rocsolver_geqr2_template<T>(handle,m,n,
                                    A,0,    //the matrix is shifted 0 entries (will work on the entire matrix)
                                    lda,strideA,
                                    ipiv,
                                    stridep,
                                    batch_count,
                                    (T*)scalars,
                                    (T*)work,
                                    (T**)workArr,
                                    (T*)diag);

    hipFree(scalars);
    hipFree(work);
    hipFree(workArr);
    hipFree(diag);
    return status;
}


/*
 * ===========================================================================
 *    C wrapper
 * ===========================================================================
 */

extern "C" {

ROCSOLVER_EXPORT rocblas_status rocsolver_sgeqr2(rocblas_handle handle, const rocblas_int m, const rocblas_int n, float *A,
                 const rocblas_int lda, float *ipiv) 
{
    return rocsolver_geqr2_impl<float>(handle, m, n, A, lda, ipiv);
}

ROCSOLVER_EXPORT rocblas_status rocsolver_dgeqr2(rocblas_handle handle, const rocblas_int m, const rocblas_int n, double *A,
                 const rocblas_int lda, double *ipiv) 
{
    return rocsolver_geqr2_impl<double>(handle, m, n, A, lda, ipiv);
}

ROCSOLVER_EXPORT rocblas_status rocsolver_cgeqr2(rocblas_handle handle, const rocblas_int m, const rocblas_int n, rocblas_float_complex *A,
                 const rocblas_int lda, rocblas_float_complex *ipiv) 
{
    return rocsolver_geqr2_impl<rocblas_float_complex>(handle, m, n, A, lda, ipiv);
}

ROCSOLVER_EXPORT rocblas_status rocsolver_zgeqr2(rocblas_handle handle, const rocblas_int m, const rocblas_int n, rocblas_double_complex *A,
                 const rocblas_int lda, rocblas_double_complex *ipiv) 
{
    return rocsolver_geqr2_impl<rocblas_double_complex>(handle, m, n, A, lda, ipiv);
}

} //extern C
