#include "internal_inc/parse_env_int.h"
#include <climits>
#include <cstdio>
#include <cstdlib>

static void Expect(bool cond, const char *msg)
{
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        std::exit(1);
    }
}

int main()
{
    Expect(ParseEnvInt("0").value() == 0, "zero");
    Expect(ParseEnvInt("3").value() == 3, "three");
    Expect(ParseEnvInt("-1").value() == -1, "neg");
    Expect(ParseEnvInt("2147483647").value() == INT_MAX, "int_max");
    Expect(!ParseEnvInt(""), "empty");
    Expect(!ParseEnvInt("abc"), "abc");
    Expect(!ParseEnvInt("12a"), "12a");
    Expect(!ParseEnvInt("9999999999999999999"), "huge");
    Expect(!ParseEnvInt("2147483648"), "int_max_plus");
    Expect(!ParseEnvInt(" 3"), "leading_space");
    std::puts("ok");
    return 0;
}
