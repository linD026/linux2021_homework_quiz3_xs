#include <stdio.h>
#include "xs.h"

// len 261
#define large_string "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"

// len 18
#define midium_string "aaaaaaaaaaaaaaaaa"

// len 9
#define small_string "aaaaaaaa"

#define NUM 1024

void test1_origin(void);
void test2_MemorySize(void);
void noraml_store(void);
void opt_store(void);

int main(int argc, char *argv[])
{
    // noraml_store();
    opt_store();
    return 0;
}
void noraml_store(void) {
    // step one, create small string
    char *small[NUM] = {0};
    for (int i = 0;i < NUM;i++) {
        small[i] = (char*)malloc(sizeof(small_string));
        memcpy(small[i], small_string, sizeof(small_string));
    }

    // step two, create midium string
    char *midium = NULL;
    midium = (char*)malloc(sizeof(midium_string));
    memcpy(midium, midium_string, sizeof(midium_string));

    // step three, create large string
    char *large = NULL;
    char *copy = NULL;
    large = (char*)malloc(sizeof(large_string));
    memcpy(large, large_string, sizeof(large_string));

    // step four, print midium string
    printf("%s\n", midium);

    // step five, copy large string
    copy = (char*)malloc(sizeof(large_string));
    memcpy(copy, large_string, sizeof(large_string));

    // step six, print small string
    printf("small %d %s\n", NUM/2, small[NUM/2]);

    // step seven, print large string
    printf("%s\n", large);
}


void opt_store(void) {
    // step one, create small string
    xs small[NUM];
    for (int i = 0;i < NUM;i++)
        xs_new(&small[i], small_string);

    // step two, create midium string
    xs midium;
    xs_new(&midium, midium_string);

    // step three, create large string
    xs large;
    xs copy;
    xs_new(&large, large_string);

    // step four, print midium string
    printf("%s\n", xs_data(&midium));

    // step five, copy large string
    xs_cow_copy(&copy, &large);

    // step six, print small string
    printf("small %d %s\n", NUM/2, xs_data(&small[NUM/2]));

    // step seven, print large string
    printf("%s\n", xs_data(&large));
}

void test2_MemorySize(void) {
        /*
    char *ptr = NULL;
    ptr = (char*)malloc(sizeof(large_string));
    memcpy(ptr, large_string, sizeof(large_string));
    char *b = NULL;
    b = (char*)malloc(sizeof(strlen(ptr) + 1));
    memcpy(b, ptr, strlen(ptr)+1 );
    */
    // test1();
    
    xs string;
    xs_new(&string, large_string);
    // xs cpy;
    // xs_cow_copy(&cpy, &string);    
    xs_reclaim_data(&string, 1);

    xs_reclaim_data(&string, 0);
}

void test1_origin(void) {
    xs string = *xs_tmp(large_string);
    xs_trim(&string, "A");
    printf("[%s] : %2zu\n", xs_data(&string), xs_size(&string));

    xs prefix = *xs_tmp("((("), suffix = *xs_tmp(")))");
    xs_concat(&string, &prefix, &suffix);
    printf("[%s] : %2zu\n", xs_data(&string), xs_size(&string));
    
    xs cow_string = *xs_tmp("aaaaaaaaaaaaaaaaaaaa");
    xs_cow_copy(&cow_string, &string);

    printf("string     [%s] : %2zu\n", xs_data(&string), xs_size(&string));
    printf("cow_string [%s] : %2zu\n", xs_data(&cow_string), xs_size(&cow_string));


    xs cp = *xs_tmp("cow front "), cs = *xs_tmp(" cow behind"); 
    xs_cow_write_concat(&cow_string, &cp, &cs, &string);
    xs_cow_write_trim(&cow_string, "a", &string);

    printf("string     [%s] : %2zu\n", xs_data(&string), xs_size(&string));
    printf("cow_string [%s] : %2zu\n", xs_data(&cow_string), xs_size(&cow_string));

    printf("copy %d\n", cow_string.sharing);
}