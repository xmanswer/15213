/* Min Xu
 * andrewID: minxu
 * email: minxu@andrew.cmu.edu
 * This program is used to transpose matrices with different sizes
 * with minimum misses in the cache*/

/* 
 nnieliu* trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"
#include "contracts.h"

#define BSQUARE 8
#define BSUB 4
#define B_61 18

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. The REQUIRES and ENSURES from 15-122 are included
 *     for your convenience. They can be removed if you like.
 */



char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    REQUIRES(M > 0);
    REQUIRES(N > 0);

    ENSURES(is_transpose(M, N, A, B));
    /*index for row and col of ints*/
    int row = 0, col = 0;
    /*index for row and col of blocks*/ 
    int bRow = 0, bCol = 0;
    /*local variables for caching*/
    int temp1, temp2, temp3, temp4, temp5, temp6, temp7, temp8;
    /*different blocking methods for different matrices */ 
    if(M == 32) { 
    /*for 32x32, divide into 16 8x8 blocks*/
        for(bCol = 0; bCol < M; bCol += BSQUARE) {
	    for(bRow = 0; bRow < N; bRow += BSQUARE){
	        for(row = bRow; row < bRow + BSQUARE; row += 1){
    /*use 8 local variables to transpose one row to one column*/
		    temp1 = A[row][bCol];
	            temp2 = A[row][bCol+1];
	            temp3 = A[row][bCol+2];
		    temp4 = A[row][bCol+3];
	            temp5 = A[row][bCol+4];
		    temp6 = A[row][bCol+5];
		    temp7 = A[row][bCol+6];
		    temp8 = A[row][bCol+7];
                    B[bCol][row] = temp1;
		    B[bCol+1][row] = temp2;
		    B[bCol+2][row] = temp3;
		    B[bCol+3][row] = temp4;
		    B[bCol+4][row] = temp5;
		    B[bCol+5][row] = temp6;
		    B[bCol+6][row] = temp7;
                    B[bCol+7][row] = temp8;
                }   
            }
        }
    }
    else if (M == 64) {  
    /* for 64x64, divide into 8x8 blocks and further divide into 4x4 blocks
     * idea is to read 8 items in a row of A at a time, but only store 4 
     * lines in B to avoid conflict misses, so need parts of B as a cache*/
        for(bCol = 0; bCol < M; bCol += BSQUARE) {
	    for(bRow = 0; bRow < N; bRow += BSQUARE){
	        for(row = bRow; row < bRow + BSUB; row++) {
		    temp1 = A[row][bCol];
		    temp2 = A[row][bCol+1];
		    temp3 = A[row][bCol+2];
		    temp4 = A[row][bCol+3]; 
		    temp5 = A[row][bCol+4];
		    temp6 = A[row][bCol+5];
		    temp7 = A[row][bCol+6];
                    temp8 = A[row][bCol+7]; 
	            /*regular transpose for left up A to left up B*/
		    B[bCol][row] = temp1;
		    B[bCol+1][row] = temp2;
		    B[bCol+2][row] = temp3;
                    B[bCol+3][row] = temp4;
		    /*left up of A to right up of B*/ 
		    B[bCol][row+4] = temp5;
		    B[bCol+1][row+4] = temp6;
		    B[bCol+2][row+4] = temp7;
                    B[bCol+3][row+4] = temp8;
	        } 
	       for(col = bCol; col < bCol + BSUB; col++) {
	           temp1 = B[col][bRow+4];
		   temp2 = B[col][bRow+5];
                   temp3 = B[col][bRow+6];
		   temp4 = B[col][bRow+7]; 
	           /*left down of A to righ up of B*/
		   for(row = bRow; row < bRow + BSUB; row++) {
		       B[col][row+4] = A[row+4][col];
		   }
	           /*right up of B to left down of B*/
                   B[col+4][bRow] = temp1;
		   B[col+4][bRow+1] = temp2;
		   B[col+4][bRow+2] = temp3;
		   B[col+4][bRow+3] = temp4;
	       }
	      /*regular transpose for right down of A to right down of B*/ 
               for(col = bCol + BSUB; col < bCol + 2*BSUB; col++){
	           temp1 = A[bRow+4][col];
		   temp2 = A[bRow+5][col];
		   temp3 = A[bRow+6][col];
		   temp4 = A[bRow+7][col];
		   B[col][bRow+4] = temp1;
		   B[col][bRow+5] = temp2;
		   B[col][bRow+6] = temp3;
		   B[col][bRow+7] = temp4;
	      }
            }
        }
    }
    else {
    /*for 67x61, divide into 18x18 blocks*/
        for(bCol = 0; bCol < M; bCol += B_61) {
	    for(bRow = 0; bRow < N; bRow += B_61) {
    /*set another boundary for row and col index 
      sweep due to asymmetric matrix*/
	        for(row = bRow; (row < bRow + B_61) && (row < N); row++) {
		    for(col = bCol; (col < bCol+B_61) && (col < M); col++) {
		        if(col != row) {
			    B[col][row] = A[row][col];	
		        }   
    /*use local variable to cache diagonal items*/
			else {
		            temp1 = A[row][col];
			    temp2 = row;
		        }
		    }
    /*effectively reduce the unneccessary misses of diagonal items*/
	            if(row == temp2){
	                B[row][row] = temp1;
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

    REQUIRES(M > 0);
    REQUIRES(N > 0);

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

    ENSURES(is_transpose(M, N, A, B));
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

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

