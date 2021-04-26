
UINT64 strlen(const char* str) {
    UINT64 len = 0;
    while (*str++)
        len++;
    return len;
}
