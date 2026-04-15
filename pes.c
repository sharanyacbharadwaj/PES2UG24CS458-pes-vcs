#include "pes.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "index.h"
#include "commit.h"
#include <dirent.h>

// ─── YOUR IMPLEMENTED FUNCTIONS (Phase 3) ───────────────────────────────────

void cmd_init() {
    mkdir(".pes", 0755);
    mkdir(".pes/objects", 0755);
    mkdir(".pes/refs", 0755);
    mkdir(".pes/refs/heads", 0755);

    FILE *head = fopen(".pes/HEAD", "w");
    if (head) {
        fprintf(head, "ref: refs/heads/main\n");
        fclose(head);
    }

    printf("Initialized empty PES repository\n");
}

void cmd_add(int argc, char *argv[]) {
    if (argc < 3) {
        printf("error: specify file(s)\n");
        return;
    }

    Index index;
    index_load(&index);

    for (int i = 2; i < argc; i++) {
        if (index_add(&index, argv[i]) != 0) {
            printf("error: failed to add %s\n", argv[i]);
        }
    }
}

void cmd_status() {
    Index index;
    index_load(&index);
    index_status(&index);
}

// ─── TEMP STUBS (Phase 4 & 5 not implemented yet) ───────────────────────────

void cmd_commit(int argc, char *argv[]) {
    if (argc < 4 || strcmp(argv[2], "-m") != 0) {
        printf("Usage: pes commit -m \"message\"\n");
        return;
    }

    const char *message = argv[3];

    ObjectID commit_id;
    if (commit_create(message, &commit_id) != 0) {
        printf("error: commit failed\n");
        return;
    }

    char hex[HASH_HEX_SIZE + 1];
    hash_to_hex(&commit_id, hex);

    printf("Committed as %s\n", hex);
}

void print_commit(const ObjectID *id, const Commit *commit, void *ctx) {
    (void)ctx;

    char hex[HASH_HEX_SIZE + 1];
    hash_to_hex(id, hex);

    printf("commit %s\n", hex);
    printf("Author: %s\n", commit->author);
    printf("Message: %s\n\n", commit->message);
}

void cmd_log() {
    if (commit_walk(print_commit, NULL) != 0) {
        printf("No commits yet\n");
    }
}

int branch_list() {
    DIR *dir = opendir(REFS_DIR);
    if (!dir) return -1;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue; // skip . and ..
        printf("%s\n", entry->d_name);
    }

    closedir(dir);
    return 0;
}

int branch_create(const char *name) {
    ObjectID head_id;

    if (head_read(&head_id) != 0) {
        printf("error: no commits yet\n");
        return -1;
    }

    char path[512];
    snprintf(path, sizeof(path), "%s/%s", REFS_DIR, name);

    FILE *fp = fopen(path, "w");
    if (!fp) return -1;

    char hex[HASH_HEX_SIZE + 1];
    hash_to_hex(&head_id, hex);

    fprintf(fp, "%s\n", hex);
    fclose(fp);

    return 0;
}

int branch_delete(const char *name) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", REFS_DIR, name);

    if (remove(path) != 0) {
        printf("error: branch '%s' not found\n", name);
        return -1;
    }

    return 0;
}

int checkout(const char *target) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", REFS_DIR, target);

    FILE *fp = fopen(path, "r");
    if (!fp) {
        printf("error: branch '%s' not found\n", target);
        return -1;
    }
    fclose(fp);

    FILE *head = fopen(HEAD_FILE, "w");
    if (!head) return -1;

    fprintf(head, "ref: refs/heads/%s\n", target);
    fclose(head);

    return 0;
}

// ─── PROVIDED: Phase 5 Command Wrappers ─────────────────────────────────────

void cmd_branch(int argc, char *argv[]) {
    if (argc == 2) {
        branch_list();
    } else if (argc == 3) {
        if (branch_create(argv[2]) == 0) {
            printf("Created branch '%s'\n", argv[2]);
        } else {
            fprintf(stderr, "error: failed to create branch '%s'\n", argv[2]);
        }
    } else if (argc == 4 && strcmp(argv[2], "-d") == 0) {
        if (branch_delete(argv[3]) == 0) {
            printf("Deleted branch '%s'\n", argv[3]);
        } else {
            fprintf(stderr, "error: failed to delete branch '%s'\n", argv[3]);
        }
    } else {
        fprintf(stderr, "Usage:\n  pes branch\n  pes branch <name>\n  pes branch -d <name>\n");
    }
}

void cmd_checkout(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: pes checkout <branch_or_commit>\n");
        return;
    }

    const char *target = argv[2];
    if (checkout(target) == 0) {
        printf("Switched to '%s'\n", target);
    } else {
        fprintf(stderr, "error: checkout failed. Do you have uncommitted changes?\n");
    }
}

// ─── PROVIDED: Command dispatch ─────────────────────────────────────────────

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: pes <command> [args]\n");
        fprintf(stderr, "\nCommands:\n");
        fprintf(stderr, "  init            Create a new PES repository\n");
        fprintf(stderr, "  add <file>...   Stage files for commit\n");
        fprintf(stderr, "  status          Show working directory status\n");
        fprintf(stderr, "  commit -m <msg> Create a commit from staged files\n");
        fprintf(stderr, "  log             Show commit history\n");
        fprintf(stderr, "  branch          List, create, or delete branches\n");
        fprintf(stderr, "  checkout <ref>  Switch branches or restore working tree\n");
        return 1;
    }

    const char *cmd = argv[1];

    if      (strcmp(cmd, "init") == 0)     cmd_init();
    else if (strcmp(cmd, "add") == 0)      cmd_add(argc, argv);
    else if (strcmp(cmd, "status") == 0)   cmd_status();
    else if (strcmp(cmd, "commit") == 0)   cmd_commit(argc, argv);
    else if (strcmp(cmd, "log") == 0)      cmd_log();
    else if (strcmp(cmd, "branch") == 0)   cmd_branch(argc, argv);
    else if (strcmp(cmd, "checkout") == 0) cmd_checkout(argc, argv);
    else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        fprintf(stderr, "Run 'pes' with no arguments for usage.\n");
        return 1;
    }

    return 0;
}
