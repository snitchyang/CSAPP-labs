/*
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/*
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded.
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    if (M == 32 && N == 32)
    {
        //simply divide it into 8x8 blocks
        int i, j, k, l, tmp;
        for (i = 0; i < N; i += 8)
        {
            for (j = 0; j < M; j += 8)
            {
                for (k = i; k < i + 8; k++)
                {
                    for (l = j; l < j + 8; l++)
                    {
                        if (k != l)
                        {
                            B[l][k] = A[k][l];
                        }
                        else
                        {
                            tmp = A[k][l];
                        }
                    }
                    if (i == j)
                    {
                        B[k][k] = tmp;
                    }
                }
            }
        }
    }
    else if (M == 64 && N == 64)
    {
        // we continue to use 8x8 blocks, but we divide it into 4 smaller blocks
        int a_0, a_1, a_2, a_3, a_4, a_5, a_6, a_7;
        for (int i = 0; i < 64; i += 8)
        {
            for (int j = 0; j < 64; j += 8)
            {
                for (int k = i; k < i + 4; k++)
                {
                    //first copy A1 to B1 and A2 to B2
                    a_0 = A[k][j + 0];
                    a_1 = A[k][j + 1];
                    a_2 = A[k][j + 2];
                    a_3 = A[k][j + 3];
                    a_4 = A[k][j + 4];
                    a_5 = A[k][j + 5];
                    a_6 = A[k][j + 6];
                    a_7 = A[k][j + 7];

                    B[j + 0][k] = a_0;
                    B[j + 1][k] = a_1;
                    B[j + 2][k] = a_2;
                    B[j + 3][k] = a_3;
                    B[j + 0][k + 4] = a_4;
                    B[j + 1][k + 4] = a_5;
                    B[j + 2][k + 4] = a_6;
                    B[j + 3][k + 4] = a_7;
                }
                for (int k = j; k < j + 4; k++)
                {
                    //then A3 to B2 and B2 to B3
                    a_0 = B[k][i + 4];
                    a_1 = B[k][i + 5];
                    a_2 = B[k][i + 6];
                    a_3 = B[k][i + 7];

                    a_4 = A[i + 4][k];
                    a_5 = A[i + 5][k];
                    a_6 = A[i + 6][k];
                    a_7 = A[i + 7][k];

                    B[k][i + 4] = a_4;
                    B[k][i + 5] = a_5;
                    B[k][i + 6] = a_6;
                    B[k][i + 7] = a_7;

                    B[k + 4][i + 0] = a_0;
                    B[k + 4][i + 1] = a_1;
                    B[k + 4][i + 2] = a_2;
                    B[k + 4][i + 3] = a_3;
                }
                for (int k = i + 4; k < i + 8; k++)
                {
                    //finally A4 to B4
                    a_4 = A[k][j + 4];
                    a_5 = A[k][j + 5];
                    a_6 = A[k][j + 6];
                    a_7 = A[k][j + 7];
                    B[j + 4][k] = a_4;
                    B[j + 5][k] = a_5;
                    B[j + 6][k] = a_6;
                    B[j + 7][k] = a_7;
                }
            }
        }
    }
    else if (M == 61 && N == 67)
    {
        //simply divide into 16x16 blocks
        int i, j, k, l, tmp;
        for (i = 0; i < N; i += 17)
        {
            for (j = 0; j < M; j += 17)
            {
                for (k = i; k < i + 17 && k < N; k++)
                {
                    for (l = j; l < j + 17 && l < M; l++)
                    {
                        if (k != l)
                        {
                            B[l][k] = A[k][l];
                        }
                        else
                        {
                            tmp = A[k][l];
                        }
                    }
                    if (i == j)
                    {
                        B[k][k] = tmp;
                    }
                }
            }
        }
    }
}

/*
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started.
 */

/*
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++)
    {
        for (j = 0; j < M; j++)
        {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }
}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc);

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc);
}

/*
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++)
    {
        for (j = 0; j < M; ++j)
        {
            if (A[i][j] != B[j][i])
            {
                return 0;
            }
        }
    }
    return 1;
}
