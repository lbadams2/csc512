
#include <iostream>
#include <bitset>
#include <cstdint>
#include <cmath>
#include <limits>
#include "profiler.h"
#include "drcctlib_vscodeex_format.h"

using namespace std;
using namespace DrCCTProf;

/*
    Tips: different integer types have distinct boundaries
    INT64_MAX, INT32_MAX, INT16_MAX, INT8_MAX
    INT64_MIN, INT32_MIN, INT16_MIN, INT8_MIN
*/

namespace runtime_profiling {

static inline int64_t
GetOpndIntValue(Operand opnd)
{
    if (opnd.type == OperandType::kImmediateFloat ||
        opnd.type == OperandType::kImmediateDouble ||
        opnd.type == OperandType::kUnsupportOpnd || opnd.type == OperandType::kNullOpnd) {
        return 0;
    }
    int64_t value = 0;
    switch (opnd.size) {
    case 1: value = static_cast<int64_t>(opnd.value.valueInt8); break;
    case 2: value = static_cast<int64_t>(opnd.value.valueInt16); break;
    case 4: value = static_cast<int64_t>(opnd.value.valueInt32); break;
    case 8: value = static_cast<int64_t>(opnd.value.valueInt64); break;
    default: break;
    }
    return value;
}

// implement your algorithm in this function
static inline bool
IntegerOverflow(OperatorType opType, uint64_t flagsValue, Operand src1, Operand src2,
                Operand dst)
{
    //cout << "Iniside Integer Overflow" << endl;
    int64_t src1_val = GetOpndIntValue(src1);
    int64_t src2_val = GetOpndIntValue(src2);
    if (opType == OperatorType::kOPadd) {
        bool is_overflow = false;
        if((flagsValue & EFLAGS_READ_OF) != 0)
            is_overflow = false;
        return is_overflow;
    } else if (opType == OperatorType::kOPsub) {
        bool is_overflow = false;
        if((flagsValue & EFLAGS_READ_OF) != 0)
            is_overflow = false;
        return is_overflow;
    } else if (opType == OperatorType::kOPshl) {
        bool is_overflow = false;
        if((flagsValue & EFLAGS_READ_OF) != 0)
            is_overflow = false;
        return is_overflow;
    } else {
        return false;
    }
    return false;
}

// 
void
OnAfterInsExec(Instruction *instr, context_handle_t contxt, uint64_t flagsValue,
                        CtxtContainer *ctxtContainer)
{
    OperatorType opType = instr->getOperatorType();
    // add: Destination = Source0 + Source1
    if (opType == OperatorType::kOPadd) {
        Operand srcOpnd0 = instr->getSrcOperand(0);
        Operand srcOpnd1 = instr->getSrcOperand(1);
        Operand dstOpnd = instr->getDstOperand(0);
        std::bitset<64> bitFlagsValue(flagsValue);

        if (IntegerOverflow(opType, flagsValue, srcOpnd0, srcOpnd1, dstOpnd)) {
            ctxtContainer->addCtxt(contxt);
        }

        cout << "ip(" << hex << instr->ip << "):"
             << "add " << dec << GetOpndIntValue(srcOpnd0) << " "
             << GetOpndIntValue(srcOpnd1) << " -> " << GetOpndIntValue(dstOpnd) << " "
             << bitFlagsValue << endl;

    }

    // sub: Destination = Source0 - Source1
    if (opType == OperatorType::kOPsub) {
        Operand srcOpnd0 = instr->getSrcOperand(0);
        Operand srcOpnd1 = instr->getSrcOperand(1);
        Operand dstOpnd = instr->getDstOperand(0);
        std::bitset<64> bitFlagsValue(flagsValue);

        if (IntegerOverflow(opType, flagsValue, srcOpnd0, srcOpnd1, dstOpnd)) {
            ctxtContainer->addCtxt(contxt);
        }

        cout << "ip(" << hex << instr->ip << "):"
             << "sub " << dec << GetOpndIntValue(srcOpnd0) << " "
             << GetOpndIntValue(srcOpnd1) << " -> " << GetOpndIntValue(dstOpnd) << " "
             << bitFlagsValue << endl;

    }

    // shl: Destination = Source0 << Source1
    if (opType == OperatorType::kOPshl) {
        Operand srcOpnd0 = instr->getSrcOperand(0);
        Operand srcOpnd1 = instr->getSrcOperand(1);
        Operand dstOpnd = instr->getDstOperand(0);
        std::bitset<64> bitFlagsValue(flagsValue);

        if (IntegerOverflow(opType, flagsValue, srcOpnd0, srcOpnd1, dstOpnd)) {
            ctxtContainer->addCtxt(contxt);
        }

        cout << "ip(" << hex << instr->ip << "):"
             << "shl " << dec << GetOpndIntValue(srcOpnd0) << " "
             << GetOpndIntValue(srcOpnd1) << " -> " << GetOpndIntValue(dstOpnd) << " "
             << bitFlagsValue << endl;

    }
}

void
OnBeforeAppExit(CtxtContainer *ctxtContainer)
{
    Profile::profile_t *profile = new Profile::profile_t();
    profile->add_metric_type(1, "", "integer overflow occurrence");

    std::vector<context_handle_t> list = ctxtContainer->getCtxtList();
    for (size_t i = 0; i < list.size(); i++) {
        inner_context_t *cur_ctxt = drcctlib_get_full_cct(list[i]);
        profile->add_sample(cur_ctxt)->append_metirc((uint64_t)1);
        drcctlib_free_full_cct(cur_ctxt);
    }
    profile->serialize_to_file("integer-overflow-profile.drcctprof");
    delete profile;

    file_t profileTxt = dr_open_file("integer-overflow-profile.txt",
                                     DR_FILE_WRITE_OVERWRITE | DR_FILE_ALLOW_LARGE);
    DR_ASSERT(profileTxt != INVALID_FILE);
    for (size_t i = 0; i < list.size(); i++) {
        dr_fprintf(profileTxt, "INTEGER OVERFLOW\n");
        drcctlib_print_backtrace_first_item(profileTxt, list[i], true, false);
        dr_fprintf(profileTxt, "=>BACKTRACE\n");
        drcctlib_print_backtrace(profileTxt, list[i], false, true, -1);
        dr_fprintf(profileTxt, "\n\n");
    }
    dr_close_file(profileTxt);
}

}