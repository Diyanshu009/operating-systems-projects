
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define COUNT 35000
#define LOW 1
#define HIGH 50000

void create_data(int *arr, int n) {
    for(int i=0;i<n;i++)
        arr[i] = rand()%(HIGH-LOW+1)+LOW;
}

void bubble(int *arr, int n) {
    for(int i=0;i<n-1;i++)
        for(int j=0;j<n-i-1;j++)
            if(arr[j] > arr[j+1]) {
                int t = arr[j];
                arr[j] = arr[j+1];
                arr[j+1] = t;
            }
}

void swap(int *a,int *b){
    int t=*a; *a=*b; *b=t;
}

int partition(int arr[], int low, int high){
    int pivot = arr[low];
    int i = low + 1;
    for(int j=low+1;j<=high;j++){
        if(arr[j] < pivot){
            swap(&arr[i],&arr[j]);
            i++;
        }
    }
    swap(&arr[low],&arr[i-1]);
    return i-1;
}

void quicksort(int arr[], int low, int high){
    if(low<high){
        int p = partition(arr,low,high);
        quicksort(arr,low,p-1);
        quicksort(arr,p+1,high);
    }
}

int main(){

    srand(time(NULL));

    int *a = malloc(COUNT*sizeof(int));
    int *b = malloc(COUNT*sizeof(int));

    double Tu, Tpu, To, Tpo;

    clock_t t1 = clock();
    create_data(a,COUNT);

    clock_t p1 = clock();
    bubble(a,COUNT);
    clock_t p2 = clock();
    clock_t t2 = clock();

    Tu = (double)(t2-t1)/CLOCKS_PER_SEC;
    Tpu = (double)(p2-p1)/CLOCKS_PER_SEC;

    clock_t t3 = clock();
    create_data(b,COUNT);

    clock_t p3 = clock();
    quicksort(b,0,COUNT-1);
    clock_t p4 = clock();
    clock_t t4 = clock();

    To = (double)(t4-t3)/CLOCKS_PER_SEC;
    Tpo = (double)(p4-p3)/CLOCKS_PER_SEC;

    double P = Tpu/Tu;
    double S = Tpu/Tpo;
    double measured = Tu/To;
    double theoretical = 1.0/((1-P)+(P/S));

    printf("\nUnoptimized Total: %.4f\n",Tu);
    printf("Unoptimized Processing: %.4f\n",Tpu);
    printf("Optimized Total: %.4f\n",To);
    printf("Optimized Processing: %.4f\n",Tpo);

    printf("\nP: %.4f\n",P);
    printf("S: %.4f\n",S);
    printf("Measured Speedup: %.4f\n",measured);
    printf("Theoretical Speedup: %.4f\n",theoretical);

    free(a);
    free(b);

    return 0;
}
