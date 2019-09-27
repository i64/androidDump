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

#include <limits.h>

int get_pid_from_package_name(char *package_name)
{

    size_t MAX_PROC = 1024;
    const char *proc_directory = "/proc";
    char *proc_name = calloc(1, MAX_PROC);
    DIR *directory = opendir(proc_directory);

    if (directory)
    {

        struct dirent *de = 0;

        while ((de = readdir(directory)) != 0)
        {
            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
                continue;

            int pid = -1;
            int res = sscanf(de->d_name, "%d", &pid);

            if (res == 1)
            {
                char cmdline_file[1024] = {0};
                sprintf(cmdline_file, "%s/%d/cmdline", proc_directory, pid);

                FILE *cmdline = fopen(cmdline_file, "r");

                if (getline(&proc_name, &MAX_PROC, cmdline) > 0)
                {
                    if (strstr(proc_name, package_name) != 0)
                    {
                        return pid;
                    }
                }

                fclose(cmdline);
            }
        }
        closedir(directory);
        return 1;
    }
}

unsigned long array[127][30];
int stat_counter = -1;

int verifier(long inode, int stat_counter)
{
    for (int i = 0; i <= stat_counter; i++)
    {
        if (array[i][0] == inode)
        {
            return 1;
        }
    }
    return 0;
}

int main(int argc, char **argv)
{

    char *package_name = argv[1];
    pid_t const pid = get_pid_from_package_name(package_name);

    printf("%d\n", pid);

    char sf_maps[1024];
    
    sprintf(sf_maps, "/proc/%d/maps", pid);
    
    FILE *f_maps = fopen(sf_maps, "r");
    
    int i = 0;
    int i_arr[30] = {0};
    char line[256] = {0};

    while (fgets(line, 256, f_maps) != NULL)
    {

        unsigned long inode;
        char c_director[127];
        unsigned long addr_start;
        unsigned long addr_end;

        #if (__WORDSIZE == 32)
            sscanf(line, "%8lx-%8lx", &addr_start, &addr_end);
        #else
            sscanf(line, "%12lx-%12lx", &addr_start, &addr_end);
        #endif

        sscanf(line, "%*s %*s %*s %*s %ld\n", &inode);
        sscanf(line, "%*[^//]%[^\n]", c_director);

        if (strstr(c_director, package_name) != 0 && strstr(c_director, package_name) != NULL && inode != 0)
        {

            if (verifier(inode, stat_counter + 1) == 0)
            {
                stat_counter++;
                i = 0;
                array[stat_counter][i] = inode;
            }
            array[stat_counter][i + 1] = addr_start;
            array[stat_counter][i + 2] = addr_end;
            i += 2;
            i_arr[stat_counter] = i / 2;
        }
    }

    fclose(f_maps);
    for (int j = 0; j <= stat_counter; ++j)
    {

        int r = 1;
        char inode[10];

        sprintf(inode, "%ld", array[j][0]);

        FILE *fp;
        fp = fopen(inode, "a");

        for (int i = 1; i <= i_arr[j]; i++)
        {
            uintptr_t const address = array[j][r];
            size_t const size = array[j][r + 1] - array[j][r];
            char *mem_buffer = NULL;

            printf("%lx => %lx | ", array[j][r], array[j][r + 1]);

            asprintf(&mem_buffer, "/proc/%d/mem", pid);
            char *result_buffer = malloc(size);
            ptrace(PTRACE_ATTACH, pid, NULL, NULL);
            int memfd = open(mem_buffer, O_RDONLY);

            assert(memfd != -1);

            pread(memfd, result_buffer, size, address);
            fwrite(result_buffer, 1, size, fp);

            ptrace(PTRACE_DETACH, pid, NULL, 0);
            close(memfd);
            fclose(fp);

            free(mem_buffer);
            free(result_buffer);

            r += 2;
        }
        printf(" <=> %s\n", inode);
    }

    return 0;
}
