// simplefs.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BLOCK_SIZE 4096
#define MAX_BLOCKS 1024
#define MAX_INODES 128
#define MAX_NAME 256
#define MAX_LOGS 100

struct inode {
    int id;
    int size;
    char permissions[10];   
    int ref_count;
    int blocks[12];         // direct blocks
    int indirect_block;     // single indirect
    int owner_uid;
    int group_id;
    time_t timestamp;
};

struct dir_entry {
    char name[MAX_NAME];
    int inode_id;
    int is_soft_link;
    char link_path[MAX_NAME];
};

struct log_entry {
    char operation[MAX_NAME];
    time_t timestamp;
    unsigned int hash;
};

struct simplefs {
    char blocks[MAX_BLOCKS][BLOCK_SIZE];
    struct inode inodes[MAX_INODES];
    struct dir_entry directory[MAX_INODES];
    struct log_entry logs[MAX_LOGS];

    int block_count;
    int inode_count;
    int dir_count;
    int log_count;
};

static unsigned int compute_hash(const char *op, time_t ts) {
    return (unsigned int)(((int)ts) ^ (int)strlen(op));
}

void init_fs(struct simplefs *fs) {
    memset(fs, 0, sizeof(struct simplefs));
    for (int i = 0; i < MAX_INODES; i++) {
        fs->inodes[i].indirect_block = -1;
        fs->inodes[i].group_id = -1;
    }
}

int create_file(struct simplefs *fs, const char *name, const char *permissions,
                int uid, const char *data) {

    if (fs->inode_count >= MAX_INODES || fs->block_count >= MAX_BLOCKS)
        return -1;

    struct inode *inode = &fs->inodes[fs->inode_count++];
    inode->id = fs->inode_count - 1;
    inode->size = strlen(data);
    strncpy(inode->permissions, permissions, 10);
    inode->ref_count = 1;
    inode->owner_uid = uid;
    inode->group_id = uid % 10;
    inode->timestamp = time(NULL);
    inode->indirect_block = -1;

    int total_blocks = (inode->size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    int num_direct = total_blocks < 12 ? total_blocks : 12;

    for (int i = 0; i < num_direct; i++) {
        int block_id = fs->block_count++;
        inode->blocks[i] = block_id;

        for (int j = 0; j < BLOCK_SIZE && j < inode->size - (i * BLOCK_SIZE); j++)
            fs->blocks[block_id][j] =
                data[i * BLOCK_SIZE + j] ^ 0x55;
    }

    if (total_blocks > 12) {
        inode->indirect_block = fs->block_count++;
        int *indirect_array = (int *)fs->blocks[inode->indirect_block];

        int remaining = total_blocks - 12;

        for (int i = 0; i < remaining; i++) {
            int block_id = fs->block_count++;
            indirect_array[i] = block_id;

            for (int j = 0; j < BLOCK_SIZE &&
                            j < inode->size - ((12 + i) * BLOCK_SIZE);
                 j++) {
                fs->blocks[block_id][j] =
                    data[(12 + i) * BLOCK_SIZE + j] ^ 0x55;
            }
        }
    }

    struct dir_entry *entry = &fs->directory[fs->dir_count++];
    strncpy(entry->name, name, MAX_NAME);
    entry->inode_id = inode->id;
    entry->is_soft_link = 0;

    struct log_entry *log = &fs->logs[fs->log_count % MAX_LOGS];
    snprintf(log->operation, MAX_NAME, "Created file %s by UID %d", name, uid);
    log->timestamp = time(NULL);
    log->hash = compute_hash(log->operation, log->timestamp);
    fs->log_count++;

    return inode->id;
}

int read_file(struct simplefs *fs, const char *name, int uid,
              char *buffer, int max_len) {

    for (int d = 0; d < fs->dir_count; d++) {
        if (strcmp(fs->directory[d].name, name) == 0) {

            struct inode *inode = &fs->inodes[fs->directory[d].inode_id];

            int user_group = uid % 10;
            if (inode->owner_uid == uid) {
            } else if (inode->group_id == user_group && inode->permissions[3] == 'r') {
            } else if (inode->permissions[6] == 'r') {
            } else {
                return -1;
            }

            int total_blocks = (inode->size + BLOCK_SIZE - 1) / BLOCK_SIZE;
            int bytes_read = 0;
            int remaining = inode->size;

            for (int i = 0; i < 12 && remaining > 0; i++) {
                int block_id = inode->blocks[i];
                if (i >= total_blocks) break;

                int to_copy = remaining < BLOCK_SIZE ? remaining : BLOCK_SIZE;

                for (int j = 0; j < to_copy && bytes_read < max_len - 1; j++)
                    buffer[bytes_read++] = fs->blocks[block_id][j] ^ 0x55;

                remaining -= to_copy;
            }

            if (remaining > 0 && inode->indirect_block != -1) {
                int *indirect_array = (int *)fs->blocks[inode->indirect_block];
                int i = 0;

                while (remaining > 0) {
                    int block_id = indirect_array[i++];

                    int to_copy = remaining < BLOCK_SIZE ? remaining : BLOCK_SIZE;

                    for (int j = 0; j < to_copy && bytes_read < max_len - 1; j++)
                        buffer[bytes_read++] =
                            fs->blocks[block_id][j] ^ 0x55;

                    remaining -= to_copy;
                }
            }

            buffer[bytes_read] = '\0';

            struct log_entry *log = &fs->logs[fs->log_count % MAX_LOGS];
            snprintf(log->operation, MAX_NAME,
                     "Read file %s by UID %d", name, uid);
            log->timestamp = time(NULL);
            log->hash = compute_hash(log->operation, log->timestamp);
            fs->log_count++;

            return inode->size;
        }
    }
    return -1;
}

int create_hard_link(struct simplefs *fs,
                     const char *existing_name,
                     const char *new_name,
                     int uid) {

    for (int i = 0; i < fs->dir_count; i++) {
        if (strcmp(fs->directory[i].name, existing_name) == 0) {

            struct inode *inode = &fs->inodes[fs->directory[i].inode_id];

            int user_group = uid % 10;
            if (inode->owner_uid == uid) {
            } else if (inode->group_id == user_group && inode->permissions[4] == 'w') {
            } else if (inode->permissions[7] == 'w') {
            } else {
                return -1;
            }

            inode->ref_count++;

            struct dir_entry *entry = &fs->directory[fs->dir_count++];
            strncpy(entry->name, new_name, MAX_NAME);
            entry->inode_id = inode->id;
            entry->is_soft_link = 0;

            struct log_entry *log = &fs->logs[fs->log_count % MAX_LOGS];
            snprintf(log->operation, MAX_NAME,
                     "Created hard link %s to %s by UID %d",
                     new_name, existing_name, uid);
            log->timestamp = time(NULL);
            log->hash = compute_hash(log->operation, log->timestamp);
            fs->log_count++;

            return 0;
        }
    }
    return -1;
}

int create_soft_link(struct simplefs *fs,
                     const char *existing_name,
                     const char *new_name,
                     int uid) {

    struct dir_entry *entry = &fs->directory[fs->dir_count++];
    strncpy(entry->name, new_name, MAX_NAME);
    strncpy(entry->link_path, existing_name, MAX_NAME);
    entry->is_soft_link = 1;

    struct log_entry *log = &fs->logs[fs->log_count % MAX_LOGS];
    snprintf(log->operation, MAX_NAME,
             "Created soft link %s to %s by UID %d",
             new_name, existing_name, uid);
    log->timestamp = time(NULL);
    log->hash = compute_hash(log->operation, log->timestamp);
    fs->log_count++;

    return 0;
}

void print_logs(struct simplefs *fs) {
    printf("Filesystem Logs:\n");
    for (int i = 0; i < fs->log_count && i < MAX_LOGS; i++) {
        printf("[%ld] %s (Hash: %u)\n",
               (long)fs->logs[i].timestamp,
               fs->logs[i].operation,
               fs->logs[i].hash);
    }
}

void verify_logs(struct simplefs *fs) {
    printf("Verifying log integrity:\n");
    for (int i = 0; i < fs->log_count && i < MAX_LOGS; i++) {
        struct log_entry *log = &fs->logs[i];
        unsigned int expected = compute_hash(log->operation, log->timestamp);
        if (expected == log->hash) {
            printf("Log %d OK\n", i);
        } else {
            printf("Log %d TAMPERED: %s\n", i, log->operation);
        }
    }
}
