
/* 
 *  Copyright (c) 2020-2021 Xuhpclab. All rights reserved.
 *  Licensed under the MIT License.
 *  See LICENSE file for more information.
 */

/* DrCCTProf Client Sample:
 * instr_statistics_clean_call.cpp
 *
 * This sample client will display the top 200 instructions with their execution times and
 * calling paths in the output file.
 *
 * If you are new to the DynamoRIO Dynamic Binary Instrumentation (DBI) tool platform in
 * general, and DynamoRIO in particular, ensure you understand the method by which
 * instrumentation is added to application code.
 *
 * Remember that instrumentation occurs in two phases, transformation and execution:
 * - Transformation
 *      Instrumentation code is inserted into the application code.
 * - Execution
 *      The application code runs, including the instrumentation code which was inserted
 *      during transformation.
 */

#include <iterator>
#include <vector>
#include <map>

#include "dr_api.h"
#include "drcctlib.h"

#define DRCCTLIB_PRINTF(_FORMAT, _ARGS...) \
    DRCCTLIB_PRINTF_TEMPLATE("instr_statistics_clean_call", _FORMAT, ##_ARGS) // Make DRCCTLIB_PRINTF alias for DRCCTLIB_PRINTF_TEMPLATE
#define DRCCTLIB_EXIT_PROCESS(_FORMAT, _ARGS...) \
    DRCCTLIB_CLIENT_EXIT_PROCESS_TEMPLATE("instr_statistics_clean_call", _FORMAT, ##_ARGS)

#ifdef ARM_CCTLIB
#    define OPND_CREATE_CCT_INT OPND_CREATE_INT
#else
#    define OPND_CREATE_CCT_INT OPND_CREATE_INT32
#endif

#define MAX_CLIENT_CCT_PRINT_DEPTH 10
#define TOP_REACH_NUM_SHOW 200

uint64_t *gloabl_hndl_call_num;
static file_t gTraceFile;

using namespace std;

// Execution
void
InsCount(int32_t slot)
{
    void *drcontext = dr_get_current_drcontext();
    context_handle_t cur_ctxt_hndl = drcctlib_get_context_handle(drcontext, slot);
    gloabl_hndl_call_num[cur_ctxt_hndl]++; // cur_ctxt_hndl represents the instruction and call path associated with it?
}

// Transformation
void
InsTransEventCallback(void *drcontext, instr_instrument_msg_t *instrument_msg)
{

    instrlist_t *bb = instrument_msg->bb;
    instr_t *instr = instrument_msg->instr;
    int32_t slot = instrument_msg->slot;

    dr_insert_clean_call(drcontext, bb, instr, (void *)InsCount, false, 1, OPND_CREATE_CCT_INT(slot));
}

static inline void
InitGlobalBuff()
{
    gloabl_hndl_call_num = (uint64_t *)dr_raw_mem_alloc(
        CONTEXT_HANDLE_MAX * sizeof(uint64_t), DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
    if (gloabl_hndl_call_num == NULL) {
        DRCCTLIB_EXIT_PROCESS(
            "init_global_buff error: dr_raw_mem_alloc fail gloabl_hndl_call_num");
    }
}

static inline void
FreeGlobalBuff()
{
    dr_raw_mem_free(gloabl_hndl_call_num, CONTEXT_HANDLE_MAX * sizeof(uint64_t));
}

// register dynamoRIO extensions
static void
ClientInit(int argc, const char *argv[])
{
    char name[MAXIMUM_FILEPATH] = "";
    DRCCTLIB_INIT_LOG_FILE_NAME(
        name, "instr_statistics_clean_call", "out");
    DRCCTLIB_PRINTF("Creating log file at:%s", name);

    gTraceFile = dr_open_file(name, DR_FILE_WRITE_OVERWRITE | DR_FILE_ALLOW_LARGE);
    DR_ASSERT(gTraceFile != INVALID_FILE);

    InitGlobalBuff();
    // InsTransEventCallback runs on each instruction in the test program
    drcctlib_init(DRCCTLIB_FILTER_ALL_INSTR, INVALID_FILE, InsTransEventCallback, false);
}

typedef struct _output_format_t {
    context_handle_t handle;
    uint64_t count;
} output_format_t;

// dynamoRIO calls this
static void
ClientExit(void)
{
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
        dr_fprintf(gTraceFile, "NO. %d PC ", i + 1);
        drcctlib_print_backtrace_first_item(gTraceFile, output_list[i].handle, true, false);
        dr_fprintf(gTraceFile, "=>EXECUTION TIMES\n%lld\n=>BACKTRACE\n",
                   output_list[i].count);
        drcctlib_print_backtrace(gTraceFile, output_list[i].handle, true, true, -1);
        dr_fprintf(gTraceFile, "\n\n\n");
        
        
    }
    dr_global_free(output_list, TOP_REACH_NUM_SHOW * sizeof(output_format_t));
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
    dr_set_client_name("DynamoRIO Client 'instr_statistics_clean_call'",
                       "http://dynamorio.org/issues");

    ClientInit(argc, argv);
    dr_register_exit_event(ClientExit);
}

#ifdef __cplusplus
}
#endif