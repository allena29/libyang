#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <ctype.h>
#include <libyang/libyang.h>

void log_cb(LY_LOG_LEVEL level, const char *msg, const char *data_path, const char *schema_path, uint64_t line) {
    fprintf(stderr, "libyang [%d]: %s (line %lu)\n", level, msg, (unsigned long)line);
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;

    const char *target_dir = argv[1];
    struct ly_ctx *ctx = NULL;
    DIR *d = opendir(target_dir);
    struct dirent *dir;

    ly_log_level(LY_LLVRB);
    ly_set_log_clb(log_cb);

    if (ly_ctx_new(target_dir, LY_CTX_REF_IMPLEMENTED | LY_CTX_NO_YANGLIBRARY, &ctx) != LY_SUCCESS) {
        fprintf(stderr, "Failed to create context\n");
        closedir(d);
        return 1;
    }

    while ((dir = readdir(d)) != NULL) {
        size_t len = strlen(dir->d_name);
        if (len < 5 || strcmp(dir->d_name + len - 5, ".yang") != 0) continue;
        if (strstr(dir->d_name, "deviation")) continue;

        char path[4096];
        snprintf(path, sizeof(path), "%s/%s", target_dir, dir->d_name);

        /* Peek to skip submodules before libyang touches them */
        FILE *f = fopen(path, "r");
        if (!f) continue;
        char buf[512];
        size_t bytes = fread(buf, 1, sizeof(buf) - 1, f);
        buf[bytes] = '\0';
        fclose(f);

        char *ptr = buf;
        while (*ptr && isspace((unsigned char)*ptr)) ptr++;
        if (strncmp(ptr, "submodule", 9) == 0 && isspace((unsigned char)ptr[9])) {
            continue;
        }

        struct lys_module *mod = NULL;
        printf("DEBUG: Parsing %s\n", dir->d_name);
        
        /* Parse and implement only this specific module */
        if (lys_parse_path(ctx, path, LYS_IN_YANG, &mod) == LY_SUCCESS) {
            lys_set_implemented(mod, NULL);
            printf("SUCCESS: Loaded %s\n", mod->name);
        } else {
            fprintf(stderr, "ERROR: Failed to parse %s\n", dir->d_name);
        }
    }

    closedir(d);
    ly_ctx_destroy(ctx);
    return 0;
}
