
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

static inline pair<int64_t, int64_t> 
getMaxMin(uint8_t dst_bytes) {
    int64_t min_int_val = 0;
    int64_t max_int_val = 0;
    switch (dst_bytes) {
        case 1: 
            min_int_val = std::numeric_limits<int8_t>::min();
            max_int_val = std::numeric_limits<int8_t>::max();
            break;
        case 2: 
            min_int_val = std::numeric_limits<int16_t>::min();
            max_int_val = std::numeric_limits<int16_t>::max();
            break;
        case 4: 
            min_int_val = std::numeric_limits<int32_t>::min();
            max_int_val = std::numeric_limits<int32_t>::max();
            break;
        case 8: 
            min_int_val = std::numeric_limits<int64_t>::min();
            max_int_val = std::numeric_limits<int64_t>::max();
            break;
        default: 
            cout << "in default case" << endl;
            break;
    }
    std::pair <int64_t,int64_t> maxMin(min_int_val, max_int_val);
    return maxMin;
}

static inline bool
safe_add(int64_t a, int64_t b, uint8_t dst_bytes)
{
    pair<int64_t, int64_t> maxMin = getMaxMin(dst_bytes);
    int64_t min_int_val = maxMin.first;
    int64_t max_int_val = maxMin.second;

    if (a > 0 && b > max_int_val - a)
        return true;
    
    // in this case below: 
    // min_int_val - a = min_int_val + a 
    // because a < 0
    else if (a < 0 && b < min_int_val - a) 
        return true;
    
    else
        return false;
    return false;
}

static inline bool
safe_sub(int64_t a, int64_t b, uint8_t dst_bytes)
{
    pair<int64_t, int64_t> maxMin = getMaxMin(dst_bytes);
    int64_t min_int_val = maxMin.first;
    int64_t max_int_val = maxMin.second;

    // a - b = c
    // max int val is 127
    // min int val is -128
    // 5 - -123
    if (a > 0 && b < min_int_val + a)
        return true;
    
    // in this case below: 
    // min_int_val - a = min_int_val + a 
    // because a < 0
    // max int val is 127
    // min int val is -128
    // -5 - 123
    else if (a < 0 && b > max_int_val + a + 1) 
        return true;
    
    else
        return false;
    return false;
}

static inline bool
safe_shl(int64_t a, int64_t b, uint8_t dst_bytes)
{
    pair<int64_t, int64_t> maxMin = getMaxMin(dst_bytes);
    int64_t min_int_val = maxMin.first;
    int64_t max_int_val = maxMin.second;

    int16_t msb = floor(log2(a));
    // 1 << 7 = 128
    // max int for signed 8 bit int is 127
    if (a > 0 && msb + b > dst_bytes - 2)
        return true;
    
    // if a < 0 shl has undefined behavior in c++11
    else
        return false;

    return false;
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
        bool is_overflow = safe_add(src1_val, src2_val, dst.size);
        return is_overflow;
    } else if (opType == OperatorType::kOPsub) {
        bool is_overflow = safe_sub(src1_val, src2_val, dst.size);
        return is_overflow;
    } else if (opType == OperatorType::kOPshl) {
        bool is_overflow = safe_shl(src1_val, src2_val, dst.size);
        return is_overflow;
    } else {
        return false;
    }
    return false;
}

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