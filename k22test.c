#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/k22info.h>

#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"

void find_depth(struct k22info * buf , int * depths , int proc_number){
    int i ; 
    for(i = 0 ; buf[proc_number].parent_pid != buf[i].pid ; i++){
    }
    depths[proc_number] = depths[i] + 1;
}

int main(void){

    int pnum , buf_size , ne ; 

    buf_size = ne = 100 ; 
    struct k22info * buf = malloc(100* sizeof(struct k22info));
    if(!buf){
        fprintf(stderr , "Memory allocation failed\n");
        return 1 ; 
    }

    do{
        pnum = syscall(467 , buf , &ne);
        if(pnum < 0){
            free(buf);
            fprintf(stderr , "Syscall Error: %d \n" , pnum);
            return 1;
        }
        printf("- User-space buf. size: %d\n" , buf_size);
        printf("- syscall"ANSI_COLOR_GREEN" return"ANSI_COLOR_RESET" val: %d\n" , pnum);

        if(pnum > buf_size){
            buf_size = ne = pnum + 10 ; 
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

    printf(ANSI_COLOR_GREEN "---" ANSI_COLOR_RESET" OK"ANSI_COLOR_GREEN" ---"ANSI_COLOR_RESET"\n\n"ANSI_COLOR_MAGENTA"#comm,pid,ppid,fcldpid,nsblpid,nvcsw,nivcsw,stime"ANSI_COLOR_RESET"\n%s,%d,%d,%d,%d,%ld,%ld,%ld\n" , buf[0].comm , buf[0].pid , buf[0].parent_pid , buf[0].first_child_pid , buf[0].next_sibling_pid , buf[0].nvcsw , buf[0].nivcsw , buf[0].start_time);

    int * depths = malloc(ne*sizeof(int));
    if(!depths){
        free(buf);
        fprintf(stderr , "Memory allocation failed\n");
        return 1 ; 
    }

    struct k22info * proc ;
    depths[0] = 0 ; 

    for(int i = 1 ; i < pnum  ; i++){
        find_depth(buf , depths , i);
        for(int j = 0 ; j < depths[i] ; j++){
            printf(ANSI_COLOR_GREEN "-");
        }
        printf("%s"ANSI_COLOR_RESET",%d,%d,%d,%d,%ld,%ld,%ld\n" , buf[i].comm , buf[i].pid , buf[i].parent_pid , buf[i].first_child_pid , buf[i].next_sibling_pid , buf[i].nvcsw , buf[i].nivcsw , buf[i].start_time);
    }

    free(depths);
    free(buf);
    return 0 ;
}