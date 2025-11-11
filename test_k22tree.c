#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <linux/k22info.h>

//Find_depth function takes as arguments the process array (buf) , the array depths and the process number that we want to find it's depth
void find_depth(struct k22info * buf , int * depths , int proc_number){
    int i ; 
    for(i = 0 ; buf[proc_number].parent_pid != buf[i].pid ; i++);//We search the buf array until we find the parent process of proc_number. (The buf array has the processes stored in DFS order)
    depths[proc_number] = depths[i] + 1; //The depth of the process will be the depth of it's parent plus 1 
}

int main(void){

    int pnum , buf_size , ne ; 

    buf_size = ne = 100 ; //User space buffer starts with normal size 100
    struct k22info * buf = malloc(buf_size* sizeof(struct k22info));
    if(!buf){
        fprintf(stderr , "Memory allocation failed\n");
        return 1 ; 
    }
 
    //Call iteratively the k22tree syscall until all processes fit in the buffer
    do{
        pnum = syscall(467 , buf , &ne); //Invoking the syscall and passing the right arguments
        if(pnum < 0){ //Check if an error occured in the syscall 
            free(buf);
            fprintf(stderr , "Syscall Error: %d \n" , errno); //Errno is a negative number that represents the type of the error 
            return 1;
        }
        printf("- User-space buf. size: %d\n" , buf_size);
        printf("- syscall return val: %d\n" , pnum);

        if(pnum > buf_size){ //If all the processes didn't fit in the buffer , then change the size of buf and recall the syscall
            buf_size = ne = pnum + 10 ; //The new buffer size will be the number of processes the syscall found plus 10 because the next time there might be more processes
            free(buf);
            buf = malloc(buf_size* sizeof(struct k22info));
            if(!buf){
                fprintf(stderr , "Reallocation Failed\n");
                return 1;
            }
        }
        else{
            break ; 
        }
    }while(1);

    printf("--- OK ---\n\n#comm,pid,ppid,fcldpid,nsblpid,nvcsw,nivcsw,stime\n%s,%d,%d,%d,%d,%ld,%ld,%ld\n" , buf[0].comm , buf[0].pid , buf[0].parent_pid , buf[0].first_child_pid , buf[0].next_sibling_pid , buf[0].nvcsw , buf[0].nivcsw , buf[0].start_time);

    int * depths = malloc(ne*sizeof(int));//This array will hold the depth of each process in the process tree 
    if(!depths){
        free(buf);
        fprintf(stderr , "Memory allocation failed\n");
        return 1 ; 
    }

    depths[0] = 0 ; 

    for(int i = 1 ; i < ne  ; i++){//For each process (i) find it's depth and print the analogous dashes 
        find_depth(buf , depths , i);
        for(int j = 0 ; j < depths[i] ; j++){
            printf("-");
        }
        //Print the information of the current process
        printf("%s,%d,%d,%d,%d,%ld,%ld,%ld\n" , buf[i].comm , buf[i].pid , buf[i].parent_pid , buf[i].first_child_pid , buf[i].next_sibling_pid , buf[i].nvcsw , buf[i].nivcsw , buf[i].start_time);
    }

    free(depths);
    free(buf);
    return 0 ;
}