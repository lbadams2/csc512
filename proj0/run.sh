cd /afs/unity.ncsu.edu/users/l/lbadams2/csc512
mkdir proj0 && cd proj0
git clone --recurse https://github.com/Xuhpclab/DrCCTProf.git

mkdir DrCCTProf/src/clients/drcctlib_instr_analysis
vim DrCCTProf/src/clients/drcctlib_instr_analysis/CMakeLists.txt
vim DrCCTProf/src/clients/drcctlib_instr_analysis/drcctlib_instr_analysis.cpp

./DrCCTProf/build.sh # or ./DrCCTProf/scripts/build_tool/remake.sh

vim p0_test_app.c
gcc -g p0_test_app.c -o p0_test_app

export drrun=/afs/unity.ncsu.edu/users/l/lbadams2/csc512/proj0/DrCCTProf/build/bin64/drrun
$drrun -t drcctlib_instr_analysis -- p0_test_app