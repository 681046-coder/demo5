/* ระบบจัดสรรสินค้าคอมพิวเตอร์ — HTTP server เขียนด้วยภาษา C ล้วน (POSIX, ไม่ใช้ไลบรารีเสริม)
 * Build: make   Run: ./server [port]   เปิด http://localhost:8080
 * API:  GET /api/items | POST /api/items | POST /api/adjust | POST /api/delete
 * ข้อมูลเก็บในไฟล์ data.tsv (id \t หมวด \t ชื่อ \t จำนวน \t จุดแจ้งเตือน) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_ITEMS 1000
#define DATA_FILE "data.tsv"

typedef struct { int id; char cat[32]; char name[96]; int qty; int min; } Item;
static Item items[MAX_ITEMS];
static int n_items = 0, next_id = 1;

static void clean(char *s) { for (; *s; s++) if (*s=='\t'||*s=='\n'||*s=='\r'||*s=='"'||*s=='\\') *s=' '; }

static void save(void) {
    FILE *f = fopen(DATA_FILE, "w"); if (!f) return;
    for (int i = 0; i < n_items; i++)
        fprintf(f, "%d\t%s\t%s\t%d\t%d\n", items[i].id, items[i].cat, items[i].name, items[i].qty, items[i].min);
    fclose(f);
}

static void load(void) {
    FILE *f = fopen(DATA_FILE, "r"); if (!f) return;
    char line[512];
    while (n_items < MAX_ITEMS && fgets(line, sizeof line, f)) {
        Item *it = &items[n_items];
        if (sscanf(line, "%d\t%31[^\t]\t%95[^\t]\t%d\t%d", &it->id, it->cat, it->name, &it->qty, &it->min) == 5) {
            if (it->id >= next_id) next_id = it->id + 1;
            n_items++;
        }
    }
    fclose(f);
}

/* ดึงค่า key=value จาก body แบบ x-www-form-urlencoded (รองรับ %XX และ +) */
static void form_get(const char *body, const char *key, char *out, size_t sz) {
    out[0] = 0; size_t kl = strlen(key); const char *p = body;
    while (p && *p) {
        if (!strncmp(p, key, kl) && p[kl] == '=') {
            p += kl + 1; size_t o = 0;
            while (*p && *p != '&' && o + 1 < sz) {
                if (*p == '+') out[o++] = ' ';
                else if (*p == '%' && p[1] && p[2]) { char h[3] = {p[1], p[2], 0}; out[o++] = (char)strtol(h, NULL, 16); p += 2; }
                else out[o++] = *p;
                p++;
            }
            out[o] = 0; return;
        }
        p = strchr(p, '&'); if (p) p++;
    }
}

static void send_all(int fd, const char *buf, size_t len) {
    while (len) { ssize_t w = write(fd, buf, len); if (w <= 0) return; buf += w; len -= (size_t)w; }
}

static void respond(int fd, int code, const char *type, const char *body, size_t len) {
    char h[256];
    int hl = snprintf(h, sizeof h, "HTTP/1.1 %d OK\r\nContent-Type: %s; charset=utf-8\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n", code, type, len);
    send_all(fd, h, (size_t)hl); send_all(fd, body, len);
}

static void send_items(int fd) {
    static char out[MAX_ITEMS * 220 + 16]; size_t o = 0;
    o += (size_t)snprintf(out + o, sizeof out - o, "[");
    for (int i = 0; i < n_items; i++) {
        Item *it = &items[i];
        o += (size_t)snprintf(out + o, sizeof out - o,
            "%s{\"id\":%d,\"cat\":\"%s\",\"name\":\"%s\",\"qty\":%d,\"min\":%d}",
            i ? "," : "", it->id, it->cat, it->name, it->qty, it->min);
    }
    o += (size_t)snprintf(out + o, sizeof out - o, "]");
    respond(fd, 200, "application/json", out, o);
}

static Item *find(int id) { for (int i = 0; i < n_items; i++) if (items[i].id == id) return &items[i]; return NULL; }

static void serve_file(int fd, const char *path, const char *type) {
    FILE *f = fopen(path, "rb");
    if (!f) { respond(fd, 404, "text/plain", "Not found", 9); return; }
    fseek(f, 0, SEEK_END); long len = ftell(f); rewind(f);
    char *buf = malloc((size_t)len + 1);
    if (!buf) { fclose(f); return; }
    size_t got = fread(buf, 1, (size_t)len, f); fclose(f);
    respond(fd, 200, type, buf, got); free(buf);
}

static void handle(int fd) {
    char req[8192]; ssize_t r = read(fd, req, sizeof req - 1); if (r <= 0) return;
    req[r] = 0;
    char method[8], path[128]; if (sscanf(req, "%7s %127s", method, path) != 2) return;
    char *body = strstr(req, "\r\n\r\n"); body = body ? body + 4 : "";
    char a[128], b[128], c[128], d[128];

    if (!strcmp(method, "GET") && !strcmp(path, "/api/items")) { send_items(fd); return; }

    if (!strcmp(method, "POST") && !strcmp(path, "/api/items")) {
        if (n_items >= MAX_ITEMS) { respond(fd, 400, "text/plain", "full", 4); return; }
        Item *it = &items[n_items];
        form_get(body, "cat", it->cat, sizeof it->cat); form_get(body, "name", it->name, sizeof it->name);
        form_get(body, "qty", a, sizeof a); form_get(body, "min", b, sizeof b);
        clean(it->cat); clean(it->name);
        if (!it->name[0] || !it->cat[0]) { respond(fd, 400, "text/plain", "bad", 3); return; }
        it->qty = atoi(a) < 0 ? 0 : atoi(a); it->min = atoi(b) < 0 ? 0 : atoi(b);
        it->id = next_id++; n_items++; save(); send_items(fd); return;
    }

    if (!strcmp(method, "POST") && !strcmp(path, "/api/adjust")) {   /* delta บวก=รับเข้า ลบ=จ่ายออก */
        form_get(body, "id", c, sizeof c); form_get(body, "delta", d, sizeof d);
        Item *it = find(atoi(c));
        if (it) { it->qty += atoi(d); if (it->qty < 0) it->qty = 0; save(); }
        send_items(fd); return;
    }

    if (!strcmp(method, "POST") && !strcmp(path, "/api/delete")) {
        form_get(body, "id", c, sizeof c); int id = atoi(c);
        for (int i = 0; i < n_items; i++) if (items[i].id == id) { items[i] = items[--n_items]; break; }
        save(); send_items(fd); return;
    }

    if (!strcmp(method, "GET") && (!strcmp(path, "/") || !strcmp(path, "/index.html"))) {
        serve_file(fd, "public/index.html", "text/html"); return;
    }
    respond(fd, 404, "text/plain", "Not found", 9);
}

int main(int argc, char **argv) {
    int port = argc > 1 ? atoi(argv[1]) : 8080;
    signal(SIGPIPE, SIG_IGN);
    load();
    int s = socket(AF_INET, SOCK_STREAM, 0), on = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_port = htons((unsigned short)port), .sin_addr.s_addr = htonl(INADDR_ANY) };
    if (bind(s, (struct sockaddr *)&addr, sizeof addr) < 0 || listen(s, 16) < 0) { perror("bind/listen"); return 1; }
    printf("เปิดที่ http://localhost:%d\n", port);
    for (;;) { int fd = accept(s, NULL, NULL); if (fd < 0) continue; handle(fd); close(fd); }
}
