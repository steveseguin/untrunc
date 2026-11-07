// Standalone FileRead regression tests. Only logging and endian helpers are
// provided here so the production file.cpp can run without FFmpeg decoders.
#include "file.h"
#include <cstdio>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <unistd.h>

LogMode g_log_mode = E;
bool g_noise_buffer_active = false, g_is_gui = false;
uint g_num_w2 = 0;
void cutNoiseBuffer(bool) {}
uint32_t swap32(uint32_t x) { return __builtin_bswap32(x); }
uint64_t swap64(uint64_t x) { return __builtin_bswap64(x); }

void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

struct TempFile {
    std::string path;
    explicit TempFile(size_t size) {
        char name[] = "/tmp/untrunc-file-read-XXXXXX";
        int fd = mkstemp(name);
        check(fd >= 0, "mkstemp failed");
        path = name;
        FILE* f = fdopen(fd, "wb");
        check(f != nullptr, "fdopen failed");
        std::vector<uchar> data(65536);
        for (size_t offset = 0; offset < size;) {
            size_t n = std::min(data.size(), size - offset);
            for (size_t i = 0; i < n; ++i) data[i] = (offset + i) % 251;
            check(fwrite(data.data(), 1, n, f) == n, "fixture write failed");
            offset += n;
        }
        check(fclose(f) == 0, "fixture close failed");
    }
    ~TempFile() { remove(path.c_str()); }
};

void compareRead(FileRead& reader, size_t size, size_t n) {
    off_t start = reader.pos();
    size_t available = start >= reader.length() ? 0 : reader.length() - start;
    size_t bytes = std::min(size * n, available);
    std::vector<uchar> dest(size * n + 1, 255);
    size_t result = reader.readBuffer(dest.data(), size, n);
    check(result == (size ? bytes / size : 0), "wrong complete-item count");
    check(reader.pos() == start + off_t(bytes), "wrong position");
    for (size_t i = 0; i < bytes; ++i)
        check(dest[i] == (start + i) % 251, "wrong file byte");
    for (size_t i = bytes; i < dest.size(); ++i)
        check(dest[i] == 255, "wrote beyond file data");
}

void expectReadFailure(const std::function<void(FileRead&)>& operation,
                       size_t length) {
    TempFile file(length);
    FileRead reader(file.path);
    bool threw = false;
    try { operation(reader); } catch (const char*) { threw = true; }
    check(threw, "truncated typed read did not fail");
    check(reader.pos() == off_t(length), "short read passed EOF");
}

int main(int argc, char**) {
    unsigned passed = 0, failed = 0;
    auto test = [&](const char* name, const std::function<void()>& fn) {
        try { fn(); ++passed; std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& e) {
            ++failed; std::cerr << "FAIL " << name << ": " << e.what() << '\n';
        }
    };
    test("empty file", [] {
        TempFile f(0); FileRead r(f.path); compareRead(r, 1, 8);
    });
    test("short byte read and repeated EOF", [] {
        TempFile f(3); FileRead r(f.path);
        compareRead(r, 1, 8); compareRead(r, 1, 8);
    });
    test("partial multi-byte item", [] {
        TempFile f(7); FileRead r(f.path); compareRead(r, 4, 3);
    });
    test("truncated int32", [] {
        expectReadFailure([](FileRead& r) { r.readInt(); }, 3);
    });
    test("truncated int64", [] {
        expectReadFailure([](FileRead& r) { r.readInt64(); }, 7);
    });
    test("truncated string", [] {
        expectReadFailure([](FileRead& r) { r.getString(8); }, 3);
    });
    test("truncated vector", [] {
        expectReadFailure([](FileRead& r) { r.read(8); }, 3);
    });
    test("exact typed reads", [] {
        TempFile f(12); FileRead r(f.path);
        check(r.readInt() == 0x00010203u, "int32 content");
        check(r.readInt64() == 0x0405060708090a0bLL, "int64 content");
        check(r.atEnd(), "exact read must reach EOF");
    });
    test("buffer refill, direct read and seek boundaries", [] {
        const size_t B = 15 * (1 << 20);
        TempFile f(3 * B + 29); FileRead r(f.path);
        compareRead(r, 1, 2 * B + 7);
        for (size_t offset : {B - 3, 2 * B - 3, 3 * B + 26, 3 * B + 29}) {
            r.seek(offset); compareRead(r, 4, 8);
        }
        r.seek(0); compareRead(r, 1, 3 * B + 35);
        r.seek(5); compareRead(r, 1, 4);
    });
    test("pointer-prefetch then short read", [] {
        const size_t B = 15 * (1 << 20);
        TempFile f(B + 17); FileRead r(f.path);
        r.getPtrAt(B - 3, 8);
        compareRead(r, 1, 32);
    });
    test("seek beyond EOF", [] {
        TempFile f(3); FileRead r(f.path);
        r.seek(10); compareRead(r, 1, 2);
    });
    test("large sparse file", [] {
        TempFile f(0);
        const off_t length = (off_t(1) << 32) + 3;
        check(truncate(f.path.c_str(), length) == 0, "sparse fixture failed");
        FileRead r(f.path);
        uchar dest[8];
        check(r.readBuffer(dest, 1, sizeof dest) == sizeof dest,
              "large remaining length was truncated");
        r.seek(length - 3);
        std::memset(dest, 255, sizeof dest);
        check(r.readBuffer(dest, 1, sizeof dest) == 3, "large file EOF count");
        check(dest[0] == 0 && dest[1] == 0 && dest[2] == 0 && dest[3] == 255,
              "large file EOF bytes");
        check(r.pos() == length, "large file EOF position");
    });
    // The original implementation divides by zero. Skip only when comparing
    // baseline failures; the normal test invocation includes these cases.
    if (argc == 1) test("zero-sized operations", [] {
        TempFile f(3); FileRead r(f.path);
        compareRead(r, 0, 8); compareRead(r, 4, 0);
        check(r.readBuffer(nullptr, 0, 8) == 0, "zero size");
        check(r.readBuffer(nullptr, 4, 0) == 0, "zero count");
    });
    std::cout << passed << " passed, " << failed << " failed\n";
    return failed ? 1 : 0;
}
