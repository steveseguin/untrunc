// Standalone offset regressions for the actual production MutualPattern.
#include "mutual_pattern.h"
#include <functional>
#include <stdexcept>

// Only unrelated common.cpp symbols are supplied here, avoiding FFmpeg.
// Fail if a test accidentally enters one of these unrelated helper paths.
LogMode g_log_mode = E;
bool g_noise_buffer_active = false, g_is_gui = false;
uint g_num_w2 = 0;
void cutNoiseBuffer(bool) { throw std::logic_error("unexpected logging"); }
double calcEntropy(const std::vector<uchar>&) {
    throw std::logic_error("unexpected entropy calculation");
}
std::string mkHexStr(const uchar*, int, int) {
    throw std::logic_error("unexpected formatting");
}
std::mt19937& getRandomGenerator() {
    throw std::logic_error("unexpected random generation");
}

void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

int main() {
    unsigned passed = 0, failed = 0;
    auto test = [&](const char* name, const std::function<void()>& fn) {
        try { fn(); ++passed; std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& e) {
            ++failed; std::cerr << "FAIL " << name << ": " << e.what() << '\n';
        }
    };
    const std::vector<uchar> zeros(4, 0);

    test("zero transition with a fully shared reference", [&] {
        std::vector<uchar> a(32, 0);
        MutualPattern p(a, a);
        check(p.hasPattern(16, zeros), "shared zero boundary not found");
    });
    test("zero transition after variable prefix bytes", [&] {
        for (unsigned i = 0; i < 4; ++i) {
            std::vector<uchar> a(32, 0), b = a;
            b[i] = 1;
            MutualPattern p(a, b);
            check(p.hasPattern(16, zeros), "unrelated prefix mask hid boundary");
        }
    });
    test("zero transition after a fully variable first half", [&] {
        std::vector<uchar> a(32, 0), b = a;
        std::fill(b.begin(), b.begin() + 16, 1);
        MutualPattern p(a, b);
        check(p.hasPattern(16, zeros), "variable previous chunk hid boundary");
    });
    test("non-mutual boundary bytes are not a zero transition", [&] {
        for (unsigned i = 16; i < 20; ++i) {
            std::vector<uchar> a(32, 0), b = a;
            b[i] = 1;
            MutualPattern p(a, b);
            check(!p.hasPattern(16, zeros), "non-mutual boundary accepted");
        }
    });
    test("wrong shared boundary byte is rejected", [&] {
        std::vector<uchar> a(32, 0);
        a[18] = 1;
        MutualPattern p(a, a);
        check(!p.hasPattern(16, zeros), "wrong boundary byte accepted");
    });
    test("offset zero still uses its own mask", [&] {
        std::vector<uchar> a(32, 0), b = a;
        b[16] = 1;
        MutualPattern p(a, b);
        check(p.hasPattern(0, zeros), "unrelated suffix mask hid prefix");
        b[0] = 1;
        MutualPattern q(a, b);
        check(!q.hasPattern(0, zeros), "non-mutual prefix accepted");
    });
    test("wildcard placeholder is not a shared literal byte", [&] {
        std::vector<uchar> a(32, 63), b = a, question_marks(4, 63);
        b[16] = 64;
        MutualPattern p(a, b);
        check(!p.hasPattern(16, question_marks), "wildcard accepted as literal");
    });
    test("intersecting another reference preserves offset semantics", [&] {
        std::vector<uchar> a(32, 0), b = a;
        MutualPattern p(a, a);
        b[0] = 1;
        check(p.intersectBufIf(b), "expected prefix intersection");
        check(p.hasPattern(16, zeros), "new prefix wildcard hid boundary");
        b[17] = 1;
        check(p.intersectBufIf(b), "expected target intersection");
        check(!p.hasPattern(16, zeros), "new target wildcard ignored");
    });
    test("all valid four-byte offsets use matching mask positions", [&] {
        for (unsigned off = 0; off <= 28; ++off) {
            for (unsigned changed = 0; changed < 32; ++changed) {
                std::vector<uchar> a(32, 0), b = a;
                b[changed] = 1;
                MutualPattern p(a, b);
                bool expected = changed < off || changed >= off + 4;
                check(p.hasPattern(off, zeros) == expected,
                      "offset/mask matrix mismatch");
            }
        }
    });
    std::cout << passed << " passed, " << failed << " failed\n";
    return failed ? 1 : 0;
}
