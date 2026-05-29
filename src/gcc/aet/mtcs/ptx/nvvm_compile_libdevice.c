#include <nvvm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * 用格式是bitcode的libdevice.10.bc生成文本文件
 * libdevice.10.bc是一个基础数学函数库
 * 该文件不属于编译器。
 * 编译方法
 * export CUDA_HOME=/usr/local/cuda
 * gcc nvvm_compile_libdevice.c -o nvvm_libdevice     -I$CUDA_HOME/nvvm/include     -L$CUDA_HOME/nvvm/lib64 -lnvvm
 * 运行方法
 * export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/cuda/nvvm/lib64
 * ./nvvm_libdevice compute_80 /usr/local/cuda/nvvm/libdevice/libdevice.10.bc libdevice.ptx
*/

static char* read_file(const char *path, size_t *size_out)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "ERROR: failed to open %s\n", path);
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *buf = (char*)malloc(size);
    if (!buf) {
        fprintf(stderr, "ERROR: malloc failed\n");
        fclose(fp);
        return NULL;
    }

    if (fread(buf, 1, size, fp) != size) {
        fprintf(stderr, "ERROR: failed to read file\n");
        free(buf);
        fclose(fp);
        return NULL;
    }

    fclose(fp);
    *size_out = size;
    return buf;
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        printf("Usage: %s <compute_version> <libdevice.bc> <output.ptx>\n", argv[0]);
        return 1;
    }
    const char *compute_version = argv[1];
    const char *input_bc = argv[2];
    const char *output_ptx = argv[3];

    size_t bc_size = 0;
    char *bc_buf = read_file(input_bc, &bc_size);
    if (!bc_buf) return 1;

    // 创建 NVVM Program
    nvvmProgram prog;
    nvvmResult res = nvvmCreateProgram(&prog);
    if (res != NVVM_SUCCESS) {
        fprintf(stderr, "ERROR: nvvmCreateProgram failed\n");
        return 1;
    }

    // 加载 bitcode
    res = nvvmAddModuleToProgram(prog, bc_buf, bc_size, "libdevice");
    if (res != NVVM_SUCCESS) {
        fprintf(stderr, "ERROR: nvvmAddModuleToProgram failed\n");
        return 1;
    }

    // 设置架构
    char arch[512];
    sprintf(arch,"-arch=%s",compute_version);
    const char *options[] = {arch};

    // 编译
    res = nvvmCompileProgram(prog, 1, options);
    if (res != NVVM_SUCCESS) {
        size_t logSize;
        nvvmGetProgramLogSize(prog, &logSize);
        char *log = (char *)malloc(logSize);
        nvvmGetProgramLog(prog, log);
        fprintf(stderr, "NVVM Compile Error:\n%s\n", log);
        free(log);
        nvvmDestroyProgram(&prog);
        return 1;
    }

    // 获取 PTX 输出
    size_t ptxSize;
    nvvmGetCompiledResultSize(prog, &ptxSize);

    char *ptx = (char*)malloc(ptxSize);
    nvvmGetCompiledResult(prog, ptx);

    // 写入文件
    FILE *outf = fopen(output_ptx, "wb");
    if (!outf) {
        fprintf(stderr, "ERROR: failed to write PTX file\n");
        return 1;
    }

    fwrite(ptx, 1, ptxSize, outf);
    fclose(outf);

    printf("OK: Generated PTX: %s  (size = %zu bytes)\n", output_ptx, ptxSize);

    free(ptx);
    free(bc_buf);
    nvvmDestroyProgram(&prog);

    return 0;
}

// "__nv_expf" → "expf"
// "__nv_exp"  → "exp"
static const char* nv_to_math(const char* nv) {
    if (strncmp(nv, "__nv_", 5) != 0)
        return NULL;

    static char out[64];
    memset(out, 0, sizeof(out));

    const char* p = nv + 5;

    // copy until end or non alpha
    int i = 0;
    while (*p && (isalnum(*p) || *p=='_'))
        out[i++] = *p++;

    out[i] = 0;

    return out;
}


static void trim(char* s) {
    char* p = s;
    while (*p && isspace(*p)) p++;
    memmove(s, p, strlen(p)+1);

    p = s + strlen(s)-1;
    while (p >= s && isspace(*p)) *p-- = 0;
}

//生成文件
static int createFile(char *output_ptx) {
    FILE* in = fopen(output_ptx/*!"libdevice.ptx"*/, "r");
    if (!in) {
        perror("open libdevice.ptx");
        return 1;
    }

    FILE* csv = fopen("math_map.csv", "w");
    FILE* hout = fopen("math_map.h", "w");

    fprintf(csv, "mathh,libdevice\n");

    fprintf(hout,
        "// math_map.h (generated from PTX)\n"
        "typedef struct { const char* mathh; const char* libdevice; } MathMap;\n"
        "static const MathMap kMathMap[] = {\n"
    );

    char line[4096];
    while (fgets(line, sizeof(line), in)) {
        trim(line);

        // pattern: .func __nv_*
        char* p = strstr(line, ".func");
        if (!p) p = strstr(line, ".visible .func");
        if (!p) continue;

        char* at = strchr(p, '_'); // first underscore inside name
        if (!at) continue;

        // extract function name
        char name[128];
        int i = 0;
        while (at[i] && (isalnum(at[i]) || at[i]=='_' ))
            name[i] = at[i], i++;
        name[i] = 0;

        if (!strstr(name, "__nv_")) continue;

        const char* math = nv_to_math(name);
        if (!math) continue;

        fprintf(csv, "%s,%s\n", math, name);
        fprintf(hout, "    {\"%s\", \"%s\"},\n", math, name);
    }

    fprintf(hout, "};\n");

    fclose(in);
    fclose(csv);
    fclose(hout);

    printf("Generated math_map.csv and math_map.h successfully.\n");
    return 0;
}
