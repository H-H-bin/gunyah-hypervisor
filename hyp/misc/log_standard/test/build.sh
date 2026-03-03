# edit command below
clang -std=gnu18 -DHYP_STANDALONE_TEST -DNDEBUG -Wno-gcc-compat -I build/<platform>/<featureset>/production/include -I hyp/interfaces/util/include -I hyp/misc/log_standard/include/ -o test hyp/misc/log_standard/src/string_util.c hyp/misc/log_standard/test/string_test.c
