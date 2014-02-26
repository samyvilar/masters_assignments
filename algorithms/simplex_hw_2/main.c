/* 
 * File:   main.c
 * Author: Samy Vilar
 *
 * Created on October 21, 2011, 11:36 PM
 */

#include <stdio.h>
#include <stdlib.h>

/* Use Bland's rule, return index of first negative value, in order to avoid cycles ... */
int get_pivot_col_index(double *values, int max)
{
    int index = 0;                 
    for (; index < max; index++)
        if (values[index] < 0)
            return index;
    
    return -1;
}

/* Blands rule ...  */
int get_pivot_row_index(double **table, int pivot_col, int numb_rows, int numb_cols)
{   
    int min_index = 0, index = 0; double min;
    for (; index < numb_rows; index++)
        if (table[index][pivot_col] > 0)
        {
           min = table[index][numb_cols - 1] / table[index][pivot_col]; 
           min_index = index;
           break ;
        }        
    
    for (; index < numb_rows; index++)        
        if (table[index][pivot_col] > 0 && (min > (table[index][numb_cols - 1] / table[index][pivot_col])))
        {
            min = table[index][numb_cols - 1] / table[index][pivot_col];
            min_index = index;
        }
    
    return min_index;
}

void print_table(double **table, int row_max, int col_max)
{
    int row = 0, col = 0;
    for (; row < row_max; row++)
    {
        for (col = 0; col < col_max; col++)
            printf("%f ", table[row][col]);
        printf("\n");
    }    
}

int simplex(int         numb_var,       /* number of variables                                   */
            int         numb_ineq,      /* number of inequalities                                */ 
            double      *ineq_coef_mtrx,/* matrix containing coefficient of the inequalities     */
            double      *ineq_rhs,      /* right hand hand side of the inequalities              */
            double      *obj_func_coef, /* coefficients of the objective function                */
            double      *results        /* result vector contains optimum values                 */
            )
{
    int numb_rows = numb_ineq + 1, numb_cols = numb_var + numb_ineq + 1, count = 0, index = 0, row_index = 0, col_index = 0;
    double **table = (double **)malloc(sizeof(double *) * numb_rows);
    for (index = 0; index < numb_rows; index++)
        table[index] = (double *)malloc(sizeof(double) * numb_cols);        
    
    int loc = numb_var + 1; 
    for (row_index = 0; row_index < numb_ineq; row_index++)
    {
        for (col_index = 0; col_index < numb_var; col_index++)                   
            table[row_index][col_index] = *(ineq_coef_mtrx + numb_var * row_index + col_index);                                
                
        table[row_index][loc++] = 1;
        table[row_index][numb_cols - 1] = ineq_rhs[row_index];
    }
    
    for (index = 0; index < numb_var; index++)
        table[numb_rows - 1][index] = -1 * obj_func_coef[index];       
        
    int pivot_col_index, pivot_row_index;
    double pivot_value, factor;                
    
    while ((pivot_col_index = get_pivot_col_index(table[numb_rows - 1], numb_cols)) != -1)
    {               
        pivot_row_index = get_pivot_row_index(table, pivot_col_index, numb_rows, numb_cols);        
        pivot_value     = table[pivot_row_index][pivot_col_index];                
        
        for (index = 0; index < numb_cols; index++)
            if (table[pivot_row_index][index] != 0)
                table[pivot_row_index][index] /= pivot_value;
                
        for (row_index = 0; row_index < numb_rows; row_index++)
        {
            factor = table[row_index][pivot_col_index] / table[pivot_row_index][pivot_col_index];
            for (col_index = 0; col_index < numb_cols; col_index++)
                if (row_index != pivot_row_index)
                    table[row_index][col_index] -= factor * table[pivot_row_index][col_index];
        }
        
        count++;       
    }
    
    for (index = 0; index < numb_var; index++)
        if (table[numb_rows - 1][index] != 0)
            results[index] = 0;
        else
            for (row_index = 0; row_index < numb_rows; row_index++)
                if (table[row_index][index] != 0)
                {
                    results[index] = table[row_index][numb_cols - 1];
                    break ;
                }
    
    return count;
}


int main(int argc, char** argv) 
{
    
   double A1[40][2], A2[12][10], b1[40], b2[12],c1[2],c2[10];
   double result1[2], result2[10];
   int i, j; double s;
   printf("Preparing test1: two variables, many inequalities\n");
   i=0;
   for( s=0.05; s <= 1.001; s+=0.05 )
   {  A1[i][0] = (1.0/s); A1[i][1] = (s-1.0)/(s*s); b1[i]=1.0; i+=1; 
      A1[i][1] = (1.0/s); A1[i][0] = (s-1.0)/(s*s); b1[i]=1.0; i+=1; 
   }
   c1[0]=1.0; c1[1]=1.0;
   printf("Running test1: %d  inequalities\n", i);
   j = simplex(2,40, &(A1[0][0]), &(b1[0]), &(c1[0]), &(result1[0]));
   printf("Test1: extremal point (%f, %f) after %d steps\n", 
          result1[0], result1[1], j);
   printf("Preparing test2: ten variables, twelve inequalities\n");
   for( i=0; i<12; i++ )
     for (j=0; j<10; j++ )
       A2[i][j]=0.0;
   for( i=0; i<10; i++ )
   {  A2[i][i] =1.0;
      b2[i]=1.0;
   }
   for( i=0; i<10; i++ )
   {  A2[10][i] =1.0; A2[11][i] =0.0;
   }
   b2[10] = 5.0;
   for( i=6; i<10; i++ )
   {  A2[11][i] = i;
   }
   b2[11] =3.0;
   for( i=0; i<10; i++ )
     c2[i] = 11.0-i;
   printf("Running test2\n");
   j = simplex(10,12, &(A2[0][0]), &(b2[0]), &(c2[0]), &(result2[0]));
   printf("Test2: extremal point \n");
   printf("%f, %f, %f, %f, %f,\n %f, %f, %f, %f, %f\n",
          result2[0], result2[1], result2[2], result2[3], result2[4],
          result2[5], result2[6], result2[7], result2[8], result2[9]);
   printf("found after %d steps\n", j);
   
   return 0;
}
