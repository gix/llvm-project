// RUN: %check_clang_tidy %s misc-tidy-variadic-calls %t -- -- -fms-extensions -fms-compatibility -Wno-non-pod-varargs -Wno-clang-diagnostic-non-pod-varargs

namespace ATL {

template< typename BaseType = char >
class ChTraitsBase
{};

template< typename _CharType = char >
class ChTraitsCRT : public ChTraitsBase< _CharType >
{};

template<typename BaseType , bool t_bMFCDLL = false>
class CSimpleStringT
{};

template<typename BaseType, class StringTraits>
class CStringT : public CSimpleStringT<BaseType, false>
{
public:
    CStringT() throw();
    ~CStringT() throw() {}
    CStringT(const CStringT& strSrc);
    CStringT(const char* pszSrc);
    CStringT(const wchar_t* pszSrc);
    CStringT(const unsigned char* pszSrc);
    CStringT& operator=(const CStringT& strSrc);
    CStringT& operator=(const char* pszSrc);
    CStringT& operator=(const wchar_t* pszSrc);
    CStringT& operator=(const unsigned char* pszSrc);
    CStringT Left(int nCount) const;
    CStringT Right(int nCount) const;
    operator const char*() const throw();
    const char* GetString() const;
    friend CStringT operator+(const CStringT& str1, const CStringT& str2);
    friend CStringT operator+(const CStringT& str1, const char* str2);
    friend CStringT operator+(const char* str1, const CStringT& str2);
    void __cdecl Format(const char* fmt, ...);
    bool IsEmpty() const;
};

} // namespace ATL

template<typename _CharType = char, class StringIterator = ATL::ChTraitsCRT<_CharType>>
class StrTraitMFC
{};

typedef char TCHAR;
typedef ATL::CStringT< wchar_t, StrTraitMFC< wchar_t > > CStringW;
typedef ATL::CStringT< char, StrTraitMFC< char > > CStringA;
typedef ATL::CStringT< TCHAR, StrTraitMFC< TCHAR > > CString;

namespace ATL {

class COleDateTime
{};

} // namespace ATL

void printf(const char* fmt, ...);
size_t strlen(const char*);

#define DBGLOG(fmt, ...) printf(fmt, __VA_ARGS__)

#define _DEBUG_MSG_LEN 10
#define _tcslen strlen

struct MyDateTime : ATL::COleDateTime
{
    CString FormatGerman() const { return CString(); }
};

#define Y_STR y.str2
#define Z_OBJ y.z

struct Z
{
    CString str3;
};

struct Y
{
    Z z;
    CString str2;
    Z& g() { return z; }
    CString str() { return str2; }
    CString& ref() { return str2; }
    CString const& cref() { return str2; }
};

struct X
{
    CString str;
    Y y;
    CString& operator[](int index) { return str; }
    CString& operator()(int index) { return str; }
    Y& f() { return y; }
};

void f1(CString s, CString& r, CString const& c, CString* p, X* px, X& rx)
{
    Y local;

    printf("%s %s %s", s, r, c);
    // CHECK-MESSAGES: :[[@LINE-1]]:24: warning: CString passed as vararg [misc-tidy-variadic-calls]
    // CHECK-MESSAGES: :[[@LINE-2]]:27: warning: CString passed as vararg [misc-tidy-variadic-calls]
    // CHECK-MESSAGES: :[[@LINE-3]]:30: warning: CString passed as vararg [misc-tidy-variadic-calls]
    // CHECK-FIXES: {{^}}    printf("%s %s %s", s.GetString(), r.GetString(), c.GetString());{{$}}

    printf("%s", p[0]);
    // CHECK-MESSAGES: :[[@LINE-1]]:18: warning: CString passed as vararg [misc-tidy-variadic-calls]
    // CHECK-FIXES: {{^}}    printf("%s", p[0].GetString());{{$}}

    printf("%s", rx[0]);
    // CHECK-MESSAGES: :[[@LINE-1]]:18: warning: CString passed as vararg [misc-tidy-variadic-calls]
    // CHECK-FIXES: {{^}}    printf("%s", rx[0].GetString());{{$}}

    printf("%s", px ? px->str : s);
    // CHECK-MESSAGES: :[[@LINE-1]]:18: warning: CString passed as vararg [misc-tidy-variadic-calls]
    // CHECK-FIXES: {{^}}    printf("%s", (px ? px->str : s).GetString());{{$}}

    printf("%s", s = (px ? px->str : s));
    // CHECK-MESSAGES: :[[@LINE-1]]:18: warning: CString passed as vararg [misc-tidy-variadic-calls]
    // CHECK-FIXES: {{^}}    printf("%s", (s = (px ? px->str : s)).GetString());{{$}}

    printf("%s", s + r);
    // CHECK-MESSAGES: :[[@LINE-1]]:18: warning: CString passed as vararg [misc-tidy-variadic-calls]
    // CHECK-FIXES: {{^}}    printf("%s", (s + r).GetString());{{$}}

    printf("%s", (s + r));
    // CHECK-MESSAGES: :[[@LINE-1]]:18: warning: CString passed as vararg [misc-tidy-variadic-calls]
    // CHECK-FIXES: {{^}}    printf("%s", (s + r).GetString());{{$}}

    printf("%s", rx.Y_STR); // Skipped due to macro at end
    printf("%s", (px + 1)->Y_STR); // Skipped due to macro at end

    printf("%s %d", rx.Z_OBJ.str3, 1);
    // CHECK-MESSAGES: :[[@LINE-1]]:21: warning: CString passed as vararg [misc-tidy-variadic-calls]
    // CHECK-FIXES: {{^}}    printf("%s %d", rx.Z_OBJ.str3.GetString(), 1);{{$}}


    printf("%s", local.str());
    // CHECK-MESSAGES: :[[@LINE-1]]:18: warning: CString passed as vararg [misc-tidy-variadic-calls]
    // CHECK-FIXES: {{^}}    printf("%s", local.str().GetString());{{$}}
    printf("%s", local.ref());
    // CHECK-MESSAGES: :[[@LINE-1]]:18: warning: CString passed as vararg [misc-tidy-variadic-calls]
    // CHECK-FIXES: {{^}}    printf("%s", local.ref().GetString());{{$}}
    printf("%s", local.cref());
    // CHECK-MESSAGES: :[[@LINE-1]]:18: warning: CString passed as vararg [misc-tidy-variadic-calls]
    // CHECK-FIXES: {{^}}    printf("%s", local.cref().GetString());{{$}}

    printf("%s", rx.f().str());
    // CHECK-MESSAGES: :[[@LINE-1]]:18: warning: CString passed as vararg [misc-tidy-variadic-calls]
    // CHECK-FIXES: {{^}}    printf("%s", rx.f().str().GetString());{{$}}
    printf("%s", rx.f().ref());
    // CHECK-MESSAGES: :[[@LINE-1]]:18: warning: CString passed as vararg [misc-tidy-variadic-calls]
    // CHECK-FIXES: {{^}}    printf("%s", rx.f().ref().GetString());{{$}}
    printf("%s", rx.f().cref());
    // CHECK-MESSAGES: :[[@LINE-1]]:18: warning: CString passed as vararg [misc-tidy-variadic-calls]
    // CHECK-FIXES: {{^}}    printf("%s", rx.f().cref().GetString());{{$}}

    // ----------

    DBGLOG("%s %s %s", s, r, c);
    // CHECK-MESSAGES: :[[@LINE-1]]:24: warning: CString passed as vararg [misc-tidy-variadic-calls]
    // CHECK-MESSAGES: :[[@LINE-2]]:27: warning: CString passed as vararg [misc-tidy-variadic-calls]
    // CHECK-MESSAGES: :[[@LINE-3]]:30: warning: CString passed as vararg [misc-tidy-variadic-calls]
    // CHECK-FIXES: {{^}}    DBGLOG("%s %s %s", s.GetString(), r.GetString(), c.GetString());{{$}}

    DBGLOG("%s", p[0]);
    // CHECK-MESSAGES: :[[@LINE-1]]:18: warning: CString passed as vararg [misc-tidy-variadic-calls]
    // CHECK-FIXES: {{^}}    DBGLOG("%s", p[0].GetString());{{$}}

    DBGLOG("%s", rx[0]);
    // CHECK-MESSAGES: :[[@LINE-1]]:18: warning: CString passed as vararg [misc-tidy-variadic-calls]
    // CHECK-FIXES: {{^}}    DBGLOG("%s", rx[0].GetString());{{$}}

    DBGLOG("%s", px ? px->str : s);
    // CHECK-MESSAGES: :[[@LINE-1]]:18: warning: CString passed as vararg [misc-tidy-variadic-calls]
    // CHECK-FIXES: {{^}}    DBGLOG("%s", (px ? px->str : s).GetString());{{$}}

    DBGLOG("%s", s = (px ? px->str : s));
    // CHECK-MESSAGES: :[[@LINE-1]]:18: warning: CString passed as vararg [misc-tidy-variadic-calls]
    // CHECK-FIXES: {{^}}    DBGLOG("%s", (s = (px ? px->str : s)).GetString());{{$}}

    DBGLOG("%s", s + r);
    // CHECK-MESSAGES: :[[@LINE-1]]:18: warning: CString passed as vararg [misc-tidy-variadic-calls]
    // CHECK-FIXES: {{^}}    DBGLOG("%s", (s + r).GetString());{{$}}

    DBGLOG("%s", (s + r));
    // CHECK-MESSAGES: :[[@LINE-1]]:18: warning: CString passed as vararg [misc-tidy-variadic-calls]
    // CHECK-FIXES: {{^}}    DBGLOG("%s", (s + r).GetString());{{$}}

    DBGLOG("%s", rx.Y_STR); // Skipped due to macro at end
    DBGLOG("%s", (px + 1)->Y_STR); // Skipped due to macro at end

    DBGLOG("%s %d", rx.Z_OBJ.str3, 1);
    // CHECK-MESSAGES: :[[@LINE-1]]:21: warning: CString passed as vararg [misc-tidy-variadic-calls]
    // CHECK-FIXES: {{^}}    DBGLOG("%s %d", rx.Z_OBJ.str3.GetString(), 1);{{$}}

    MyDateTime dt;
    DBGLOG("%s", dt.FormatGerman());
    // CHECK-MESSAGES: :[[@LINE-1]]:18: warning: CString passed as vararg [misc-tidy-variadic-calls]
    // CHECK-FIXES: {{^}}    DBGLOG("%s", dt.FormatGerman().GetString());{{$}}

    DBGLOG("%s", s.Left(3 + 1) + s.Right(3));
    // CHECK-MESSAGES: :[[@LINE-1]]:18: warning: CString passed as vararg [misc-tidy-variadic-calls]
    // CHECK-FIXES: {{^}}    DBGLOG("%s", (s.Left(3 + 1) + s.Right(3)).GetString());{{$}}

    char const* fmt = "%s";
    DBGLOG(fmt, s.Right(0).Left(_DEBUG_MSG_LEN - _tcslen(fmt) - 1));
    // CHECK-MESSAGES: :[[@LINE-1]]:17: warning: CString passed as vararg [misc-tidy-variadic-calls]
    // CHECK-FIXES: {{^}}    DBGLOG(fmt, s.Right(0).Left(_DEBUG_MSG_LEN - _tcslen(fmt) - 1).GetString());{{$}}
}
