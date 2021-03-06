/* ************************************************************************
 * Copyright 2019-2020 Advanced Micro Devices, Inc.
 * ************************************************************************ */

#include "rocauxiliary_ormbr_unmbr.hpp"

template <typename T, bool COMPLEX = is_complex<T>>
rocblas_status rocsolver_ormbr_unmbr_impl(rocblas_handle handle, const rocblas_storev storev, const rocblas_side side, const rocblas_operation trans, 
                                          const rocblas_int m, const rocblas_int n, 
                                          const rocblas_int k, T* A, const rocblas_int lda, T* ipiv, T *C, const rocblas_int ldc)
{
    if(!handle)
        return rocblas_status_invalid_handle;

    //logging is missing ???

    // argument checking
    if (m < 0 || n < 0 ||  k < 0 || ldc < m)
        return rocblas_status_invalid_size;
    
    rocblas_int nq = side == rocblas_side_left ? m : n;

    if (storev == rocblas_column_wise && lda < nq)
        return rocblas_status_invalid_size;
    if (storev == rocblas_row_wise && lda < min(nq,k))
        return rocblas_status_invalid_size;
    if (!A || !ipiv || !C)
        return rocblas_status_invalid_pointer;
    if (COMPLEX && trans == rocblas_operation_transpose)
        return rocblas_status_invalid_value;
    if (!COMPLEX && trans == rocblas_operation_conjugate_transpose)
        return rocblas_status_invalid_value;

    rocblas_int strideA = 0;
    rocblas_int strideP = 0;
    rocblas_int strideC = 0;
    rocblas_int batch_count=1;

    // memory managment
    size_t size_1;  //size of constants
    size_t size_2;  //size of workspace
    size_t size_3;  //size of array of pointers to workspace
    size_t size_4;  // size of temporary array for triangular factor
    rocsolver_ormbr_unmbr_getMemorySize<T,false>(storev,side,m,n,k,batch_count,&size_1,&size_2,&size_3,&size_4);

    // (TODO) MEMORY SIZE QUERIES AND ALLOCATIONS TO BE DONE WITH ROCBLAS HANDLE
    void *scalars, *work, *workArr, *trfact;
    hipMalloc(&scalars,size_1);
    hipMalloc(&work,size_2);
    hipMalloc(&workArr,size_3);
    hipMalloc(&trfact,size_4);
    if (!scalars || (size_2 && !work) || (size_3 && !workArr) || (size_4 && !trfact))
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
           rocsolver_ormbr_unmbr_template<false,false,T>(handle,storev,side,trans,
                                                         m,n,k,
                                                         A,0,    //shifted 0 entries
                                                         lda,
                                                         strideA,
                                                         ipiv,
                                                         strideP,
                                                         C,0,  
                                                         ldc,
                                                         strideC,
                                                         batch_count,
                                                         (T*)scalars,
                                                         (T*)work,
                                                         (T**)workArr,
                                                         (T*)trfact);

    hipFree(scalars);
    hipFree(work);
    hipFree(workArr);
    hipFree(trfact);
    return status;
}


/*
 * ===========================================================================
 *    C wrapper
 * ===========================================================================
 */

extern "C" {

ROCSOLVER_EXPORT rocblas_status rocsolver_sormbr(rocblas_handle handle,
                                                 const rocblas_storev storev,
                                                 const rocblas_side side,
                                                 const rocblas_operation trans,
                                                 const rocblas_int m,
                                                 const rocblas_int n,
                                                 const rocblas_int k,
                                                 float *A,
                                                 const rocblas_int lda,
                                                 float *ipiv,
                                                 float *C,
                                                 const rocblas_int ldc)
{
    return rocsolver_ormbr_unmbr_impl<float>(handle, storev, side, trans, m, n, k, A, lda, ipiv, C, ldc);
}

ROCSOLVER_EXPORT rocblas_status rocsolver_dormbr(rocblas_handle handle,
                                                 const rocblas_storev storev,
                                                 const rocblas_side side,
                                                 const rocblas_operation trans,
                                                 const rocblas_int m,
                                                 const rocblas_int n,
                                                 const rocblas_int k,
                                                 double *A,
                                                 const rocblas_int lda,
                                                 double *ipiv,
                                                 double *C,
                                                 const rocblas_int ldc)
{
    return rocsolver_ormbr_unmbr_impl<double>(handle, storev, side, trans, m, n, k, A, lda, ipiv, C, ldc);
}

ROCSOLVER_EXPORT rocblas_status rocsolver_cunmbr(rocblas_handle handle,
                                                 const rocblas_storev storev,
                                                 const rocblas_side side,
                                                 const rocblas_operation trans,
                                                 const rocblas_int m,
                                                 const rocblas_int n,
                                                 const rocblas_int k,
                                                 rocblas_float_complex *A,
                                                 const rocblas_int lda,
                                                 rocblas_float_complex *ipiv,
                                                 rocblas_float_complex *C,
                                                 const rocblas_int ldc)
{
    return rocsolver_ormbr_unmbr_impl<rocblas_float_complex>(handle, storev, side, trans, m, n, k, A, lda, ipiv, C, ldc);
}

ROCSOLVER_EXPORT rocblas_status rocsolver_zunmbr(rocblas_handle handle,
                                                 const rocblas_storev storev,
                                                 const rocblas_side side,
                                                 const rocblas_operation trans,
                                                 const rocblas_int m,
                                                 const rocblas_int n,
                                                 const rocblas_int k,
                                                 rocblas_double_complex *A,
                                                 const rocblas_int lda,
                                                 rocblas_double_complex *ipiv,
                                                 rocblas_double_complex *C,
                                                 const rocblas_int ldc)
{
    return rocsolver_ormbr_unmbr_impl<rocblas_double_complex>(handle, storev, side, trans, m, n, k, A, lda, ipiv, C, ldc);
}

} //extern C

