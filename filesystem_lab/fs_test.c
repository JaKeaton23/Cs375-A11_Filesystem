// fs_test.c
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define MAX_NAME 256
#define BUFFER_SIZE 50000

extern struct simplefs {
    char blocks[1024][4096];
    struct inode {
        int id;
        int size;
        char permissions[10];
        int ref_count;
        int blocks[12];
        int indirect_block;
        int owner_uid;
        int group_id;
        time_t timestamp;
    } inodes[128];
    struct dir_entry {
        char name[256];
        int inode_id;
        int is_soft_link;
        char link_path[256];
    } directory[128];
    struct log_entry {
        char operation[256];
        time_t timestamp;
        unsigned int hash;
    } logs[100];
    int block_count, inode_count, dir_count, log_count;
} simplefs;

extern void init_fs(struct simplefs *fs);
extern int create_file(struct simplefs *fs, const char *name, const char *permissions, int uid, const char *data);
extern int read_file(struct simplefs *fs, const char *name, int uid, char *buffer, int max_len);
extern int create_hard_link(struct simplefs *fs, const char *existing_name, const char *new_name, int uid);
extern int create_soft_link(struct simplefs *fs, const char *existing_name, const char *new_name, int uid);
extern void print_logs(struct simplefs *fs);
extern void verify_logs(struct simplefs *fs);

int main() {

    struct simplefs fs;
    init_fs(&fs);

    char buffer[BUFFER_SIZE];

    //
    // BASE FILE CREATION FOR ALL TESTS
    //
    create_file(&fs, "file1.txt", "rw-r--r--", 1001, "Hello, Filesystem!");

    printf("Read: %s\n",
        read_file(&fs, "file1.txt", 1001, buffer, BUFFER_SIZE) > 0 ?
        buffer : "FAILED");

    //
    // HARD LINK TEST
    //
    create_hard_link(&fs, "file1.txt", "file1_link.txt", 1001);
    printf("Hard link read: %s\n",
        read_file(&fs, "file1_link.txt", 1001, buffer, BUFFER_SIZE) > 0 ?
        buffer : "FAILED");

    //
    // SOFT LINK TEST
    //
    create_soft_link(&fs, "file1.txt", "file1_soft.txt", 1001);
    printf("Soft link read: %s\n",
        read_file(&fs, "file1_soft.txt", 1001, buffer, BUFFER_SIZE) > 0 ?
        buffer : "FAILED");

    //
    // ===== INDIRECT BLOCK TEST =====
    //
    char bigdata[50000];
    for (int i = 0; i < 50000; i++) bigdata[i] = 'A';

    create_file(&fs, "bigfile.txt", "rw-r--r--", 1001, bigdata);

    if (read_file(&fs, "bigfile.txt", 1001, buffer, BUFFER_SIZE) > 0) {
        printf("\nIndirect Block Test:\n");
        printf("Big file read success (first 50 chars): %.50s\n", buffer);
    }

    //
    // ===== GROUP PERMISSION TEST =====
    //
    printf("\nGroup Permission Test:\n");

    int r1 = read_file(&fs, "file1.txt", 1001, buffer, BUFFER_SIZE);
    printf("Owner read (UID 1001): %d\n", r1);

    int r2 = read_file(&fs, "file1.txt", 1011, buffer, BUFFER_SIZE);
    printf("Group read (UID 1011): %d\n", r2);

    int r3 = read_file(&fs, "file1.txt", 2002, buffer, BUFFER_SIZE);
    printf("Other read (UID 2002): %d\n", r3);

    //
    // Print real logs
    //
    print_logs(&fs);

    //
    // Verify original logs
    //
    verify_logs(&fs);

    //
    // ===== TAMPERING TEST =====
    //
    printf("\nTampering Test:\n");
    fs.logs[0].hash = 99999; // force tamper
    verify_logs(&fs);

    return 0;
}
