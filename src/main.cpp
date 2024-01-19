#include <core.h>
#include <stdio.h>
#include <unistd.h>

extern char **environ;

int main(int argc, char* argv[], char* envp[]) {
    FILE* dump = fopen("/Users/ryzerth/dump.txt", "wb");

    char buf[1024];
    fprintf(dump, "Working directory: %s\n\n", getcwd(buf, 1023));
    
    fprintf(dump, "Arguments:\n");
    fprintf(dump, "----------\n");
    for (int i = 0; i < argc; i++) {
        fprintf(dump, "%d: '%s'\n", i, argv[i]);
    }
    fprintf(dump, "\n");

    fprintf(dump, "Environment Variables:\n");
    fprintf(dump, "----------\n");
    for (char** env = environ; *env; env++) {
        fprintf(dump, "%s\n", *env);
    }
    fprintf(dump, "\n");

    fclose(dump);

    return sdrpp_main(argc, argv);
}