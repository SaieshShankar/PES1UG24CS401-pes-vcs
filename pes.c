// pes.c — CLI entry point and command dispatch

#include "pes.h"
#include "index.h"
#include "commit.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

// Forward declarations
int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out);

// ─── cmd_init ────────────────────────────────────────────────────────────────
void cmd_init(void) {
    // Create .pes directory structure
    mkdir(PES_DIR, 0755);
    mkdir(OBJECTS_DIR, 0755);
    mkdir(".pes/refs", 0755);
    mkdir(REFS_DIR, 0755);

    // Write HEAD pointing to main branch
    FILE *f = fopen(HEAD_FILE, "w");
    if (f) {
        fprintf(f, "ref: refs/heads/main\n");
        fclose(f);
    }

    printf("Initialized empty PES repository in .pes/\n");
}

// ─── cmd_add ─────────────────────────────────────────────────────────────────
void cmd_add(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: pes add <file>...\n");
        return;
    }

    Index index;
    if (index_load(&index) != 0) {
        fprintf(stderr, "error: failed to load index\n");
        return;
    }

    for (int i = 2; i < argc; i++) {
        if (index_add(&index, argv[i]) != 0) {
            fprintf(stderr, "error: failed to stage '%s'\n", argv[i]);
        } else {
            printf("staged: %s\n", argv[i]);
        }
    }
}

// ─── cmd_status ──────────────────────────────────────────────────────────────
void cmd_status(void) {
    Index index;
    if (index_load(&index) != 0) {
        fprintf(stderr, "error: failed to load index\n");
        return;
    }
    index_status(&index);
}

// ─── cmd_log ─────────────────────────────────────────────────────────────────
static void print_commit_cb(const ObjectID *id, const Commit *c, void *ctx) {
    (void)ctx;
    char hex[HASH_HEX_SIZE + 1];
    hash_to_hex(id, hex);
    printf("commit %s\n", hex);
    printf("Author: %s\n", c->author);
    printf("Date:   %llu\n", (unsigned long long)c->timestamp);
    printf("\n    %s\n\n", c->message);
}

void cmd_log(void) {
    if (commit_walk(print_commit_cb, NULL) != 0) {
        fprintf(stderr, "error: no commits yet\n");
    }
}

// ─── cmd_commit ──────────────────────────────────────────────────────────────
// TODO: implement this function
void cmd_commit(int argc, char *argv[]) {
    // Parse -m <message>
    const char *message = NULL;
    for (int i = 2; i < argc - 1; i++) {
        if (strcmp(argv[i], "-m") == 0) {
            message = argv[i + 1];
            break;
        }
    }

    if (!message) {
        fprintf(stderr, "error: commit requires a message (-m \"message\")\n");
        return;
    }

    ObjectID commit_id;
    if (commit_create(message, &commit_id) != 0) {
        fprintf(stderr, "error: commit failed\n");
        return;
    }

    char hex[HASH_HEX_SIZE + 1];
    hash_to_hex(&commit_id, hex);
    // Print first 12 chars of hash + message
    printf("Committed: %.12s... %s\n", hex, message);
}

// ─── Stub branch/checkout functions (Phase 5 — analysis only) ───────────────
// These are called by the PROVIDED cmd_branch and cmd_checkout below.
// Minimal stubs so the binary compiles and all Phase 1-4 tests pass.

void branch_list(void) {
    // Print current branch
    FILE *f = fopen(HEAD_FILE, "r");
    if (!f) { printf("(no branches)\n"); return; }
    char line[512];
    if (fgets(line, sizeof(line), f)) {
        fclose(f);
        // line is "ref: refs/heads/main"
        char *branch = strstr(line, "refs/heads/");
        if (branch) {
            branch += strlen("refs/heads/");
            branch[strcspn(branch, "\r\n")] = '\0';
            printf("* %s\n", branch);
        }
    } else {
        fclose(f);
    }
}

int branch_create(const char *name) {
    // Read current HEAD commit
    ObjectID id;
    char ref_path[512];
    snprintf(ref_path, sizeof(ref_path), "%s/%s", REFS_DIR, name);
    FILE *f = fopen(ref_path, "w");
    if (!f) return -1;
    if (head_read(&id) == 0) {
        char hex[HASH_HEX_SIZE + 1];
        hash_to_hex(&id, hex);
        fprintf(f, "%s\n", hex);
    }
    fclose(f);
    return 0;
}

int branch_delete(const char *name) {
    char ref_path[512];
    snprintf(ref_path, sizeof(ref_path), "%s/%s", REFS_DIR, name);
    return unlink(ref_path);
}

int checkout(const char *target) {
    // Minimal: just update HEAD to point to target branch if it exists
    char ref_path[512];
    snprintf(ref_path, sizeof(ref_path), "%s/%s", REFS_DIR, target);
    if (access(ref_path, F_OK) == 0) {
        FILE *f = fopen(HEAD_FILE, "w");
        if (!f) return -1;
        fprintf(f, "ref: refs/heads/%s\n", target);
        fclose(f);
        return 0;
    }
    return -1;
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
        fprintf(stderr, "Usage:\n pes branch\n pes branch <name>\n pes branch -d <name>\n");
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
        fprintf(stderr, "  init              Create a new PES repository\n");
        fprintf(stderr, "  add <file>...     Stage files for commit\n");
        fprintf(stderr, "  status            Show working directory status\n");
        fprintf(stderr, "  commit -m <msg>   Create a commit from staged files\n");
        fprintf(stderr, "  log               Show commit history\n");
        fprintf(stderr, "  branch            List, create, or delete branches\n");
        fprintf(stderr, "  checkout <ref>    Switch branches or restore working tree\n");
        return 1;
    }

    const char *cmd = argv[1];
    if      (strcmp(cmd, "init")     == 0) cmd_init();
    else if (strcmp(cmd, "add")      == 0) cmd_add(argc, argv);
    else if (strcmp(cmd, "status")   == 0) cmd_status();
    else if (strcmp(cmd, "commit")   == 0) cmd_commit(argc, argv);
    else if (strcmp(cmd, "log")      == 0) cmd_log();
    else if (strcmp(cmd, "branch")   == 0) cmd_branch(argc, argv);
    else if (strcmp(cmd, "checkout") == 0) cmd_checkout(argc, argv);
    else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        fprintf(stderr, "Run 'pes' with no arguments for usage.\n");
        return 1;
    }
    return 0;
}
