#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef KEYLEN
#define KEYLEN 512
#endif

char *trim(char *str) {
    char *end = NULL;
    if (str == NULL) {
        return str;
    }
    if (*str == 0) {
        return str;
    }
    end = str + strlen(str) - 1;
    while (end > str && isspace(*end)) {
        end--;
    }
    *(end+1) = 0;
    return str;
}

int main(int argc, char **argv) {
    int rank, size;
    char buffer[KEYLEN];
    char *recvbuf = NULL;
    int i = 0;
    FILE *fp = NULL;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (argc < 3) MPI_Abort(MPI_COMM_WORLD, 1);
    fp = fopen(argv[1], "r");
    if (fp == NULL) MPI_Abort(MPI_COMM_WORLD, 1);
    memset(buffer, 0, sizeof(char) * KEYLEN);
    fread(buffer, 1, KEYLEN, fp);
    trim(buffer);
    fclose(fp);
    buffer[KEYLEN - 1] = 0;
    recvbuf = (char *) malloc(sizeof(char) * KEYLEN * size);
    if (recvbuf == NULL) {
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    MPI_Allgather(buffer, KEYLEN, MPI_CHAR, recvbuf, KEYLEN, MPI_CHAR, MPI_COMM_WORLD);

    fp = fopen(argv[2], "w");
    if (fp == NULL) MPI_Abort(MPI_COMM_WORLD, 1);
    for (i = 0; i < size; ++i) {
        fprintf(fp, "%s\n", (recvbuf + KEYLEN*i));
    }
    fclose(fp);

    free(recvbuf);

    MPI_Finalize();
    return 0;
}
