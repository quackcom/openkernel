/* openkernel - OKFS in-memory filesystem implementation */
#include "fs.h"
#include "memory.h"
#include "kernel.h"
#include <stddef.h>
#include <stdint.h>

#define FS_MAX_NODES 128
#define FS_CMD_TEXT_MAX 4096

typedef struct {
    int used;
    int parent;
    fs_type_t type;
    char name[FS_MAX_NAME];
    char *data;
    size_t size;
    size_t cap;
} fs_node_t;

static fs_node_t nodes[FS_MAX_NODES];
static int cwd = 0;
static char cwd_path[FS_MAX_PATH] = "/";

/* Editor session */
static int edit_active = 0;
static char edit_path[FS_MAX_PATH];
static char *edit_buf = NULL;
static size_t edit_len = 0;
static size_t edit_cap = 0;

/* Pending line: populated by ":e <n>" (no text) so the shell can
   pre-fill the command buffer with the current line content. */
static char edit_pending_line[FS_MAX_PATH];
static int edit_pending_line_valid = 0;

/* When ":e <n>" (no text) loads a line, this tracks which line to
   replace when the user types the next non-command line. */
static int edit_target_line = 0;

static size_t fs_strlen(const char *s)
{
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

static int fs_streq(const char *a, const char *b)
{
    while (*a && *b) {
        if (*a != *b) return 0;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static int fs_strncmp(const char *a, const char *b, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) return (unsigned char)a[i] - (unsigned char)b[i];
        if (a[i] == '\0') return 0;
    }
    return 0;
}

static void fs_strcpy(char *dst, const char *src, size_t max)
{
    size_t i = 0;
    while (src[i] && i < max - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int fs_ends_with_txt(const char *name)
{
    size_t len = fs_strlen(name);
    if (len < 4) return 0;
    return name[len - 4] == '.' &&
           name[len - 3] == 't' &&
           name[len - 2] == 'x' &&
           name[len - 1] == 't';
}

static int fs_is_valid_name(const char *name)
{
    if (!name || !name[0]) return 0;
    if (fs_streq(name, ".") || fs_streq(name, "..")) return 0;
    for (size_t i = 0; name[i]; i++) {
        char c = name[i];
        if (c == '/' || c == '\\') return 0;
    }
    return 1;
}

static int fs_alloc_node(void)
{
    for (int i = 1; i < FS_MAX_NODES; i++) {
        if (!nodes[i].used) {
            return i;
        }
    }
    return -1;
}

static int fs_find_child(int parent, const char *name)
{
    for (int i = 1; i < FS_MAX_NODES; i++) {
        if (nodes[i].used && nodes[i].parent == parent &&
            fs_streq(nodes[i].name, name)) {
            return i;
        }
    }
    return -1;
}

static void fs_rebuild_cwd_path(void)
{
    int stack[FS_MAX_NODES];
    int depth = 0;
    int cur = cwd;

    if (cur == 0) {
        cwd_path[0] = '/';
        cwd_path[1] = '\0';
        return;
    }

    while (cur > 0 && depth < FS_MAX_NODES) {
        stack[depth++] = cur;
        cur = nodes[cur].parent;
    }

    cwd_path[0] = '\0';
    for (int i = depth - 1; i >= 0; i--) {
        int id = stack[i];
        size_t pos = fs_strlen(cwd_path);
        if (pos > 0 && cwd_path[pos - 1] != '/') {
            if (pos < FS_MAX_PATH - 1) cwd_path[pos++] = '/';
            cwd_path[pos] = '\0';
        }
        const char *name = nodes[id].name;
        size_t nlen = fs_strlen(name);
        if (pos + nlen >= FS_MAX_PATH - 1) break;
        for (size_t j = 0; j < nlen; j++) cwd_path[pos++] = name[j];
        cwd_path[pos] = '\0';
    }

    if (cwd_path[0] == '\0') {
        cwd_path[0] = '/';
        cwd_path[1] = '\0';
    }
}

static int fs_resolve(int start, const char *path, int want_dir)
{
    int cur = start;
    char component[FS_MAX_NAME];
    size_t i = 0;

    if (!path || !path[0]) return FS_ERR_INVALID;

    if (path[0] == '/') {
        cur = 0;
        i = 1;
    }

    while (path[i] == '/') i++;

    while (path[i] != '\0') {
        size_t j = 0;
        while (path[i] && path[i] != '/') {
            if (j >= FS_MAX_NAME - 1) return FS_ERR_INVALID;
            component[j++] = path[i++];
        }
        component[j] = '\0';
        while (path[i] == '/') i++;

        if (component[0] == '\0') continue;

        if (fs_streq(component, ".")) continue;

        if (fs_streq(component, "..")) {
            if (cur == 0) continue;
            cur = nodes[cur].parent;
            continue;
        }

        int child = fs_find_child(cur, component);
        if (child < 0) {
            if (path[i] == '\0' && !want_dir) return FS_ERR_NOT_FOUND;
            return FS_ERR_NOT_FOUND;
        }
        cur = child;
    }

    if (want_dir && nodes[cur].type != FS_TYPE_DIR) return FS_ERR_NOT_DIR;
    return cur;
}

static int fs_resolve_parent(const char *path, char *out_name, int *out_parent)
{
    char copy[FS_MAX_PATH];
    size_t len = fs_strlen(path);
    if (len >= FS_MAX_PATH) return FS_ERR_INVALID;
    fs_strcpy(copy, path, FS_MAX_PATH);

    char *slash = NULL;
    for (size_t i = len; i > 0; i--) {
        if (copy[i - 1] == '/') {
            slash = &copy[i - 1];
            break;
        }
    }

    if (!slash) {
        *out_parent = (path[0] == '/') ? 0 : cwd;
        fs_strcpy(out_name, copy, FS_MAX_NAME);
        return FS_OK;
    }

    *slash = '\0';
    const char *parent_path = copy;
    if (parent_path[0] == '\0') parent_path = "/";

    int parent = fs_resolve((path[0] == '/') ? 0 : cwd, parent_path, 1);
    if (parent < 0) return parent;

    fs_strcpy(out_name, slash + 1, FS_MAX_NAME);
    *out_parent = parent;
    return FS_OK;
}

static int fs_set_file_data(int id, const char *data, size_t len, int append)
{
    if (len > FS_MAX_FILE_SIZE) return FS_ERR_NO_SPACE;

    size_t new_size = append ? nodes[id].size + len : len;
    if (new_size > FS_MAX_FILE_SIZE) return FS_ERR_NO_SPACE;

    if (new_size > nodes[id].cap) {
        size_t new_cap = nodes[id].cap ? nodes[id].cap : 256;
        while (new_cap < new_size) new_cap *= 2;
        if (new_cap > FS_MAX_FILE_SIZE) new_cap = FS_MAX_FILE_SIZE;

        char *nbuf = (char *)kmalloc(new_cap);
        if (!nbuf) return FS_ERR_NO_SPACE;

        if (append && nodes[id].data && nodes[id].size > 0) {
            for (size_t i = 0; i < nodes[id].size; i++) nbuf[i] = nodes[id].data[i];
        }
        if (nodes[id].data) kfree(nodes[id].data);
        nodes[id].data = nbuf;
        nodes[id].cap = new_cap;
    }

    if (append) {
        for (size_t i = 0; i < len; i++) {
            nodes[id].data[nodes[id].size + i] = data[i];
        }
        nodes[id].size = new_size;
    } else {
        for (size_t i = 0; i < len; i++) nodes[id].data[i] = data[i];
        nodes[id].size = len;
    }

    return FS_OK;
}

static int fs_create_entry(int parent, const char *name, fs_type_t type)
{
    if (!fs_is_valid_name(name)) return FS_ERR_INVALID;
    if (type == FS_TYPE_FILE && !fs_ends_with_txt(name)) return FS_ERR_NOT_TXT;
    if (fs_find_child(parent, name) >= 0) return FS_ERR_EXISTS;

    int id = fs_alloc_node();
    if (id < 0) return FS_ERR_FULL;

    nodes[id].used = 1;
    nodes[id].parent = parent;
    nodes[id].type = type;
    fs_strcpy(nodes[id].name, name, FS_MAX_NAME);
    nodes[id].data = NULL;
    nodes[id].size = 0;
    nodes[id].cap = 0;
    return id;
}

const char *fs_strerror(int err)
{
    switch (err) {
        case FS_OK: return "OK";
        case FS_ERR_NOT_FOUND: return "Not found";
        case FS_ERR_EXISTS: return "Already exists";
        case FS_ERR_NOT_DIR: return "Not a directory";
        case FS_ERR_NOT_FILE: return "Not a file";
        case FS_ERR_NOT_TXT: return "Only .txt files are supported";
        case FS_ERR_NO_SPACE: return "Out of memory or file too large";
        case FS_ERR_INVALID: return "Invalid path or name";
        case FS_ERR_FULL: return "Filesystem full";
        case FS_ERR_NOT_EMPTY: return "Directory not empty";
        default: return "Unknown error";
    }
}

void fs_init(void)
{
    for (int i = 0; i < FS_MAX_NODES; i++) {
        nodes[i].used = 0;
        nodes[i].parent = -1;
        nodes[i].type = FS_TYPE_DIR;
        nodes[i].name[0] = '\0';
        nodes[i].data = NULL;
        nodes[i].size = 0;
        nodes[i].cap = 0;
    }

    nodes[0].used = 1;
    nodes[0].parent = -1;
    nodes[0].type = FS_TYPE_DIR;
    nodes[0].name[0] = '\0';
    cwd = 0;
    cwd_path[0] = '/';
    cwd_path[1] = '\0';

    fs_mkdir("/docs");
    fs_write_file("/readme.txt",
        "Welcome to openkernel OKFS.\n"
        "Use 'help --fs' for file commands.\n"
        "Try: cat readme.txt\n",
        0);
    fs_write_file("/docs/about.txt",
        "openkernel educational filesystem.\n"
        "Text files use the .txt extension.\n",
        0);
}

const char *fs_pwd(void)
{
    return cwd_path;
}

int fs_cd(const char *path)
{
    int target = fs_resolve(cwd, path, 1);
    if (target < 0) return target;
    if (nodes[target].type != FS_TYPE_DIR) return FS_ERR_NOT_DIR;
    cwd = target;
    fs_rebuild_cwd_path();
    return FS_OK;
}

int fs_mkdir(const char *path)
{
    char name[FS_MAX_NAME];
    int parent = 0;
    int err = fs_resolve_parent(path, name, &parent);
    if (err != FS_OK) return err;
    if (!fs_is_valid_name(name)) return FS_ERR_INVALID;
    return fs_create_entry(parent, name, FS_TYPE_DIR) >= 0 ? FS_OK : FS_ERR_FULL;
}

int fs_touch(const char *path)
{
    char name[FS_MAX_NAME];
    int parent = 0;
    int err = fs_resolve_parent(path, name, &parent);
    if (err != FS_OK) return err;

    int existing = fs_find_child(parent, name);
    if (existing >= 0) {
        if (nodes[existing].type != FS_TYPE_FILE) return FS_ERR_NOT_FILE;
        return FS_OK;
    }

    int id = fs_create_entry(parent, name, FS_TYPE_FILE);
    return id >= 0 ? FS_OK : id;
}

int fs_rm(const char *path)
{
    int id = fs_resolve(cwd, path, 0);
    if (id < 0) return id;
    if (id == 0) return FS_ERR_INVALID;

    if (nodes[id].type == FS_TYPE_DIR) {
        for (int i = 1; i < FS_MAX_NODES; i++) {
            if (nodes[i].used && nodes[i].parent == id) return FS_ERR_NOT_EMPTY;
        }
    }

    if (nodes[id].data) {
        kfree(nodes[id].data);
        nodes[id].data = NULL;
    }

    if (cwd == id) {
        cwd = nodes[id].parent;
        fs_rebuild_cwd_path();
    }

    nodes[id].used = 0;
    nodes[id].size = 0;
    nodes[id].cap = 0;
    return FS_OK;
}

static void fs_emit_size_line(const char *prefix, const char *name, size_t size,
                              void (*emit)(const char *))
{
    char line[96];
    size_t p = 0;
    while (*prefix && p < 90) line[p++] = *prefix++;
    for (size_t i = 0; name[i] && p < 90; i++) line[p++] = name[i];
    const char *mid = " (";
    while (*mid && p < 90) line[p++] = *mid++;
    char num[16];
    size_t ni = 0;
    size_t v = size;
    if (v == 0) num[ni++] = '0';
    else {
        while (v > 0 && ni < 15) { num[ni++] = '0' + (v % 10); v /= 10; }
    }
    while (ni > 0 && p < 90) line[p++] = num[--ni];
    const char *tail = " bytes)";
    while (*tail && p < 90) line[p++] = *tail++;
    line[p] = '\0';
    emit(line);
}

int fs_ls(const char *path, void (*emit)(const char *))
{
    int dir;
    if (!path || !path[0] || fs_streq(path, ".") || fs_streq(path, "./")) {
        dir = cwd;
    } else {
        dir = fs_resolve(cwd, path, 1);
        if (dir < 0) return dir;
    }

    if (nodes[dir].type != FS_TYPE_DIR) return FS_ERR_NOT_DIR;

    int found = 0;
    for (int i = 1; i < FS_MAX_NODES; i++) {
        if (!nodes[i].used || nodes[i].parent != dir) continue;
        found = 1;
        if (nodes[i].type == FS_TYPE_DIR) {
            char line[FS_MAX_NAME + 16];
            size_t p = 0;
            const char *pfx = "[DIR]  ";
            while (*pfx) line[p++] = *pfx++;
            for (size_t j = 0; nodes[i].name[j] && p < sizeof(line) - 1; j++) {
                line[p++] = nodes[i].name[j];
            }
            line[p] = '\0';
            emit(line);
        } else {
            fs_emit_size_line("[FILE] ", nodes[i].name, nodes[i].size, emit);
        }
    }

    if (!found) emit("(empty)");
    return FS_OK;
}

int fs_cat(const char *path, void (*emit)(const char *))
{
    int id = fs_resolve(cwd, path, 0);
    if (id < 0) return id;
    if (nodes[id].type != FS_TYPE_FILE) return FS_ERR_NOT_FILE;
    if (!fs_ends_with_txt(nodes[id].name)) return FS_ERR_NOT_TXT;

    if (nodes[id].size == 0) {
        emit("(empty file)");
        return FS_OK;
    }

    char line[128];
    size_t lp = 0;
    for (size_t i = 0; i < nodes[id].size; i++) {
        char c = nodes[id].data[i];
        if (c == '\n' || lp >= 127) {
            line[lp] = '\0';
            emit(line);
            lp = 0;
            if (c == '\n') continue;
        }
        line[lp++] = c;
    }
    if (lp > 0) {
        line[lp] = '\0';
        emit(line);
    }
    return FS_OK;
}

int fs_write_file(const char *path, const char *data, size_t len)
{
    if (!data) return FS_ERR_INVALID;
    if (len == 0) len = fs_strlen(data);

    char name[FS_MAX_NAME];
    int parent = 0;
    int err = fs_resolve_parent(path, name, &parent);
    if (err != FS_OK) return err;

    int id = fs_find_child(parent, name);
    if (id < 0) {
        id = fs_create_entry(parent, name, FS_TYPE_FILE);
        if (id < 0) return id;
    } else if (nodes[id].type != FS_TYPE_FILE) {
        return FS_ERR_NOT_FILE;
    }

    return fs_set_file_data(id, data, len, 0);
}

int fs_append_file(const char *path, const char *data, size_t len)
{
    if (!data) return FS_ERR_INVALID;
    if (len == 0) len = fs_strlen(data);

    int id = fs_resolve(cwd, path, 0);
    if (id < 0) return id;
    if (nodes[id].type != FS_TYPE_FILE) return FS_ERR_NOT_FILE;
    if (!fs_ends_with_txt(nodes[id].name)) return FS_ERR_NOT_TXT;

    return fs_set_file_data(id, data, len, 1);
}

/* ---- Editor ---- */

static int edit_buf_append(const char *text, size_t len)
{
    size_t need = edit_len + len;
    if (need > FS_MAX_FILE_SIZE) return FS_ERR_NO_SPACE;

    if (need > edit_cap) {
        size_t new_cap = edit_cap ? edit_cap : 256;
        while (new_cap < need) new_cap *= 2;
        if (new_cap > FS_MAX_FILE_SIZE) new_cap = FS_MAX_FILE_SIZE;
        char *nbuf = (char *)kmalloc(new_cap);
        if (!nbuf) return FS_ERR_NO_SPACE;
        for (size_t i = 0; i < edit_len; i++) nbuf[i] = edit_buf[i];
        if (edit_buf) kfree(edit_buf);
        edit_buf = nbuf;
        edit_cap = new_cap;
    }

    for (size_t i = 0; i < len; i++) edit_buf[edit_len++] = text[i];
    return FS_OK;
}

static void edit_buf_free(void)
{
    if (edit_buf) kfree(edit_buf);
    edit_buf = NULL;
    edit_len = 0;
    edit_cap = 0;
    edit_pending_line_valid = 0;
    edit_target_line = 0;
}

/* Forward declaration */
static int edit_fetch_line(int n, char *out, size_t max);

int fs_edit_is_active(void)
{
    return edit_active;
}

const char *fs_edit_path(void)
{
    return edit_path;
}

int fs_edit_begin(const char *path)
{
    if (edit_active) return FS_ERR_INVALID;

    int id = fs_resolve(cwd, path, 0);
    if (id < 0) return id;  /* file not found — don't auto-create */

    if (nodes[id].type != FS_TYPE_FILE) return FS_ERR_NOT_FILE;
    if (!fs_ends_with_txt(nodes[id].name)) return FS_ERR_NOT_TXT;

    edit_buf_free();
    edit_active = 1;
    fs_strcpy(edit_path, path, FS_MAX_PATH);

    if (nodes[id].size > 0) {
        int err = edit_buf_append(nodes[id].data, nodes[id].size);
        if (err != FS_OK) {
            edit_active = 0;
            edit_path[0] = '\0';
            return err;
        }
    }
    return FS_OK;
}

int fs_edit_handle_line(const char *line, void (*emit)(const char *))
{
    if (!edit_active) return FS_ERR_INVALID;

    /* If a target line was set by ":e <n>" and this line is not a
       command, treat it as an inline replacement of that line. */
    if (edit_target_line > 0) {
        if (line[0] != ':') {
            int target = edit_target_line;
            edit_target_line = 0;
            /* Build a synthetic ":e <n> <text>" command */
            char syn[FS_CMD_TEXT_MAX];
            size_t sp = 0;
            char prefix[16];
            int pi = 0;
            prefix[pi++] = ':'; prefix[pi++] = 'e'; prefix[pi++] = ' ';
            int n = target;
            int rev[8], ri = 0;
            do { rev[ri++] = n % 10; n /= 10; } while (n > 0);
            while (ri > 0) prefix[pi++] = '0' + rev[--ri];
            prefix[pi++] = ' ';
            prefix[pi] = '\0';
            for (int i = 0; prefix[i] && sp < sizeof(syn) - 2; i++) syn[sp++] = prefix[i];
            for (int i = 0; line[i] && sp < sizeof(syn) - 2; i++) syn[sp++] = line[i];
            syn[sp] = '\0';
            return fs_edit_handle_line(syn, emit);
        }
        /* Command typed → cancel target */
        edit_target_line = 0;
    }

    if (fs_streq(line, ":save") || fs_streq(line, ":w")) {
        int err = fs_write_file(edit_path, edit_buf, edit_len);
        if (err != FS_OK) {
            emit(fs_strerror(err));
            return err;
        }
        emit("Saved.");
        edit_active = 0;
        edit_path[0] = '\0';
        edit_buf_free();
        return FS_OK;
    }

    if (fs_streq(line, ":q") || fs_streq(line, ":quit")) {
        emit("Edit cancelled.");
        edit_active = 0;
        edit_path[0] = '\0';
        edit_buf_free();
        return FS_OK;
    }

    if (fs_streq(line, ":p")) {
        if (edit_len == 0) {
            emit("(empty)");
        } else {
            int n = 1;
            char buf[81];
            size_t bp = 0;
            for (size_t i = 0; i < edit_len; i++) {
                char c = edit_buf[i];
                if (c == '\n') {
                    buf[bp] = '\0';
                    char num[8];
                    int ni = 0, v = n;
                    do { num[ni++] = '0' + (v % 10); v /= 10; } while (v > 0);
                    char out[96];
                    int oi = 0;
                    while (ni > 0) out[oi++] = num[--ni];
                    out[oi++] = ':'; out[oi++] = ' ';
                    for (int j = 0; buf[j] && oi < 94; j++) out[oi++] = buf[j];
                    out[oi] = '\0';
                    emit(out);
                    n++;
                    bp = 0;
                } else {
                    if (bp < 80) buf[bp++] = c;
                }
            }
        }
        return FS_OK;
    }

    if (line[0] == ':' && line[1] == 'e' && line[2] == ' ') {
        int ln = 0;
        const char *p = line + 3;
        while (*p >= '0' && *p <= '9') { ln = ln * 10 + (*p - '0'); p++; }
        int idx = ln - 1;
        if (idx < 0) { emit("Invalid line number"); return FS_ERR_INVALID; }
        if (edit_len == 0) { emit("File is empty"); return FS_ERR_INVALID; }
        int total = 0;
        for (size_t i = 0; i < edit_len; i++) { if (edit_buf[i] == '\n') total++; }
        if (edit_buf[edit_len - 1] != '\n') total++;
        if (idx >= total) { emit("Line number out of range"); return FS_ERR_INVALID; }

        /* :e <n> without text — fetch current line content into pending buffer */
        if (*p == '\0') {
            if (edit_fetch_line(ln, edit_pending_line, FS_MAX_PATH) == 0) {
                edit_pending_line_valid = 1;
                edit_target_line = ln;
            }
            return FS_OK;
        }

        if (*p != ' ') { emit("Usage: :e <n> [text]"); return FS_ERR_INVALID; }
        p++;

        char *old = edit_buf;
        size_t old_len = edit_len;
        edit_buf = NULL; edit_len = 0; edit_cap = 0;

        int cur = 0;
        size_t pos = 0;
        while (pos < old_len) {
            size_t start = pos;
            while (pos < old_len && old[pos] != '\n') pos++;
            if (cur == idx) {
                edit_buf_append(p, fs_strlen(p));
            } else {
                edit_buf_append(&old[start], pos - start);
            }
            if (pos < old_len && old[pos] == '\n') {
                edit_buf_append("\n", 1);
                pos++;
            }
            cur++;
        }

        kfree(old);
        emit("Line updated.");
        return FS_OK;
    }

    int err = edit_buf_append(line, fs_strlen(line));
    if (err != FS_OK) {
        emit(fs_strerror(err));
        return err;
    }
    err = edit_buf_append("\n", 1);
    if (err != FS_OK) emit(fs_strerror(err));
    return err;
}

void fs_edit_display_content(void (*emit)(const char *))
{
    char line[81];
    size_t lp = 0;

    if (!edit_active || !emit) return;

    if (edit_len == 0) {
        emit("(empty file)");
        return;
    }

    emit("--- file ---");
    for (size_t i = 0; i < edit_len; i++) {
        char c = edit_buf[i];
        if (c == '\n') {
            line[lp] = '\0';
            emit(line);
            lp = 0;
        } else {
            if (lp >= 80) {
                line[lp] = '\0';
                emit(line);
                lp = 0;
            }
            line[lp++] = c;
        }
    }
    if (lp > 0) {
        line[lp] = '\0';
        emit(line);
    }
}

const char *fs_edit_get_pending_line(void)
{
    if (edit_pending_line_valid) {
        edit_pending_line_valid = 0;
        return edit_pending_line;
    }
    return NULL;
}

/* Copy the content of line n (1-indexed) from edit_buf into out (max max chars).
   Returns 0 on success, -1 if line n doesn't exist. */
static int edit_fetch_line(int n, char *out, size_t max)
{
    if (edit_len == 0 || n < 1) return -1;
    int cur = 1;
    size_t pos = 0;
    while (pos < edit_len && cur < n) {
        if (edit_buf[pos] == '\n') cur++;
        pos++;
    }
    if (cur != n || pos >= edit_len) return -1;
    size_t oi = 0;
    while (pos < edit_len && edit_buf[pos] != '\n' && oi < max - 1) {
        out[oi++] = edit_buf[pos++];
    }
    out[oi] = '\0';
    return 0;
}

/* ---- Shell command parsing ---- */

static void skip_spaces(const char **p)
{
    while (**p == ' ') (*p)++;
}

static int parse_quoted_tail(const char *cmd, char *out, size_t max)
{
    skip_spaces(&cmd);
    if (*cmd != '"') return 0;
    cmd++;
    size_t i = 0;
    while (*cmd && *cmd != '"' && i < max - 1) out[i++] = *cmd++;
    if (*cmd != '"') return 0;
    out[i] = '\0';
    return 1;
}

static int parse_path_and_rest(const char *cmd, const char *verb, char *path, char *rest, size_t max)
{
    size_t vlen = fs_strlen(verb);
    if (fs_strncmp(cmd, verb, vlen) != 0) return 0;
    cmd += vlen;
    skip_spaces(&cmd);

    size_t pi = 0;
    while (*cmd && *cmd != ' ' && pi < FS_MAX_PATH - 1) path[pi++] = *cmd++;
    path[pi] = '\0';
    if (path[0] == '\0') return 0;

    skip_spaces(&cmd);
    if (!*cmd) {
        rest[0] = '\0';
        return 1;
    }
    return parse_quoted_tail(cmd, rest, max);
}

static void emit_err(void (*emit)(const char *), int err)
{
    char msg[64];
    size_t p = 0;
    const char *pfx = "Error: ";
    while (*pfx && p < 60) msg[p++] = *pfx++;
    const char *e = fs_strerror(err);
    while (*e && p < 63) msg[p++] = *e++;
    msg[p] = '\0';
    emit(msg);
}

void fs_show_help(void (*emit)(const char *))
{
    emit("Filesystem commands:");
    emit("ls <path>              - List directory");
    emit("cd <path>              - Change directory");
    emit("pwd                    - Print working directory");
    emit("mkdir <path>           - Create directory");
    emit("touch <file.txt>       - Create empty text file");
    emit("rm <path>              - Remove file or empty directory");
    emit("cat <file.txt>         - Print file contents");
    emit("write <file.txt> \"t\"  - Create/overwrite text file");
    emit("append <file.txt> \"t\" - Append text to file");
    emit("edit <file.txt>        - Edit existing .txt file");
}

int fs_handle_command(const char *command, void (*emit)(const char *))
{
    if (fs_streq(command, "help --fs")) {
        fs_show_help(emit);
        return 1;
    }
    if (fs_streq(command, "pwd")) {
        emit(fs_pwd());
        return 1;
    }

    char path[FS_MAX_PATH];
    char rest[FS_CMD_TEXT_MAX];

    if (fs_strncmp(command, "ls ", 3) == 0) {
        const char *arg = command + 3;
        skip_spaces(&arg);
        int err = fs_ls(arg, emit);
        if (err != FS_OK) emit_err(emit, err);
        return 1;
    }
    if (fs_strncmp(command, "dir ", 4) == 0) {
        const char *arg = command + 4;
        skip_spaces(&arg);
        int err = fs_ls(arg, emit);
        if (err != FS_OK) emit_err(emit, err);
        return 1;
    }

    if (fs_strncmp(command, "cd ", 3) == 0) {
        int err = fs_cd(command + 3);
        if (err != FS_OK) emit_err(emit, err);
        return 1;
    }

    if (fs_strncmp(command, "mkdir ", 6) == 0) {
        int err = fs_mkdir(command + 6);
        if (err != FS_OK) emit_err(emit, err);
        else emit("Directory created.");
        return 1;
    }

    if (fs_strncmp(command, "touch ", 6) == 0) {
        int err = fs_touch(command + 6);
        if (err != FS_OK) emit_err(emit, err);
        else emit("File created.");
        return 1;
    }

    if (fs_strncmp(command, "rm ", 3) == 0) {
        int err = fs_rm(command + 3);
        if (err != FS_OK) emit_err(emit, err);
        else emit("Removed.");
        return 1;
    }

    if (fs_strncmp(command, "cat ", 4) == 0) {
        int err = fs_cat(command + 4, emit);
        if (err != FS_OK) emit_err(emit, err);
        return 1;
    }

    if (parse_path_and_rest(command, "write", path, rest, sizeof(rest))) {
        int err = fs_write_file(path, rest, 0);
        if (err != FS_OK) emit_err(emit, err);
        else emit("Written.");
        return 1;
    }

    if (parse_path_and_rest(command, "append", path, rest, sizeof(rest))) {
        int err = fs_append_file(path, rest, 0);
        if (err != FS_OK) emit_err(emit, err);
        else emit("Appended.");
        return 1;
    }

    if (fs_strncmp(command, "edit ", 5) == 0) {
        int err = fs_edit_begin(command + 5);
        if (err != FS_OK) {
            emit_err(emit, err);
        } else {
            emit("Edit mode. Commands: :save/:w, :q, :p, :e <n> [text], :e <n> (load line)");
            emit(fs_edit_path());
        }
        return 1;
    }

    return 0;
}
