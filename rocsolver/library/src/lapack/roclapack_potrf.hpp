/************************************************************************
 * Derived from the BSD3-licensed
 * LAPACK routine (version 3.1) --
 *     Univ. of Tennessee, Univ. of California Berkeley,
 *     Univ. of Colorado Denver and NAG Ltd..
 *     December 2016
 * Copyright 2019-2020 Advanced Micro Devices, Inc.
 * ***********************************************************************/

#ifndef ROCLAPACK_POTRF_HPP
#define ROCLAPACK_POTRF_HPP

#include "rocblas.hpp"
#include "rocsolver.h"
#include "common_device.hpp"
#include "ideal_sizes.hpp"
#include "roclapack_potf2.hpp"

template<typename U>
__global__ void chk_positive(rocblas_int *iinfo, rocblas_int *info, int j) 
{
    int id = hipBlockIdx_x;

    if (info[id] == 0 && iinfo[id] > 0)
            info[id] = iinfo[id] + j;   
}

template <typename T>
void rocsolver_potrf_getMemorySize(const rocblas_int n, const rocblas_int batch_count,
                                  size_t *size_1, size_t *size_2, size_t *size_3, size_t *size_4)
{
    if (n < POTRF_POTF2_SWITCHSIZE) {
        rocsolver_potf2_getMemorySize<T>(n,batch_count,size_1,size_2,size_3);
        *size_4 = 0;
    } else {
        rocsolver_potf2_getMemorySize<T>(POTRF_POTF2_SWITCHSIZE,batch_count,size_1,size_2,size_3);
        *size_4 = sizeof(rocblas_int)*batch_count;
    }   
}

template <typename S, typename T, typename U, bool COMPLEX = is_complex<T>>
rocblas_status rocsolver_potrf_template(rocblas_handle handle,
                                        const rocblas_fill uplo, const rocblas_int n, U A,
                                        const rocblas_int shiftA,
                                        const rocblas_int lda, const rocblas_stride strideA,
                                        rocblas_int *info, const rocblas_int batch_count,
                                        T*scalars, T* work, T* pivotGPU, rocblas_int *iinfo)
{
    // quick return
    if (n == 0 || batch_count == 0) 
        return rocblas_status_success;

    hipStream_t stream;
    rocblas_get_stream(handle, &stream);
    
    // everything must be executed with scalars on the host
    rocblas_pointer_mode old_mode;
    rocblas_get_pointer_mode(handle,&old_mode);
    rocblas_set_pointer_mode(handle,rocblas_pointer_mode_host);

    // if the matrix is small, use the unblocked (BLAS-levelII) variant of the algorithm
    if (n < POTRF_POTF2_SWITCHSIZE) 
        return rocsolver_potf2_template<T>(handle, uplo, n, A, shiftA, lda, strideA, info, batch_count, scalars, work, pivotGPU);

    // **** THIS SYNCHRONIZATION WILL BE REQUIRED UNTIL
    //      TRSM_BATCH FUNCTIONALITY IS ENABLED. ****
    #ifdef batched
        T* AA[batch_count];
        hipMemcpy(AA, A, batch_count*sizeof(T*), hipMemcpyDeviceToHost);
    #else
        T* AA = A;
    #endif

    //constants for rocblas functions calls
    T t_one = 1;
    S s_one = 1;
    S s_minone = -1;

    rocblas_int blocksReset = (batch_count - 1) / BLOCKSIZE + 1;
    dim3 gridReset(blocksReset, 1, 1);
    dim3 threads(BLOCKSIZE, 1, 1);
    T* M;
    rocblas_int jb;

    //info=0 (starting with a positive definite matrix)
    hipLaunchKernelGGL(reset_info,gridReset,threads,0,stream,info,batch_count,0);

    // **** TRSM_BATCH IS EXECUTED IN A FOR-LOOP UNTIL 
    //      FUNCITONALITY IS ENABLED. ****

    if (uplo == rocblas_fill_upper) { // Compute the Cholesky factorization A = U'*U.
        for (rocblas_int j = 0; j < n; j += POTRF_POTF2_SWITCHSIZE) {
            // Factor diagonal and subdiagonal blocks 
            jb = min(n - j, POTRF_POTF2_SWITCHSIZE);  //number of columns in the block
            hipLaunchKernelGGL(reset_info,gridReset,threads,0,stream,iinfo,batch_count,0);
            rocsolver_potf2_template<T>(handle, uplo, jb, A, shiftA + idx2D(j, j, lda), lda, strideA, iinfo, batch_count, scalars, work, pivotGPU);
            
            // test for non-positive-definiteness.
            hipLaunchKernelGGL(chk_positive<U>,gridReset,threads,0,stream,iinfo,info,j);
            
            if (j + jb < n) {
                // update trailing submatrix
                for (int b=0;b<batch_count;++b) {
                    M = load_ptr_batch<T>(AA,b,shiftA,strideA);
                    rocblas_trsm(handle, rocblas_side_left, uplo, rocblas_operation_conjugate_transpose,
                             rocblas_diagonal_non_unit, jb, (n - j - jb), &t_one,
                             (M + idx2D(j, j, lda)), lda, (M + idx2D(j, j + jb, lda)), lda);
                }

                rocblasCall_herk<S,T>(handle, uplo, rocblas_operation_conjugate_transpose, n-j-jb, jb, &s_minone,
                                A, shiftA + idx2D(j,j+jb,lda), lda, strideA, &s_one,
                                A, shiftA + idx2D(j+jb,j+jb,lda), lda, strideA, batch_count);
            }
        }

    } else { // Compute the Cholesky factorization A = L'*L.
        for (rocblas_int j = 0; j < n; j += POTRF_POTF2_SWITCHSIZE) {
            // Factor diagonal and subdiagonal blocks 
            jb = min(n - j, POTRF_POTF2_SWITCHSIZE);  //number of columns in the block
            hipLaunchKernelGGL(reset_info,gridReset,threads,0,stream,iinfo,batch_count,0);
            rocsolver_potf2_template<T>(handle, uplo, jb, A, shiftA + idx2D(j, j, lda), lda, strideA, iinfo, batch_count, scalars, work, pivotGPU);
            
            // test for non-positive-definiteness.
            hipLaunchKernelGGL(chk_positive<U>,gridReset,threads,0,stream,iinfo,info,j);
            
            if (j + jb < n) {
                // update trailing submatrix
                for (int b=0;b<batch_count;++b) {
                    M = load_ptr_batch<T>(AA,b,shiftA,strideA);
                    rocblas_trsm(handle, rocblas_side_right, uplo, rocblas_operation_conjugate_transpose,
                             rocblas_diagonal_non_unit, (n - j - jb), jb, &t_one,
                             (M + idx2D(j, j, lda)), lda, (M + idx2D(j + jb, j, lda)), lda);
                }

                rocblasCall_herk<S,T>(handle, uplo, rocblas_operation_none, n-j-jb, jb, &s_minone,
                                A, shiftA + idx2D(j+jb,j,lda), lda, strideA, &s_one,
                                A, shiftA + idx2D(j+jb,j+jb,lda), lda, strideA, batch_count);
            }
        }
    }

    rocblas_set_pointer_mode(handle,old_mode);
    return rocblas_status_success;
}

#endif /* ROCLAPACK_POTRF_HPP */
