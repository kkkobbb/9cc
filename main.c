/*
 * simple compiler (output x86_64 asm)
 */
#include <stdio.h>

#include "9cc.h"


int main(int argc, char **argv) {
    // ./9cc <プログラム文>
    if (argc != 2) {
        fprintf(stderr, "引数の個数が正しくありません");
        return 1;
    }

    // 構文解析
    Node **code = parse(argv[1]);

    // asm出力
    codegen(code);

    return 0;
}
