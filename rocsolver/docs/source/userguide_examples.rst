
*************************
Using rocSOLVER 
*************************

.. toctree::
   :maxdepth: 4
   :caption: Contents:

Once installed, rocSOLVER can be used just like any other library with a C API. 
The header file will need to be included in the user code, and both the rocBLAS and rocSOLVER shared libraries 
will become link-time and run-time dependencies for the user applciation.

QR factorization of a single matrix
================================================

The following code snippet uses rocSOLVER to compute the QR factorization of a general m-by-n real matrix in double precsision. 
For a full description of the used rocSOLVER routine, see the API documentation here: :ref:`qr_label`. 

.. code-block:: C

    ///////////////////////////
    // example.c source code //
    ///////////////////////////

    #include <iostream>
    #include <stdlib.h>
    #include <vector>
    #include <rocsolver.h>      // this includes all the rocsolver C interfaces and type declarations

    using namespace std;

    int main() {
        rocsolver_int M;
        rocsolver_int N;
        rocsolver_int lda;

        // initialize M, N and lda with desired values
        // here===>>

        rocsolver_handle handle;
        rocsolver_create_handle(&handle); // this creates the rocsolver handle

        size_t size_A = size_t(lda) * N;     // this is the size of the array that will hold the matrix
        size_t size_piv = size_t(min(M, N)); // this is size of array that will have the Householder scalars   

        vector<double> hA(size_A);        // creates array for matrix in CPU
        vector<double> hIpiv(size_piv);   // creates array for householder scalars in CPU

        double *dA, *dIpiv;
        hipMalloc(&dA,sizeof(double)*size_A);       // allocates memory for matrix in GPU
        hipMalloc(&dIpiv,sizeof(double)*size_piv);  // allocates memory for scalars in GPU
  
        // initialize matrix A (array hA) with input data
        // here===>>
        // ( matrices must be stored in column major format, i.e. entry (i,j) 
        //  should be accessed by hA[i + j*lda] )

        hipMemcpy(dA,hA.data(),sizeof(double)*size_A,hipMemcpyHostToDevice); // copy data to GPU
        rocsolver_dgeqrf(handle, M, N, dA, lda, dIpiv);                      // compute the QR factorization on the GPU   
        hipMemcpy(hA.data(),dA,sizeof(double)*size_A,hipMemcpyDeviceToHost); // copy the results back to CPU
        hipMemcpy(hIpiv.data(),dIpiv,sizeof(double)*size_piv,hipMemcpyDeviceToHost);

        // do something with the results in hA and hIpiv
        // here===>>

        hipFree(dA);                        // de-allocate GPU memory 
        hipFree(dIpiv);
        rocsolver_destroy_handle(handle);   // destroy handle
  
        return 0;
    }

The compile command may vary depending on the system and session environment. Here is an example of a common use case:

.. code-block:: bash

    hipcc -I/opt/rocm/include -L/opt/rocm/lib -lrocsolver -lrocblas example.c -o example.exe  


QR factorization of a batch of matrices
================================================

One of the advantages of using GPUs is the ability to execute in parallel many operations of the same type but on different data sets. 
Based on this idea, rocSOLVER and rocBLAS provide a `batch` version of most of their routines. These batch versions allow the user to execute 
the same operation on a set of different matrices and/or vectors with a single library call. For more details on the approach to batch
functionality followed in rocSOLVER, see :ref:`batch_label`.

Strided_batched version
---------------------------

The following code snippet uses rocSOLVER to compute the QR factorization of a series of general m-by-n real matrices in double precsision. 
The matrices must be stored in contiguous memory locations on the GPU, and are accessed by a pointer to the first matrix and a 
stride value that gives the separation between one matrix and the next one. 
For a full description of the used rocSOLVER routine, see the API documentation here: :ref:`qr_strided_label`. 

.. code-block:: C

    ///////////////////////////////////
    // example_strided.c source code //
    ///////////////////////////////////

    #include <iostream>
    #include <stdlib.h>
    #include <vector>
    #include <rocsolver.h>      //  this includes all the rocsolver C interfaces and type declarations

    using namespace std;

    int main() {
        rocsolver_int M;
        rocsolver_int N;
        rocsolver_int lda;
        rocsolver_int batch_count;  // number of matrices to factorize in the batch

        // initialize batch_count, M, N and lda with desired values
        // here===>>

        rocsolver_handle handle;
        rocsolver_create_handle(&handle); // this creates the rocsolver handle

        rocblas_int strideA = lda*N;                   // separation between two matrices in memory
        size_t size_A = size_t(strideA)*batch_count;   // size of the array that holds the matrices
        rocblas_int strideP = min(M,N);                // separation between two sets of Householder scalars
        size_t size_piv = size_t(strideP)*batch_count; // size of the array that will have the Householder scalars
        
        vector<double> hA(size_A);        // creates array for matrices in CPU
        vector<double> hIpiv(size_piv);   // creates array for householder scalars in CPU

        double *dA, *dIpiv;
        hipMalloc(&dA,sizeof(double)*size_A);       // allocates memory for matrices in GPU
        hipMalloc(&dIpiv,sizeof(double)*size_piv);  // allocates memory for scalars in GPU
  
        // initialize all matrices (array hA) with input data
        // here===>>
        // ( matrices must be stored in column major format, i.e. entry (i,j)
        //   of the k-th matrix in the batch should be accessed by hA[i + j*lda + k*strideA] )

        hipMemcpy(dA,hA.data(),sizeof(double)*size_A,hipMemcpyHostToDevice);          // copy data to GPU

        rocsolver_dgeqrf_strided_batched(handle, M, N, dA,
                                         lda, strideA, dIpiv, strideP, batch_count);  // compute the QR factorization on the GPU   

        hipMemcpy(hA.data(),dA,sizeof(double)*size_A,hipMemcpyDeviceToHost);          // copy the results back to CPU
        hipMemcpy(hIpiv.data(),dIpiv,sizeof(double)*size_piv,hipMemcpyDeviceToHost);

        // do something with the results in hA and hIpiv
        // here===>>

        hipFree(dA);                        // de-allocate GPU memory 
        hipFree(dIpiv);
        rocsolver_destroy_handle(handle);   // destroy handle
  
        return 0;
    }

Batched version
---------------------------

The following code snippet uses rocSOLVER to compute the QR factorization of a series of general m-by-n real matrices in double precsision. 
The matrices do not need to be in contiguous memory locations on the GPU, and will be accessed by an array of pointers. 
For a full description of the used rocSOLVER routine, see the API documentation here: :ref:`qr_batched_label`. 

.. code-block:: C

    ///////////////////////////////////
    // example_batched.c source code //
    ///////////////////////////////////

    #include <iostream>
    #include <stdlib.h>
    #include <vector>
    #include <rocsolver.h>      //  this includes all the rocsolver C interfaces and type declarations

    using namespace std;

    int main() {
        rocsolver_int M;
        rocsolver_int N;
        rocsolver_int lda;
        rocsolver_int batch_count;  // number of matrices to factorize in the batch

        // initialize batch_count, M, N and lda with desired values
        // here===>>

        rocsolver_handle handle;
        rocsolver_create_handle(&handle); // this creates the rocsolver handle

        size_t size_A = size_t(lda)*N;                 // size of the array that holds one matrix
        rocblas_int strideP = min(M,N);                // separation between two sets of Householder scalars
        size_t size_piv = size_t(strideP)*batch_count; // size of the array that will have the Householder scalars
        
        vector<double> hIpiv(size_piv);         // creates array for householder scalars in CPU
        vector<double> hA[batch_count];         // creates array on the CPU of pointers to the CPU
        for(int b=0; b < batch_count; ++b) {
            hA[b] = vector<double>(size_A);     // each pointer will point to a matrix of the batch on the CPU
        }

        double* A[batch_count];                      // creates array on the CPU of pointers to the GPU
        for (int b = 0; b < batch_count; ++b)
            hipMalloc(&A[b], sizeof(double)*size_A); // each pointer will point to a matrix of the batch on the GPU

        double **dA, *dIpiv;
        hipMalloc(&dA,sizeof(double*) * size_A);    // array on the GPU of pointers on the GPU
        hipMalloc(&dIpiv,sizeof(double)*size_piv);  // allocates memory for scalars in GPU
  
        // initialize all matrices (arrays hA[k]) with input data
        // here===>>
        // ( matrices must be stored in column major format, i.e. entry (i,j)
        //   of the k-th matrix in the batch should be accessed by hA[k][i + j*lda] )

        for(int b=0; b < batch_count; ++b)
            hipMemcpy(A[b], hA[b].data(), sizeof(double)*size_A, hipMemcpyHostToDevice);    // copy data to GPU
        hipMemcpy(dA, A, sizeof(double*) * batch_count, hipMemcpyHostToDevice);             // copy pointers to GPU
        
        rocsolver_dgeqrf_batched(handle, M, N, dA,
                                 lda, dIpiv, strideP, batch_count);  // compute the QR factorization on the GPU   

        for(int b=0;b<batch_count;b++)
            hipMemcpy(hA[b].data(), A[b], sizeof(double) * size_A, hipMemcpyDeviceToHost); // copy the results back
        hipMemcpy(hIpiv.data(),dIpiv,sizeof(double)*size_piv,hipMemcpyDeviceToHost);

        // do something with the results in hA and hIpiv
        // here===>>

        for(int b=0;b<batch_count;++b)
            hipFree(A[b]);
        hipFree(dA);                        // de-allocate GPU memory 
        hipFree(dIpiv);
        rocsolver_destroy_handle(handle);   // destroy handle
  
        return 0;
    }

