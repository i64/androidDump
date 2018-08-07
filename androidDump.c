#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64
#undef ULONG_MAX
#define ULONG_MAX (__LONG_MAX__ * 2UL + 1UL)

#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ptrace.h>

int getPidFromPackageName(char *packageName)
{

    size_t maxProc = 1024;
    const char *procDir = "/proc";
    char *procIsmi = calloc(1, maxProc);
    DIR *dir = opendir(procDir);

    if (dir)
    {

        struct dirent *de = 0;

        while ((de = readdir(dir)) != 0)
        {
            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
                continue;

            int pid = -1;
            int res = sscanf(de->d_name, "%d", &pid);

            if (res == 1)
            {
                char cmdline_file[1024] = {0};
                sprintf(cmdline_file, "%s/%d/cmdline", procDir, pid);

                FILE *cmdline = fopen(cmdline_file, "r");

                if (getline(&procIsmi, &maxProc, cmdline) > 0)
                {
                    if (strstr(procIsmi, packageName) != 0)
                    {
                        return pid;
                    }
                }

                fclose(cmdline);
            }
        }
        closedir(dir);
        return 1;
    }
}

unsigned long array[127][30];
int statCounter = -1;

int checkIt(long offset, int statCounter)
{
    for (int i = 0; i <= statCounter; i++)
    {
        if (array[i][0] == offset)
        {
            return 1;
        }
    }
    return 0;
}

int main(int argc, char **argv)
{

    char *paketIsmi = argv[1];
    pid_t const pid = getPidFromPackageName(paketIsmi);

    char line[256];
    printf("%d\n", pid);

    char mapsFilename[1024];
    sprintf(mapsFilename, "/proc/%d/maps", pid);
    FILE *pMapsFile = fopen(mapsFilename, "r");
    int i = 0;
    int iArr[30] = {0};

    while (fgets(line, 256, pMapsFile) != NULL)
    {

        unsigned long offset;
        char dizin[127];
        unsigned long baslangicAdresi;
        unsigned long bitisAdresi;

        sscanf(line, "%8lx-%8lx", &baslangicAdresi, &bitisAdresi); // this is for 32 bit. change %8lx to %12lx for 64 bit
        sscanf(line, "%*s %*s %*s %*s %ld\n", &offset);
        sscanf(line, "%*[^//]%[^\n]", dizin);

        if (strstr(dizin, paketIsmi) != 0 && strstr(dizin, paketIsmi) != NULL && offset != 0)
        {

            if (checkIt(offset, statCounter + 1) == 0)
            {
                statCounter++;
                i = 0;
                array[statCounter][i] = offset;
            }
            array[statCounter][i + 1] = baslangicAdresi;
            array[statCounter][i + 2] = bitisAdresi;
            i += 2;
            iArr[statCounter] = i / 2;
        }
    }

    fclose(pMapsFile);
    for (int j = 0; j <= statCounter; ++j)
    {

        int r = 1;
        char offset[10];

        sprintf(offset, "%ld", array[j][0]);

        FILE *fp;
        fp = fopen(offset, "a");

        for (int i = 1; i <= iArr[j]; i++)
        {
            uintptr_t const address = array[j][r];
            size_t const size = array[j][r + 1] - array[j][r];
            char *memBuffer = NULL;

            printf("%lx => %lx | ", array[j][r], array[j][r + 1]);

            asprintf(&memBuffer, "/proc/%d/mem", pid);
            char *resultBuffer = malloc(size);
            ptrace(PTRACE_ATTACH, pid, NULL, NULL);
            int memfd = open(memBuffer, O_RDONLY);

            assert(memfd != -1);

            pread(memfd, resultBuffer, size, address);
            fwrite(resultBuffer, 1, size, fp);

            ptrace(PTRACE_DETACH, pid, NULL, 0);
            close(memfd);
            fclose(fp);

            free(memBuffer);
            free(resultBuffer);

            r += 2;
        }
        printf(" <=> %s\n", offset);
    }

    return 0;
}
