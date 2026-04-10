#ifndef ADS_MYASSERT_H
#define ADS_MYASSERT_H

#include <cassert>

#define DIV_ROUNDUP(x, y) ((x)+(y)-1)/(y)


#define endl '\n'
#define sc static_cast
#define scu64(x) static_cast<uint64_t>(x)
#define scu32(x) static_cast<uint32_t>(x)
#define scu16(x) static_cast<uint16_t>(x)
#define scu8(x) static_cast<uint8_t>(x)
#define sc64(x) static_cast<int64_t>(x)
#define sc32(x) static_cast<int32_t>(x)
#define sc16(x) static_cast<int16_t>(x)
#define sc8(x) static_cast<int8_t>(x)

#define rc reinterpret_cast
#define rcbyte reinterpret_cast<uint8_t*>
#define rcuint reinterpret_cast<uintptr_t>


template<typename ...Args>
void _printAll(Args ... rest);

template<typename A, typename ...Args>
void _printAll(A v1, Args... vrest) {
    std::cerr << v1 << " ";
    _printAll(vrest...);
}

template< >
void _printAll() {
    std::cerr << endl;
    std::cerr.flush();
}

#define FILELINE __FILE__":" TOSTRING(__LINE__)
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define RANGE_E(x, from, to) (((from)<=(x))&((x)<(to)))
#define RANGE RANGE_E
#define RANGE_I(x, from, to) (((from)<=(x))&((x)<=(to)))
#define  PRINT(...) _printAll(FILELINE,__VA_ARGS__);
#define NDEBUG
#ifdef NDEBUG
#define ASSERT_NE(...)
#define ASSERT_E(...)
#define ASSERT_GR(...)
#define ASSERT_GE(...)
#define ASSERT_L(...)
#define ASSERT_LE(...)
#define ASSERT_E(...)
#define ASSERT_R(...)
#define ASSERT_RI(...)
#define ASSERT_RE(...)
#define ASSERT_B(...)
#define ASSERT_PNE(...)
#define ASSERT_PE(...)
#define ASSERT_PGR(...)
#define ASSERT_PGE(...)
#define ASSERT_PL(...)
#define ASSERT_PLE(...)
#define ASSERT_PE(...)
#define ASSERT_PR(...)
#define ASSERT_PB(...)
#define ASSERT_UNREACHEABLE(...)
#define ASSERT_UNIMPLEMETED(...)
#define ASSERT_CODE(...)
#define ASSERT_CODE_GLOBALE(...)
#define ASSERT_CODE_LOCALE(...)
#define NO_ASSERT_CODE(...) __VA_ARGS__
#else
#define ASSERT_NE(result, target, ...) {if((result) == (target)) {cerr<<__FILE__":" TOSTRING(__LINE__)": "#result" = "<<(result)<< " but should not be: "<< (target)<<" --";_printAll(__VA_ARGS__);std::abort();}};
#define ASSERT_E(result, target, ...) if((result) != (target)) {cerr<<__FILE__":" TOSTRING(__LINE__)": "#result" = "<<(result)<< " but should be: "<< (target)<<" --";_printAll(__VA_ARGS__);std::abort();};
#define ASSERT_GR(result, target, ...) if((result) <= (target)) {cerr<<__FILE__":" TOSTRING(__LINE__)": "#result" = "<<(result)<< " but should be greater than: "<< (target)<<" --";_printAll(__VA_ARGS__);std::abort();};
#define ASSERT_GE(result, target, ...) if((result) < (target)) {cerr<<__FILE__":" TOSTRING(__LINE__)": "#result" = "<<(result)<< " but should be greater equal than: "<< (target)<<" --";_printAll(__VA_ARGS__);std::abort();};
#define ASSERT_L(result, target, ...) if((result) >= (target)) {cerr<<__FILE__":" TOSTRING(__LINE__)": "#result" = "<<(result)<< " but should be smaller than: "<< (target)<<" --";_printAll(__VA_ARGS__);std::abort();};
#define ASSERT_LE(result, target, ...) if((result) > (target)) {cerr<<__FILE__":" TOSTRING(__LINE__)": "#result" = "<<(result)<< " but should be smaller equal than: "<< (target)<<" --";_printAll(__VA_ARGS__);std::abort();};
#define ASSERT_R(result, left, right, ...) if(!(RANGE((result),(left),(right)))){cerr<<__FILE__":" TOSTRING(__LINE__)": "#result" = " << (result) <<" is not in range ["#left", "#right") = ["<<(left)<<", "<<(right)<<") --";_printAll(__VA_ARGS__);std::abort();};
#define ASSERT_RI(result, left, right, ...) if(!(RANGE_I((result),(left),(right)))){cerr<<__FILE__":" TOSTRING(__LINE__)": "#result" = " << (result) <<" is not in range ["#left", "#right"] = ["<<(left)<<", "<<(right)<<"] --";_printAll(__VA_ARGS__);std::abort();};
#define ASSERT_RE(result, left, right, ...) if(!(RANGE_E((result),(left),(right)))){cerr<<__FILE__":" TOSTRING(__LINE__)": "#result" = " << (result) <<" is not in range ["#left", "#right") = ["<<(left)<<", "<<(right)<<") --";_printAll(__VA_ARGS__);std::abort();};
#define ASSERT_B(result, ...) if(!(result)){cerr<<__FILE__":" TOSTRING(__LINE__)": "#result" is false --";_printAll(__VA_ARGS__);std::abort();};
#define ASSERT_UNREACHEABLE(...) cerr<<__FILE__":" TOSTRING(__LINE__)": unreacheable code had been reached --";_printAll(__VA_ARGS__);std::abort();
#define ASSERT_UNIMPLEMETED(...) cerr<<__FILE__":" TOSTRING(__LINE__)": unimplemented code had been reached --";_printAll(__VA_ARGS__);std::abort();
#define ASSERT_PNE(precond, result, target, ...) {if((precond) && (result) == (target)) {cerr<<__FILE__":" TOSTRING(__LINE__)": "#result" = "<<(result)<< " but should not be: "<< (target)<<" --";_printAll(__VA_ARGS__);std::abort();}};
#define ASSERT_PE(precond, result, target, ...) if((precond) && (result) != (target)) {cerr<<__FILE__":" TOSTRING(__LINE__)": "#result" = "<<(result)<< " but should be: "<< (target)<<" --";_printAll(__VA_ARGS__);std::abort();};
#define ASSERT_PGR(precond, result, target, ...) if((precond) && (result) <= (target)) {cerr<<__FILE__":" TOSTRING(__LINE__)": "#result" = "<<(result)<< " but should be greater than: "<< (target)<<" --";_printAll(__VA_ARGS__);std::abort();};
#define ASSERT_PGE(precond, result, target, ...) if((precond) && (result) < (target)) {cerr<<__FILE__":" TOSTRING(__LINE__)": "#result" = "<<(result)<< " but should be greater equal than: "<< (target)<<" --";_printAll(__VA_ARGS__);std::abort();};
#define ASSERT_PL(precond, result, target, ...) if((precond) && (result) >= (target)) {cerr<<__FILE__":" TOSTRING(__LINE__)": "#result" = "<<(result)<< " but should be smaller than: "<< (target)<<" --";_printAll(__VA_ARGS__);std::abort();};
#define ASSERT_PLE(precond, result, target, ...) if((precond) && (result) > (target)) {cerr<<__FILE__":" TOSTRING(__LINE__)": "#result" = "<<(result)<< " but should be smaller equal than: "<< (target)<<" --";_printAll(__VA_ARGS__);std::abort();};
#define ASSERT_PR(precond, result, left, right, ...) if((precond) && !(RANGE((result),(left),(right)))){cerr<<__FILE__":" TOSTRING(__LINE__)": "#result" = " << (result) <<" is not in range ["#left", "#right") = ["<<(left)<<", "<<(right)<<") --";_printAll(__VA_ARGS__);std::abort();};
#define ASSERT_PB(precond, result, ...) if((precond) && !(result)){cerr<<__FILE__":" TOSTRING(__LINE__)": "#result" is false --";_printAll(__VA_ARGS__);std::abort();};
#define ASSERT_CODE_GLOBALE(...) __VA_ARGS__
#define ASSERT_CODE_LOCALE(...){ __VA_ARGS__}
#define NO_ASSERT_CODE(...)
#endif
#define ALWAYS_ASSERT_NE(result, target, ...) {if((result) == (target)) {cerr<<__FILE__":" TOSTRING(__LINE__)": "#result" = "<<(result)<< " but should not be: "<< (target)<<" --";_printAll(__VA_ARGS__);std::abort();}};
#define ALWAYS_ASSERT_E(result, target, ...) if((result) != (target)) {cerr<<__FILE__":" TOSTRING(__LINE__)": "#result" = "<<(result)<< " but should be: "<< (target)<<" --";_printAll(__VA_ARGS__);std::abort();};
#define ALWAYS_ASSERT_GR(result, target, ...) if((result) <= (target)) {cerr<<__FILE__":" TOSTRING(__LINE__)": "#result" = "<<(result)<< " but should be greater than: "<< (target)<<" --";_printAll(__VA_ARGS__);std::abort();};
#define ALWAYS_ASSERT_GE(result, target, ...) if((result) < (target)) {cerr<<__FILE__":" TOSTRING(__LINE__)": "#result" = "<<(result)<< " but should be greater equal than: "<< (target)<<" --";_printAll(__VA_ARGS__);std::abort();};
#define ALWAYS_ASSERT_L(result, target, ...) if((result) >= (target)) {cerr<<__FILE__":" TOSTRING(__LINE__)": "#result" = "<<(result)<< " but should be smaller than: "<< (target)<<" --";_printAll(__VA_ARGS__);std::abort();};
#define ALWAYS_ASSERT_LE(result, target, ...) if((result) > (target)) {cerr<<__FILE__":" TOSTRING(__LINE__)": "#result" = "<<(result)<< " but should be smaller equal than: "<< (target)<<" --";_printAll(__VA_ARGS__);std::abort();};
#define ALWAYS_ASSERT_R(result, left, right, ...) if(!(RANGE((result),(left),(right)))){cerr<<__FILE__":" TOSTRING(__LINE__)": "#result" = " << (result) <<" is not in range ["#left", "#right") = ["<<(left)<<", "<<(right)<<") --";_printAll(__VA_ARGS__);std::abort();};
#define ALWAYS_ASSERT_B(result, ...) if(!(result)){cerr<<__FILE__":" TOSTRING(__LINE__)": "#result" is false --";_printAll(__VA_ARGS__);std::abort();};
#define ALWAYS_ASSERT_UNREACHEABLE(...) cerr<<__FILE__":" TOSTRING(__LINE__)": unreacheable code has been reached --";_printAll(__VA_ARGS__);std::abort();
#define ALWAYS_ASSERT_PNE(precond, result, target, ...) {if((precond) && (result) == (target)) {cerr<<__FILE__":" TOSTRING(__LINE__)": "#result" = "<<(result)<< " but should not be: "<< (target)<<" --";_printAll(__VA_ARGS__);std::abort();}};
#define ALWAYS_ASSERT_PE(precond, result, target, ...) if((precond) && (result) != (target)) {cerr<<__FILE__":" TOSTRING(__LINE__)": "#result" = "<<(result)<< " but should be: "<< (target)<<" --";_printAll(__VA_ARGS__);std::abort();};
#define ALWAYS_ASSERT_PGR(precond, result, target, ...) if((precond) && (result) <= (target)) {cerr<<__FILE__":" TOSTRING(__LINE__)": "#result" = "<<(result)<< " but should be greater than: "<< (target)<<" --";_printAll(__VA_ARGS__);std::abort();};
#define ALWAYS_ASSERT_PGE(precond, result, target, ...) if((precond) && (result) < (target)) {cerr<<__FILE__":" TOSTRING(__LINE__)": "#result" = "<<(result)<< " but should be greater equal than: "<< (target)<<" --";_printAll(__VA_ARGS__);std::abort();};
#define ALWAYS_ASSERT_PL(precond, result, target, ...) if((precond) && (result) >= (target)) {cerr<<__FILE__":" TOSTRING(__LINE__)": "#result" = "<<(result)<< " but should be smaller than: "<< (target)<<" --";_printAll(__VA_ARGS__);std::abort();};
#define ALWAYS_ASSERT_PLE(precond, result, target, ...) if((precond) && (result) > (target)) {cerr<<__FILE__":" TOSTRING(__LINE__)": "#result" = "<<(result)<< " but should be smaller equal than: "<< (target)<<" --";_printAll(__VA_ARGS__);std::abort();};
#define ALWAYS_ASSERT_PR(precond, result, left, right, ...) if((precond) && !(RANGE((result),(left),(right)))){cerr<<__FILE__":" TOSTRING(__LINE__)": "#result" = " << (result) <<" is not in range ["#left", "#right") = ["<<(left)<<", "<<(right)<<") --";_printAll(__VA_ARGS__);std::abort();};
#define ALWAYS_ASSERT_PB(precond, result, ...) if((precond) && !(result)){cerr<<__FILE__":" TOSTRING(__LINE__)": "#result" is false --";_printAll(__VA_ARGS__);std::abort();};

#endif //ADS_MYASSERT_H
