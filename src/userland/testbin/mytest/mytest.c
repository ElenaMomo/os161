#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>

int main(){

    printf("hahahahahahahah\n");

    int i = fork();
    if(i){
        printf("parent\n");
    }
    else{
        printf("child\n");
    }
}
