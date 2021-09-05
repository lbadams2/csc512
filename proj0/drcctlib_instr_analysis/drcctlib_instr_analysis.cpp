#include <iterator>
#include <vector>
#include <map>

#include "dr_api.h"
#include "drcctlib.h"

#define DRCCTLIB_PRINTF(_FORMAT, _ARGS...) \
    DRCCTLIB_PRINTF_TEMPLATE("instr_analysis", _FORMAT, ##_ARGS)
#define DRCCTLIB_EXIT_PROCESS(_FORMAT, _ARGS...) \
    DRCCTLIB_CLIENT_EXIT_PROCESS_TEMPLATE("instr_analysis", _FORMAT, ##_ARGS)

#ifdef ARM_CCTLIB
#    define OPND_CREATE_CCT_INT OPND_CREATE_INT
#else
#    define OPND_CREATE_CCT_INT OPND_CREATE_INT32
#endif

#define MAX_CLIENT_CCT_PRINT_DEPTH 10
#define TOP_REACH_NUM_SHOW 10
#define MEM_LOAD_PROJ0 0
#define MEM_STORE_PROJ0 1
#define COND_BRANCH_PROJ0 2
#define UNCOND_BRANCH_PROJ0 3

static uint64_t mem_load = 0;
static uint64_t mem_store = 0;
static uint64_t cond_branch = 0;
static uint64_t uncond_branch = 0;

uint64_t *gloabl_hndl_call_num_load;
uint64_t *gloabl_hndl_call_num_store;
uint64_t *gloabl_hndl_call_num_cond;
uint64_t *gloabl_hndl_call_num_uncond;

static file_t gTraceFile;

using namespace std;

// Execution
void
InsCount(int32_t instr_type, int32_t slot)
{
    void *drcontext = dr_get_current_drcontext();
    context_handle_t cur_ctxt_hndl = drcctlib_get_context_handle(drcontext, slot);
    if(instr_type == MEM_LOAD_PROJ0) {
        mem_load++;
        gloabl_hndl_call_num_load[cur_ctxt_hndl]++;
    }
    else if(instr_type == MEM_STORE_PROJ0) {
        mem_store++;
        gloabl_hndl_call_num_store[cur_ctxt_hndl]++;
    }
    else if(instr_type == COND_BRANCH_PROJ0) {
        cond_branch++;
        gloabl_hndl_call_num_cond[cur_ctxt_hndl]++;
    }
    else if(instr_type == UNCOND_BRANCH_PROJ0) {
        uncond_branch++;
        gloabl_hndl_call_num_uncond[cur_ctxt_hndl]++;
    }    
}

// Transformation
void
InsTransEventCallback(void *drcontext, instr_instrument_msg_t *instrument_msg)
{

    instrlist_t *bb = instrument_msg->bb;
    instr_t *instr = instrument_msg->instr;
    int32_t slot = instrument_msg->slot;

    int32_t instr_type = 0;
    if(instr_reads_memory(instr))
        instr_type = MEM_LOAD_PROJ0;
    else if(instr_writes_memory(instr))
        instr_type = MEM_STORE_PROJ0;
    else if(instr_is_cbr(instr))
        instr_type = COND_BRANCH_PROJ0;
    else if(instr_is_ubr(instr))
        instr_type = UNCOND_BRANCH_PROJ0;

    dr_insert_clean_call(drcontext, bb, instr, (void *)InsCount, false, 2, OPND_CREATE_INT32(instr_type), OPND_CREATE_CCT_INT(slot));
}

static inline void
InitGlobalBuff()
{
    gloabl_hndl_call_num_uncond = (uint64_t *)dr_raw_mem_alloc(
        CONTEXT_HANDLE_MAX * sizeof(uint64_t), DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
    if (gloabl_hndl_call_num_uncond == NULL) {
        DRCCTLIB_EXIT_PROCESS(
            "init_global_buff error: dr_raw_mem_alloc fail gloabl_hndl_call_num_uncond");
    }

    gloabl_hndl_call_num_cond = (uint64_t *)dr_raw_mem_alloc(
        CONTEXT_HANDLE_MAX * sizeof(uint64_t), DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
    if (gloabl_hndl_call_num_cond == NULL) {
        DRCCTLIB_EXIT_PROCESS(
            "init_global_buff error: dr_raw_mem_alloc fail gloabl_hndl_call_num_cond");
    }

    gloabl_hndl_call_num_load = (uint64_t *)dr_raw_mem_alloc(
        CONTEXT_HANDLE_MAX * sizeof(uint64_t), DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
    if (gloabl_hndl_call_num_load == NULL) {
        DRCCTLIB_EXIT_PROCESS(
            "init_global_buff error: dr_raw_mem_alloc fail gloabl_hndl_call_num_load");
    }

    gloabl_hndl_call_num_store = (uint64_t *)dr_raw_mem_alloc(
        CONTEXT_HANDLE_MAX * sizeof(uint64_t), DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
    if (gloabl_hndl_call_num_store == NULL) {
        DRCCTLIB_EXIT_PROCESS(
            "init_global_buff error: dr_raw_mem_alloc fail gloabl_hndl_call_num_store");
    }
}

static inline void
FreeGlobalBuff()
{
    dr_raw_mem_free(gloabl_hndl_call_num_uncond, CONTEXT_HANDLE_MAX * sizeof(uint64_t));
    dr_raw_mem_free(gloabl_hndl_call_num_cond, CONTEXT_HANDLE_MAX * sizeof(uint64_t));
    dr_raw_mem_free(gloabl_hndl_call_num_load, CONTEXT_HANDLE_MAX * sizeof(uint64_t));
    dr_raw_mem_free(gloabl_hndl_call_num_store, CONTEXT_HANDLE_MAX * sizeof(uint64_t));
}

static void
ClientInit(int argc, const char *argv[])
{
    char name[MAXIMUM_FILEPATH] = "";
    DRCCTLIB_INIT_LOG_FILE_NAME(
        name, "instr_analysis", "out");
    DRCCTLIB_PRINTF("Creating log file at:%s", name);

    gTraceFile = dr_open_file(name, DR_FILE_WRITE_OVERWRITE | DR_FILE_ALLOW_LARGE);
    DR_ASSERT(gTraceFile != INVALID_FILE);

    InitGlobalBuff();
    drcctlib_init(DRCCTLIB_FILTER_ALL_INSTR, INVALID_FILE, InsTransEventCallback, false);
}

typedef struct _output_format_t {
    context_handle_t handle;
    uint64_t count;
} output_format_t;

static void
print_calling_context(int16_t instr_type) {
    uint64_t *gloabl_hndl_call_num;
    
    if(instr_type == MEM_LOAD_PROJ0) {
        gloabl_hndl_call_num = gloabl_hndl_call_num_load;
        dr_fprintf(gTraceFile, "MEMORY LOAD : %d\n", mem_load);
    }
    else if(instr_type == MEM_STORE_PROJ0) {
        gloabl_hndl_call_num = gloabl_hndl_call_num_store;
        dr_fprintf(gTraceFile, "MEMORY STORE : %d\n", mem_store);
    }
    else if(instr_type == COND_BRANCH_PROJ0) {
        gloabl_hndl_call_num = gloabl_hndl_call_num_cond;
        dr_fprintf(gTraceFile, "CONDITIONAL BRANCHES : %d\n", cond_branch);
    }
    else if(instr_type == UNCOND_BRANCH_PROJ0) {
        gloabl_hndl_call_num = gloabl_hndl_call_num_uncond;
        dr_fprintf(gTraceFile, "UNCONDITIONAL BRANCHES : %d\n", uncond_branch);
    }

    output_format_t *output_list =
        (output_format_t *)dr_global_alloc(TOP_REACH_NUM_SHOW * sizeof(output_format_t));
    for (int32_t i = 0; i < TOP_REACH_NUM_SHOW; i++) {
        output_list[i].handle = 0; // this is index of gloabl_hndl_call_num
        output_list[i].count = 0; // this is value of gloabl_hndl_call_num[i]
    }
    context_handle_t max_ctxt_hndl = drcctlib_get_global_context_handle_num(); // get number of contexts in gloabl_hndl_call_num
    // i think a context is an instruction plus its call path

    for (context_handle_t i = 0; i < max_ctxt_hndl; i++) {
        if (gloabl_hndl_call_num[i] <= 0) {
            continue;
        }
        if (gloabl_hndl_call_num[i] > output_list[0].count) { // output_list initialized to zeroes
            uint64_t min_count = gloabl_hndl_call_num[i];
            int32_t min_idx = 0;
            // i is index for gloabl_hndl_call_num, j is index for output_list
            for (int32_t j = 1; j < TOP_REACH_NUM_SHOW; j++) { // TOP_REACH_NUM_SHOW is size of output_list, output_list[0] has least called instr
                if (output_list[j].count < min_count) {
                    min_count = output_list[j].count;
                    min_idx = j;
                }
            }

            // if gloabl_hndl_call_num > output_list[0].count, iterate all output_list.count and find smallest output_list.count
            // overwrite output_list[0] with smallest count
            output_list[0].count = min_count;
            output_list[0].handle = output_list[min_idx].handle;
            // min_idx was just moved to 0, overwrite min_idx with gloabl_hndl_call_num[i]
            // i think this is done because there is limited amount of space in output_list
            output_list[min_idx].count = gloabl_hndl_call_num[i];
            output_list[min_idx].handle = i;
        }
    }

    // sort
    output_format_t temp;
    for (int32_t i = 0; i < TOP_REACH_NUM_SHOW; i++) {
        for (int32_t j = i; j < TOP_REACH_NUM_SHOW; j++) {
            if (output_list[i].count < output_list[j].count) {
                temp = output_list[i];
                output_list[i] = output_list[j];
                output_list[j] = temp;
            }
        }
    }

    // print output
    for (int32_t i = 0; i < TOP_REACH_NUM_SHOW; i++) {
        if (output_list[i].handle == 0) {
            break;
        }
        dr_fprintf(gTraceFile, "[NO. %d]", i + 1);
        dr_fprintf(gTraceFile, "Ins call times %lld\n",
                   output_list[i].count);
        dr_fprintf(gTraceFile, "================================================================================\n");
        drcctlib_print_backtrace(gTraceFile, output_list[i].handle, true, true, -1);
        dr_fprintf(gTraceFile, "================================================================================\n\n");
        
        
    }
    dr_global_free(output_list, TOP_REACH_NUM_SHOW * sizeof(output_format_t));
}

static void
ClientExit(void)
{
    for(int i = 0; i < 4; i++)
        print_calling_context(i);

    FreeGlobalBuff();
    drcctlib_exit();

    dr_close_file(gTraceFile);
}

#ifdef __cplusplus
extern "C" {
#endif

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    dr_set_client_name("DynamoRIO Client 'instr_analysis'",
                       "http://dynamorio.org/issues");

    ClientInit(argc, argv);
    dr_register_exit_event(ClientExit);
}

#ifdef __cplusplus
}
#endif