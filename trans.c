#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);


char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    int i = 0;
    int j = 0;
    int k = 0;
    int l = 0;
    int tmp1 = 0;
    int tmp2 = 0;
    int tmp3 = 0;
    int tmp4 = 0;
    int tmp5 = 0;
    int tmp6 = 0;
    int tmp7 = 0;
    int tmp8 = 0;
    //for 32 by 32 matrix, use block_size of 8
    if(M == 32 && N == 32) {
        for (i = 0; i < N; i += 8) {
            for (j = 0; j < M; j += 8) {
                for (k = i; k < i + 8; k++) {
                    for (l = j; l < j + 8; l++) {
                        if (k != l) {
                            B[l][k] = A[k][l];
                        }
                    }
                    //when diagonal
                    if (i == j) {
                        B[k][k] = A[k][k];
                    }
                }
            }
        }
    }
    //for matrix 64 by 64, use block_size of 8 and inner_block_size of 4
    else if(M == 64 && N == 64){
        for (i = 0; i < N; i += 8) {
            for (j = 0; j < M; j += 8) {
                for (k = i; k < i + 4; k++) {
                    //use tmp variables to store values from A to reduce cache miss and cache thrashing
                    tmp1 = A[k][j];
                    tmp2 = A[k][j+1];
                    tmp3 = A[k][j+2];
                    tmp4 = A[k][j+3];
                    tmp5 = A[k][j+4];
                    tmp6 = A[k][j+5];
                    tmp7 = A[k][j+6];
                    tmp8 = A[k][j+7];
                    B[j][k] = tmp1;
                    B[j+1][k] = tmp2;
                    B[j+2][k] = tmp3;
                    B[j+3][k] = tmp4;
                    B[j][k+4] = tmp5;
                    B[j+1][k+4] = tmp6;
                    B[j+2][k+4] = tmp7;
                    B[j+3][k+4] = tmp8;
                }
                for(l = j; l < j + 4; l++){
                    tmp1 = A[i+4][l];
                    tmp2 = A[i+5][l];
                    tmp3 = A[i+6][l];
                    tmp4 = A[i+7][l];
                    tmp5 = B[l][i+4];
                    tmp6 = B[l][i+5];
                    tmp7 = B[l][i+6];
                    tmp8 = B[l][i+7];
                    B[l][i+4] = tmp1;
                    B[l][i+5] = tmp2;
                    B[l][i+6] = tmp3;
                    B[l][i+7] = tmp4;
                    B[l+4][i] = tmp5;
                    B[l+4][i+1] = tmp6;
                    B[l+4][i+2] = tmp7;
                    B[l+4][i+3] = tmp8;
                    B[l+4][i+4] = A[i+4][l+4];
                    B[l+4][i+5] = A[i+5][l+4];
                    B[l+4][i+6] = A[i+6][l+4];
                    B[l+4][i+7] = A[i+7][l+4]; 
                }
            }
        }
    }
    //for 61 by 67 matrix, experiement with different block_size until condition is met
    else if(M==61 && N ==67){
        for (i = 0; i < N; i += 20) {
            for (j = 0; j < M; j += 20) {
                for (k = i; k < i + 20; k++) {
                    for (l = j; l < j + 20; l++) {
                        if(k < N && l < M){
                            tmp1 = A[k][l];
                            B[l][k] = tmp1;
                        }                      
                    }
                }
            }
        }
    }
}


char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

}

void registerFunctions()
{
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    registerTransFunction(trans, trans_desc); 

}


int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}


