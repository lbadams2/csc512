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
#define TOP_REACH_NUM_SHOW 200

static uint64_t mem_load = 0;
static uint64_t mem_store = 0;
static uint64_t cond_branch = 0;
static uint64_t uncond_branch = 0;
static file_t gTraceFile;

using namespace std;

// Execution
void
InsCount(instr_t *instr)
{
    if(instr_is_ubr(instr))
        uncond_branch++;
    else if(instr_is_ubr(instr))
        cond_branch++;
    else if(instr_writes_memory(instr))
        mem_store++;
    else if(instr_reads_memory(instr))
        mem_load++;
}

// Transformation
void
InsTransEventCallback(void *drcontext, instr_instrument_msg_t *instrument_msg)
{

    instrlist_t *bb = instrument_msg->bb;
    instr_t *instr = instrument_msg->instr;
    int32_t slot = instrument_msg->slot;

    dr_insert_clean_call(drcontext, bb, instr, (void *)InsCount, false, 1, OPND_CREATE_MEMPTR(instr));
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

    drcctlib_init(DRCCTLIB_FILTER_ALL_INSTR, INVALID_FILE, InsTransEventCallback, false);
}

static void
ClientExit(void)
{
    dr_fprintf(gTraceFile, "MEMORY LOAD : %d", mem_load);
    dr_fprintf(gTraceFile, "MEMORY STORE : %d", mem_store);
    dr_fprintf(gTraceFile, "CONDITIONAL BRANCHES : %d", cond_branch);
    dr_fprintf(gTraceFile, "UNCONDITIONAL BRANCHES : %d", uncond_branch);

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