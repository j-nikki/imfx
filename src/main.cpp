#include <magic_enum.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <ranges>
#include <source_location>
#include <tao/pegtl.hpp>
#include <vector>

#ifndef NDEBUG
#define DBGPRINT(Fmt, ...) fprintf(stderr, Fmt __VA_OPT__(, ) __VA_ARGS__)
#else
#define DBGPRINT(Fmt, ...) ((void)0)
#endif

using word                  = int;
constexpr inline auto szany = sizeof(word);
enum expr : word { ft, fl, pi, gb, id };
static constexpr std::array arity{2, 2, -1, 1, 0};

struct parse_state {
    int *&stk;
    int nimgs;
};

//
// GRAMMAR
//

namespace grammar
{
using namespace tao::pegtl;
// clang-format off
struct number : plus<digit> {};
struct sz : seq<number, one<'x'>, number> {};
template <expr> struct ex;
template <> struct ex<ft> : seq<string<'f', 't', '('>, sz, one<')'>> {};
template <> struct ex<fl> : seq<string<'f', 'l', '('>, sz, one<')'>> {};
template <> struct ex<pi> : seq<string<'p', 'i', '('>, struct any_, one<')'>> {};
template <> struct ex<gb> : seq<string<'g', 'b', '('>, number, one<')'>> {};
template <> struct ex<id> : seq<digit> {};
struct any_ : seq<ex<id>, star<seq<one<'.'>, sor<ex<ft>, ex<fl>, ex<pi>, ex<gb>, ex<id>>>>> {};

template <class T> void put(int *&stk, const T &x) noexcept
{ memcpy(stk, std::addressof(x), sizeof(T)), stk += (sizeof(T) + szany - 1) / szany; }
template <class T> void undo(int *&stk) noexcept
{ stk -= (sizeof(T) + szany - 1) / szany; }

template <class R> struct control_ : normal<R> {};
template <> struct control_<number> : normal<number>
{ static void unwind(const auto &, int *&stk) noexcept { DBGPRINT("UNDO I\n"), undo<int>(stk); } };
template <> struct control_<ex<id>> : normal<ex<id>>
{ static void unwind(const auto &, int *&stk) noexcept { DBGPRINT("UNDO ID\n"), undo<int>(stk); } };
template <expr E> struct control_<ex<E>> : normal<ex<E>>
{ static void unwind(const auto &, int *&stk) noexcept { DBGPRINT("UNDO E\n"), undo<expr>(stk); } };

template <class  R> struct action_ : nothing<R> {};
template <> struct action_<number> {
    static void apply(const auto &in, parse_state &ps)
    {
        int x = 0;
        for (auto it = in.begin(); it != in.end();) x = x * 10 + (*it++ - '0');
        DBGPRINT("PUT I %d\n", x);
        put<int>(ps.stk, x);
    }
};
template <> struct action_<ex<id>> {
    static void apply(const auto &in, parse_state &ps)
    {
        const int x = *in.begin() - '0';
        // TODO: x < ps.nimgs
        DBGPRINT("PUT ID %d\n", x);
        put<int>(ps.stk, x);
        put<expr>(ps.stk, id);
    }
};
template <expr E> struct action_<ex<E>> {
    static constexpr auto name = magic_enum::enum_name(E);
    static void apply(const auto &, parse_state &ps)
    {
        DBGPRINT("PUT E %.*s\n", static_cast<int>(name.size()), name.data());
        put<expr>(ps.stk, E);
    }
};
// clang-format on
} // namespace grammar

bool parse(const char *const src, int *&stk, const int argc)
{
    parse_state ps{stk, argc - 2};
    tao::pegtl::memory_input in{src, ""};
    return tao::pegtl::parse<grammar::any_, grammar::action_, grammar::control_>(in, ps) &&
           in.current() == in.end();
}

//
// IMAGE MANIPULATION
//

void putimg(cv::Mat dst, cv::Mat src)
{
    cv::Rect roi{dst.cols / 2 - src.cols / 2, dst.rows / 2 - src.rows / 2, src.cols, src.rows};
    src.copyTo(cv::Mat{dst, roi});
}

void timed(auto &&f, [[maybe_unused]] std::source_location sl =
                         std::source_location::current()) noexcept(noexcept(f()))
{
    [[maybe_unused]] const auto start = std::chrono::high_resolution_clock::now();
    f();
    DBGPRINT("%s:%u:%u: %ldµs\n", sl.file_name(), sl.line(), sl.column(),
             (std::chrono::high_resolution_clock::now() - start).count() / 1000);
}

int *dofx(int *it, const cv::Mat *const imgs, cv::Mat &res)
{
    //
    // Init params
    //
    const auto e = static_cast<expr>(*--it);
    cv::Mat tmp;
    int wo, ho;
    int hdths;
    switch (e) {
    case id: return res = imgs[*--it].clone(), it;
    case pi: it = dofx(it, imgs, tmp); break;
    case ft: [[fallthrough]];
    case fl: ho = *--it, wo = *--it; break;
    case gb: hdths = *--it; break;
    default: __builtin_unreachable();
    }
    it = dofx(it, imgs, res);

    //
    // Apply fx
    //
    double r;
    const auto r1 = static_cast<double>(wo) / res.cols;
    const auto r2 = static_cast<double>(ho) / res.rows;
    switch (e) {
    case gb: return timed([&] { cv::GaussianBlur(res, res, {}, hdths / 100.); }), it;
    case pi: return timed([&] { putimg(res, tmp); }), it;
    case ft: r = std::min(1., std::min(r1, r2)); break;
    case fl: r = std::max(r1, r2); break;
    default: __builtin_unreachable();
    }
    return timed([&] { cv::resize(res, res, {}, r, r); }), it;
}

//
// MAIN
//

int *walkstk(int *it, int idt = 0) noexcept;

int main(int argc, char **argv)
{
    if (argc < 3) return fprintf(stderr, "usage: imfx <cmd> <imgs>...\n"), 1;

    //
    // Read expression
    //
    int buf[1024], *stk = buf;
    if (!parse(argv[1], stk, argc)) return fprintf(stderr, "illegal expression '%s'\n", argv[1]), 1;
#ifndef NDEBUG
    DBGPRINT("----- IN\n%s\n----- RAW\n", argv[1]);
    for (int i = 0; buf + i != stk; ++i)
        DBGPRINT("%8x ", i);
    DBGPRINT("\n");
    for (auto it = buf; it != stk; ++it)
        DBGPRINT("%8x ", *it);
    DBGPRINT("\n----- TREE\n");
    walkstk(stk);
#endif

    //
    // Enact expression
    //
    cv::Mat ims[16];
    cv::Mat res;
    DBGPRINT("----- IMS\n");
    for (int i = 2; i < argc; ++i)
        DBGPRINT("%d %s\n", i - 2, argv[i]), ims[i - 2] = cv::imread(argv[i], cv::IMREAD_UNCHANGED);
    dofx(stk, ims, res);
    std::vector<uchar> png;
    const auto ok =
        cv::imencode(".png", res, png) &&
        static_cast<std::size_t>(write(STDOUT_FILENO, png.data(), png.size())) == png.size();
    return ok ? 0 : 1;
}

static constexpr auto space = []() {
    std::array<char, 128> res;
    std::ranges::fill(res, ' ');
    return res;
}();
int *walkstk(int *it, int idt) noexcept
{
    for (; *--it != id;) {
        [[maybe_unused]] const auto name = magic_enum::enum_name(static_cast<expr>(*it));
        DBGPRINT("%.*s%.*s\n", idt, space.data(), static_cast<int>(name.size()), name.data());
        if (auto ar = arity[*it]; ar != -1) {
            while (ar--)
                DBGPRINT("%.*s%d\n", idt + 2, space.data(), *--it);
        } else
            it = walkstk(it, idt + 2);
    }
    DBGPRINT("%.*s%d\n", idt, space.data(), *--it);
    return it;
}
