#include <stdio.h>

int main(int argc, char *argv[]) {    
    //int instr_types = (int)argv[1];
    bool mem_load = true;
    bool mem_store = true;
    bool cond_branch = true;
    bool uncond_branch = false;

    int mask = 0; // 0000
    int bits = 0;
    if(mem_load) {
        mask = 1 << 3;
        bits = bits | mask;       
    }
    if(mem_store) {
        mask = 1 << 2;
        bits = bits | mask;
    }
    if(cond_branch) {
        mask = 1 << 1;
        bits = bits | mask;
    }
    if(uncond_branch) {
        mask = 1 << 0;
        bits = bits | mask;
    }

    int mem_load_val = (bits & 8) >> 3;
    int mem_store_val = (bits & 4) >> 2;
    int cond_branch_val = (bits & 2) >> 1;
    int uncond_branch_val = bits & 1;

    printf("mem_load_val is %d\n", mem_load_val);
    printf("mem_store_val is %d\n", mem_store_val);
    printf("cond_branch_val is %d\n", cond_branch_val);
    printf("uncond_branch_val is %d\n", uncond_branch_val);
}