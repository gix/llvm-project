// RUN: %check_clang_tidy %s call-base-with-alias %t -- --

struct X
{
    void g() {}
    void h() {}
};

struct B : X
{
    void f() {}
    void g() {}
};

struct D : B
{
    using base = B;
    using super = B;

    void f()
    {
        base::f();

        B::f();
        // CHECK-MESSAGES: :[[@LINE-1]]:9: warning: Base method called without using 'base' type alias [call-base-with-alias]
        // CHECK-FIXES: {{^}}        base::f();{{$}}

        super::f();
        // CHECK-MESSAGES: :[[@LINE-1]]:9: warning: Base method called without using 'base' type alias [call-base-with-alias]
        // CHECK-FIXES: {{^}}        base::f();{{$}}
    }

    void g()
    {
        X::g();
        // CHECK-MESSAGES: :[[@LINE-1]]:9: warning: Calling method via non-direct base class. This may accidentally skip a method defined in a direct base class [call-base-with-alias]
    }

    void h()
    {
        X::h();
        // CHECK-MESSAGES: :[[@LINE-1]]:9: warning: Calling method via non-direct base class. This may accidentally skip a method defined in a direct base class [call-base-with-alias]
    }
};

struct D2 : B
{
// CHECK-FIXES: using base = B;
    void f()
    {
        B::f();
        // CHECK-MESSAGES: :[[@LINE-1]]:9: warning: Base method called without using 'base' type alias [call-base-with-alias]
        // CHECK-FIXES: {{^}}        base::f();{{$}}
    }
};
