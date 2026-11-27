#include "http.h"
#include "config.h"

#include <windows.h>
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REQUEST_HEADER_CAP 16384
#define REQUEST_TARGET_CAP 2048
#define IO_BUFFER_CAP 16384

typedef struct {
    char method[8];
    char target[REQUEST_TARGET_CAP];
    char version[16];
    char data[REQUEST_HEADER_CAP + 1];
    size_t body_offset;
    size_t body_size;
    unsigned long long content_length;
    int has_content_length;
    int expect_continue;
} http_request;

static int send_all(SOCKET socket, const void *data, size_t length)
{
    const char *cursor = (const char *)data;

    while (length) {
        int amount = length > IO_BUFFER_CAP ? IO_BUFFER_CAP : (int)length;
        int sent = send(socket, cursor, amount, 0);

        if (sent <= 0) {
            return 0;
        }
        cursor += sent;
        length -= (size_t)sent;
    }
    return 1;
}

static int send_text(SOCKET socket, const char *text)
{
    return send_all(socket, text, strlen(text));
}

static int send_format(SOCKET socket, const char *format, ...)
{
    char buffer[4096];
    va_list arguments;
    int length;

    va_start(arguments, format);
    length = vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);
    if (length < 0 || (size_t)length >= sizeof(buffer)) {
        return 0;
    }
    return send_all(socket, buffer, (size_t)length);
}

static int send_status(SOCKET socket, int code, const char *reason, const char *message)
{
    size_t length = strlen(message);

    if (!send_format(socket,
            "HTTP/1.1 %d %s\r\n"
            "Content-Type: text/plain; charset=utf-8\r\n"
            "Content-Length: %lu\r\n"
            "Connection: close\r\n\r\n",
            code, reason, (unsigned long)length)) {
        return 0;
    }
    return send_all(socket, message, length);
}

static char *trim(char *text)
{
    char *end;

    while (isspace((unsigned char)*text)) {
        ++text;
    }
    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) {
        --end;
    }
    *end = '\0';
    return text;
}

static int parse_length(const char *text, unsigned long long *result)
{
    unsigned long long value = 0;

    if (!isdigit((unsigned char)*text)) {
        return 0;
    }
    while (isdigit((unsigned char)*text)) {
        unsigned int digit = (unsigned int)(*text - '0');

        if (value > (ULLONG_MAX - digit) / 10) {
            return 0;
        }
        value = value * 10 + digit;
        ++text;
    }
    if (*text) {
        return 0;
    }
    *result = value;
    return 1;
}

static int read_request(SOCKET socket, http_request *request)
{
    size_t used = 0;
    size_t header_size;
    char *finish;
    char *line;
    char *cursor;

    memset(request, 0, sizeof(*request));
    for (;;) {
        int count;

        if (used == REQUEST_HEADER_CAP) {
            return -2;
        }
        count = recv(socket, request->data + used, REQUEST_HEADER_CAP - (int)used, 0);
        if (count <= 0) {
            return used ? -1 : 0;
        }
        used += (size_t)count;
        request->data[used] = '\0';
        finish = strstr(request->data, "\r\n\r\n");
        if (finish) {
            break;
        }
    }
    header_size = (size_t)(finish - request->data) + 4;
    line = strstr(request->data, "\r\n");
    if (!line) {
        return -1;
    }
    *finish = '\0';
    cursor = line == finish ? finish : line + 2;
    *line = '\0';
    if (sscanf(request->data, "%7s %2047s %15s", request->method,
            request->target, request->version) != 3) {
        return -1;
    }
    if (strcmp(request->version, "HTTP/1.0") && strcmp(request->version, "HTTP/1.1")) {
        return -1;
    }
    while (*cursor) {
        char *next = strstr(cursor, "\r\n");
        char *colon;
        char *name;
        char *value;

        if (next) {
            *next = '\0';
        }
        colon = strchr(cursor, ':');
        if (!colon) {
            return -1;
        }
        *colon = '\0';
        name = trim(cursor);
        value = trim(colon + 1);
        if (!*name) {
            return -1;
        }
        if (_stricmp(name, "Content-Length") == 0) {
            if (request->has_content_length ||
                    !parse_length(value, &request->content_length)) {
                return -1;
            }
            request->has_content_length = 1;
        } else if (_stricmp(name, "Expect") == 0) {
            request->expect_continue = _stricmp(value, "100-continue") == 0;
        } else if (_stricmp(name, "Transfer-Encoding") == 0) {
            return -1;
        }
        if (!next) {
            break;
        }
        cursor = next + 2;
    }
    request->body_offset = header_size;
    request->body_size = used - header_size;
    return 1;
}

static int hex_value(char value)
{
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    return -1;
}

static int decode_target(const char *target, char *decoded, size_t capacity)
{
    size_t input = 0;
    size_t output = 0;
    size_t segment = 0;

    if (target[0] != '/') {
        return 0;
    }
    while (target[input] && target[input] != '?') {
        int value = (unsigned char)target[input++];

        if (value == '%') {
            int high;
            int low;

            if (!target[input] || !target[input + 1]) {
                return 0;
            }
            high = hex_value(target[input]);
            low = hex_value(target[input + 1]);
            if (high < 0 || low < 0) {
                return 0;
            }
            value = high * 16 + low;
            input += 2;
        }
        if (value == 0 || value < 32 || value == 127 || value == ':' || value == '\\') {
            return 0;
        }
        if (value == '/' && output > segment) {
            if (output - segment == 2 && decoded[segment] == '.' && decoded[segment + 1] == '.') {
                return 0;
            }
        }
        if (value == '/' && output && decoded[output - 1] == '/') {
            continue;
        }
        if (output + 1 >= capacity) {
            return 0;
        }
        decoded[output++] = (char)value;
        if (value == '/') {
            segment = output;
        }
    }
    if (output - segment == 2 && decoded[segment] == '.' && decoded[segment + 1] == '.') {
        return 0;
    }
    decoded[output] = '\0';
    return 1;
}

static int map_path(const char *root, const char *decoded, char *path, size_t capacity)
{
    char joined[SERVER_PATH_CAP + REQUEST_TARGET_CAP];
    size_t root_length = strlen(root);
    size_t path_length;
    char *cursor;
    int written;
    DWORD length;

    if (root_length && root[root_length - 1] == '\\') {
        written = snprintf(joined, sizeof(joined), "%s%s", root, decoded + 1);
    } else {
        written = snprintf(joined, sizeof(joined), "%s\\%s", root, decoded + 1);
    }
    if (written < 0 || (size_t)written >= sizeof(joined)) {
        return 0;
    }
    for (cursor = joined; *cursor; ++cursor) {
        if (*cursor == '/') {
            *cursor = '\\';
        }
    }
    length = GetFullPathNameA(joined, (DWORD)capacity, path, NULL);
    if (!length || length >= capacity) {
        return 0;
    }
    path_length = strlen(path);
    if (path_length < root_length || _strnicmp(root, path, root_length)) {
        return 0;
    }
    if (root[root_length - 1] != '\\' && path[root_length] &&
            path[root_length] != '\\') {
        return 0;
    }
    return 1;
}

static int url_encode(const char *text, char *encoded, size_t capacity, int keep_slash)
{
    static const char digits[] = "0123456789ABCDEF";
    size_t output = 0;

    while (*text) {
        unsigned char value = (unsigned char)*text++;
        int plain = (value >= 'a' && value <= 'z') ||
                    (value >= 'A' && value <= 'Z') ||
                    (value >= '0' && value <= '9') ||
                    value == '-' || value == '_' || value == '.' || value == '~' ||
                    (keep_slash && value == '/');

        if (plain) {
            if (output + 1 >= capacity) {
                return 0;
            }
            encoded[output++] = (char)value;
        } else {
            if (output + 3 >= capacity) {
                return 0;
            }
            encoded[output++] = '%';
            encoded[output++] = digits[value >> 4];
            encoded[output++] = digits[value & 15];
        }
    }
    if (output >= capacity) {
        return 0;
    }
    encoded[output] = '\0';
    return 1;
}

static int send_html(SOCKET socket, const char *text)
{
    char buffer[512];
    size_t used = 0;

    while (*text) {
        const char *entity = NULL;
        char value = *text++;
        size_t length;

        switch (value) {
        case '&': entity = "&amp;"; break;
        case '<': entity = "&lt;"; break;
        case '>': entity = "&gt;"; break;
        case '"': entity = "&quot;"; break;
        case '\'': entity = "&#39;"; break;
        default: break;
        }
        if (!entity) {
            if (used + 1 >= sizeof(buffer) && !send_all(socket, buffer, used)) {
                return 0;
            }
            if (used + 1 >= sizeof(buffer)) {
                used = 0;
            }
            buffer[used++] = value;
            continue;
        }
        length = strlen(entity);
        if (used + length > sizeof(buffer)) {
            if (!send_all(socket, buffer, used)) {
                return 0;
            }
            used = 0;
        }
        memcpy(buffer + used, entity, length);
        used += length;
    }
    return !used || send_all(socket, buffer, used);
}

static const char *content_type(const char *path)
{
    const char *extension = strrchr(path, '.');

    if (!extension) {
        return "application/octet-stream";
    }
    if (!_stricmp(extension, ".html") || !_stricmp(extension, ".htm")) {
        return "text/html; charset=utf-8";
    }
    if (!_stricmp(extension, ".css")) {
        return "text/css; charset=utf-8";
    }
    if (!_stricmp(extension, ".js")) {
        return "application/javascript; charset=utf-8";
    }
    if (!_stricmp(extension, ".json") || !_stricmp(extension, ".txt") ||
            !_stricmp(extension, ".md") || !_stricmp(extension, ".c") ||
            !_stricmp(extension, ".h") || !_stricmp(extension, ".ini")) {
        return "text/plain; charset=utf-8";
    }
    if (!_stricmp(extension, ".png")) {
        return "image/png";
    }
    if (!_stricmp(extension, ".jpg") || !_stricmp(extension, ".jpeg")) {
        return "image/jpeg";
    }
    if (!_stricmp(extension, ".gif")) {
        return "image/gif";
    }
    if (!_stricmp(extension, ".svg")) {
        return "image/svg+xml";
    }
    if (!_stricmp(extension, ".ico")) {
        return "image/x-icon";
    }
    if (!_stricmp(extension, ".pdf")) {
        return "application/pdf";
    }
    return "application/octet-stream";
}

static void serve_file(SOCKET socket, const char *path)
{
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    LARGE_INTEGER size;
    char buffer[IO_BUFFER_CAP];

    if (file == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
            send_status(socket, 404, "Not Found", "File not found.\n");
        } else {
            send_status(socket, 403, "Forbidden", "Cannot open the file.\n");
        }
        return;
    }
    if (!GetFileSizeEx(file, &size)) {
        CloseHandle(file);
        send_status(socket, 500, "Internal Server Error", "Cannot read the file size.\n");
        return;
    }
    if (!send_format(socket,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %llu\r\n"
            "Connection: close\r\n\r\n",
            content_type(path), (unsigned long long)size.QuadPart)) {
        CloseHandle(file);
        return;
    }
    for (;;) {
        DWORD count = 0;

        if (!ReadFile(file, buffer, sizeof(buffer), &count, NULL) || !count) {
            break;
        }
        if (!send_all(socket, buffer, count)) {
            break;
        }
    }
    CloseHandle(file);
}

static void send_redirect(SOCKET socket, const char *decoded)
{
    char location[REQUEST_TARGET_CAP * 3 + 2];
    size_t length;

    if (!url_encode(decoded, location, sizeof(location), 1)) {
        send_status(socket, 414, "URI Too Long", "The request path is too long.\n");
        return;
    }
    length = strlen(location);
    location[length] = '/';
    location[length + 1] = '\0';
    send_format(socket,
        "HTTP/1.1 301 Moved Permanently\r\n"
        "Location: %s\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n\r\n", location);
}

static void serve_index(SOCKET socket, const char *path, const char *decoded)
{
    char search[SERVER_PATH_CAP + 3];
    char base[REQUEST_TARGET_CAP * 3 + 1];
    WIN32_FIND_DATAA data;
    HANDLE find;
    int valid;
    int written = snprintf(search, sizeof(search), "%s\\*", path);

    if (written < 0 || (size_t)written >= sizeof(search) ||
            !url_encode(decoded, base, sizeof(base), 1)) {
        send_status(socket, 414, "URI Too Long", "The request path is too long.\n");
        return;
    }
    find = FindFirstFileA(search, &data);
    valid = find != INVALID_HANDLE_VALUE;
    if (!valid && GetLastError() != ERROR_FILE_NOT_FOUND) {
        send_status(socket, 403, "Forbidden", "Cannot list the directory.\n");
        return;
    }
    send_text(socket,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Connection: close\r\n\r\n");
    send_text(socket,
        "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<title>Index of ");
    send_html(socket, decoded);
    send_text(socket,
        "</title><style>"
        "body{font:16px Arial,sans-serif;max-width:960px;margin:2rem auto;padding:0 1rem;color:#202124}"
        "a{color:#0969da;text-decoration:none}a:hover{text-decoration:underline}"
        "form{display:flex;gap:.6rem;align-items:center;margin:1.5rem 0;flex-wrap:wrap}"
        "button{padding:.45rem 1rem;cursor:pointer}progress{width:240px;height:1.2rem}"
        "table{width:100%;border-collapse:collapse;margin-top:1rem}th,td{text-align:left;padding:.55rem;border-bottom:1px solid #ddd}"
        "td:last-child,th:last-child{text-align:right;white-space:nowrap}#status{margin-left:.7rem}"
        "</style></head><body><h1>Index of ");
    send_html(socket, decoded);
    send_text(socket,
        "</h1><form id=\"upload-form\"><input id=\"file\" type=\"file\" required>"
        "<button type=\"submit\">Upload</button></form>"
        "<progress id=\"progress\" value=\"0\" max=\"100\"></progress><span id=\"status\"></span>");
    if (strcmp(decoded, "/")) {
        send_text(socket, "<p><a href=\"../\">../</a></p>");
    }
    send_text(socket, "<table><tr><th>Name</th><th>Size</th></tr>");
    if (valid) {
        do {
            char encoded_name[MAX_PATH * 3 + 1];
            int directory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

            if (!strcmp(data.cFileName, ".") || !strcmp(data.cFileName, "..")) {
                continue;
            }
            if (!url_encode(data.cFileName, encoded_name, sizeof(encoded_name), 0)) {
                continue;
            }
            send_text(socket, "<tr><td><a href=\"");
            send_text(socket, base);
            send_text(socket, encoded_name);
            if (directory) {
                send_text(socket, "/");
            }
            send_text(socket, "\">");
            send_html(socket, data.cFileName);
            if (directory) {
                send_text(socket, "/");
            }
            send_text(socket, "</a></td><td>");
            if (directory) {
                send_text(socket, "Directory");
            } else {
                unsigned long long size = ((unsigned long long)data.nFileSizeHigh << 32) |
                                          data.nFileSizeLow;
                send_format(socket, "%llu bytes", size);
            }
            send_text(socket, "</td></tr>");
        } while (FindNextFileA(find, &data));
        FindClose(find);
    }
    send_text(socket,
        "</table><script>"
        "var form=document.getElementById('upload-form'),picker=document.getElementById('file'),"
        "bar=document.getElementById('progress'),note=document.getElementById('status');"
        "function units(n){var u=['B','KB','MB','GB'],i=0;while(n>=1024&&i<3){n/=1024;i++;}"
        "return n.toFixed(i?1:0)+' '+u[i];}"
        "form.onsubmit=function(e){e.preventDefault();var f=picker.files[0];if(!f)return;"
        "var x=new XMLHttpRequest(),start=Date.now();x.open('PUT',encodeURIComponent(f.name));"
        "x.upload.onprogress=function(e){if(!e.lengthComputable)return;var elapsed=Math.max((Date.now()-start)/1000,.001);"
        "bar.value=100*e.loaded/e.total;note.textContent=bar.value.toFixed(1)+'%  '+units(e.loaded/elapsed)+'/s';};"
        "x.onload=function(){if(x.status>=200&&x.status<300){bar.value=100;note.textContent='Upload complete';"
        "setTimeout(function(){location.reload();},600);}else{note.textContent='Upload failed: '+x.status+' '+x.responseText;}};"
        "x.onerror=function(){note.textContent='Network error';};x.send(f);};"
        "</script></body></html>");
}

static int write_file(HANDLE file, const char *data, DWORD length)
{
    DWORD written = 0;

    while (written < length) {
        DWORD count = 0;

        if (!WriteFile(file, data + written, length - written, &count, NULL) || !count) {
            return 0;
        }
        written += count;
    }
    return 1;
}

static int receive_file(SOCKET socket, http_request *request, HANDLE file)
{
    unsigned long long remaining = request->content_length;
    size_t available = request->body_size;
    size_t offset = request->body_offset;
    char buffer[IO_BUFFER_CAP];

    while (available && remaining) {
        size_t amount = available;
        if ((unsigned long long)amount > remaining) {
            amount = (size_t)remaining;
        }
        if (!write_file(file, request->data + offset, (DWORD)amount)) {
            return 0;
        }
        offset += amount;
        available -= amount;
        remaining -= amount;
    }
    while (remaining) {
        DWORD wanted = remaining > sizeof(buffer) ? sizeof(buffer) : (DWORD)remaining;
        int count = recv(socket, buffer, (int)wanted, 0);

        if (count <= 0 || !write_file(file, buffer, (DWORD)count)) {
            return 0;
        }
        remaining -= (unsigned int)count;
    }
    return 1;
}

static void serve_put(SOCKET socket, http_request *request, const char *path, const char *decoded)
{
    DWORD attributes = GetFileAttributesA(path);
    int existed = attributes != INVALID_FILE_ATTRIBUTES;
    HANDLE file;
    size_t length = strlen(decoded);

    if (!length || decoded[length - 1] == '/') {
        send_status(socket, 400, "Bad Request", "Specify a file name.\n");
        return;
    }
    if (existed && (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        send_status(socket, 409, "Conflict", "A directory has that name.\n");
        return;
    }
    file = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error == ERROR_PATH_NOT_FOUND || error == ERROR_FILE_NOT_FOUND) {
            send_status(socket, 404, "Not Found", "The destination directory does not exist.\n");
        } else {
            send_status(socket, 403, "Forbidden", "Cannot create the file.\n");
        }
        return;
    }
    if (request->expect_continue && request->content_length > request->body_size &&
            !send_text(socket, "HTTP/1.1 100 Continue\r\n\r\n")) {
        CloseHandle(file);
        DeleteFileA(path);
        return;
    }
    if (!receive_file(socket, request, file)) {
        CloseHandle(file);
        DeleteFileA(path);
        send_status(socket, 400, "Bad Request", "The upload was incomplete.\n");
        return;
    }
    CloseHandle(file);
    if (existed) {
        send_status(socket, 200, "OK", "Updated.\n");
    } else {
        send_status(socket, 201, "Created", "Created.\n");
    }
}

static void serve_get(SOCKET socket, const char *path, const char *decoded)
{
    DWORD attributes = GetFileAttributesA(path);
    size_t length = strlen(decoded);

    if (attributes == INVALID_FILE_ATTRIBUTES) {
        send_status(socket, 404, "Not Found", "File not found.\n");
    } else if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
        if (!length || decoded[length - 1] != '/') {
            send_redirect(socket, decoded);
        } else {
            serve_index(socket, path, decoded);
        }
    } else {
        serve_file(socket, path);
    }
}

void handle_client(SOCKET client, const char *root)
{
    http_request request;
    char decoded[REQUEST_TARGET_CAP];
    char path[SERVER_PATH_CAP];
    int result = read_request(client, &request);

    if (result == 0) {
        return;
    }
    if (result == -2) {
        send_status(client, 431, "Request Header Fields Too Large", "The request headers are too large.\n");
        return;
    }
    if (result < 0) {
        send_status(client, 400, "Bad Request", "Malformed HTTP request.\n");
        return;
    }
    if (strcmp(request.method, "GET") && strcmp(request.method, "PUT")) {
        send_status(client, 405, "Method Not Allowed", "Only GET and PUT are supported.\n");
        return;
    }
    if (!decode_target(request.target, decoded, sizeof(decoded)) ||
            !map_path(root, decoded, path, sizeof(path))) {
        send_status(client, 400, "Bad Request", "Invalid request path.\n");
        return;
    }
    if (!strcmp(request.method, "GET")) {
        serve_get(client, path, decoded);
        return;
    }
    if (!request.has_content_length) {
        send_status(client, 411, "Length Required", "Content-Length is required.\n");
        return;
    }
    serve_put(client, &request, path, decoded);
}
