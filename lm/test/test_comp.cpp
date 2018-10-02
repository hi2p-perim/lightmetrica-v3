/*
    Lightmetrica - Copyright (c) 2018 Hisanari Otsu
    Distributed under MIT license. See LICENSE file for details.
*/

#include "test_common.h"
#include "test_interface.h"
#include <generated/test_python.h>
#include <pybind11/embed.h>
#include <pybind11/functional.h>

namespace py = pybind11;
using namespace py::literals;

LM_NAMESPACE_BEGIN(LM_TEST_NAMESPACE)

// ----------------------------------------------------------------------------

// _begin_snippet: A
struct A : public lm::Component {
    virtual int f1() = 0;
    virtual int f2(int a, int b) = 0;
};

struct A1 final : public A {
    virtual int f1() override { return 42; }
    virtual int f2(int a, int b) override { return a + b; }
};

LM_COMP_REG_IMPL(A1, "test::comp::a1");
// _end_snippet: A

// ----------------------------------------------------------------------------

struct B : public A {
    virtual int f3() = 0;
};

struct B1 final : public B {
    virtual int f1() override { return 42; }
    virtual int f2(int a, int b) override { return a + b; }
    virtual int f3() override { return 43; }
};

LM_COMP_REG_IMPL(B1, "test::comp::b1");

// ----------------------------------------------------------------------------

struct C : public lm::Component {
    C() { std::cout << "C"; }
    virtual ~C() { std::cout << "~C"; }
};

struct C1 : public C {
    C1() { std::cout << "C1"; }
    virtual ~C1() { std::cout << "~C1"; }
};

LM_COMP_REG_IMPL(C1, "test::comp::c1");

// ----------------------------------------------------------------------------

TEST_CASE("Component")
{
    SUBCASE("Simple interface") {
        // _begin_snippet: A_impl
        const auto p = lm::comp::create<A>("test::comp::a1");
        REQUIRE(p);
        CHECK(p->f1() == 42);
        CHECK(p->f2(1, 2) == 3);
        // _end_snippet: A_impl
    }

    SUBCASE("Inherited interface") {
        const auto p = lm::comp::create<B>("test::comp::b1");
        REQUIRE(p);
        CHECK(p->f1() == 42);
        CHECK(p->f2(1, 2) == 3);
        CHECK(p->f3() == 43);
    }

    SUBCASE("Missing implementation") {
        const auto p = lm::comp::create<A>("test::comp::a_missing");
        CHECK(p == nullptr);
    }

    SUBCASE("Cast to parent interface") {
        auto b = lm::comp::create<B>("test::comp::b1");
        const auto a = std::unique_ptr<A>(std::move(b));
        REQUIRE(a);
        CHECK(a->f1() == 42);
        CHECK(a->f2(1, 2) == 3);
    }

    SUBCASE("Constructor and destructor") {
        const auto out = captureStdout([]() {
            const auto p = lm::comp::create<C>("test::comp::c1");
            REQUIRE(p);
        });
        CHECK(out == "CC1~C1~C");
    }

    SUBCASE("Plugin") {
        lm::comp::detail::ScopedLoadPlugin pluginGuard("lm_test_plugin");
        REQUIRE(pluginGuard.valid());
        SUBCASE("Simple") {
            auto p = lm::comp::create<lm::TestPlugin>("testplugin::default");
            REQUIRE(p);
            CHECK(p->f() == 42);
        }
        SUBCASE("Plugin constructor and destructor") {
            const auto out = captureStdout([]() {
                const auto p = lm::comp::create<lm::TestPluginWithCtorAndDtor>("testpluginxtor::default");
                REQUIRE(p);
            });
            CHECK(out == "AB~B~A");
        }
    }

    SUBCASE("Failed to load plugin") {
        REQUIRE(!lm::comp::detail::loadPlugin("__missing_plugin_name__"));
    }
}

// ----------------------------------------------------------------------------

// Define a trampoline class (see ref) for the interface A
// https://pybind11.readthedocs.io/en/stable/advanced/classes.html
struct A_Py final : public A {
    virtual int f1() override {
        PYBIND11_OVERLOAD_PURE(int, A, f1);
    }
    virtual int f2(int a, int b) override {
        PYBIND11_OVERLOAD_PURE(int, A, f2, a, b);
    }
};

struct TestPlugin_Py final : public lm::TestPlugin {
    virtual int f() override {
        PYBIND11_OVERLOAD_PURE(int, lm::TestPlugin, f);
    }
};

PYBIND11_EMBEDDED_MODULE(test_comp, m) {
    py::class_<A, A_Py>(m, "A")
        .def(py::init<>())
        .def("f1", &A::f1)
        .def("f2", &A::f2);

    py::class_<lm::TestPlugin, TestPlugin_Py>(m, "TestPlugin")
        .def(py::init<>())
        .def("f", &lm::TestPlugin::f);

    m.def("createA1", []() {
        return lm::comp::create<A>("test::comp::a1");
    });

    m.def("createTestPlugin", []() {
        return lm::comp::create<lm::TestPlugin>("testplugin::default");
    });

    m.def("useA", [](A* a) -> int {
        return a->f1() * 2;
    });

    m.def("createComp", [](const char* name) -> py::object {
        // Create component instance
        const auto inst = lm::comp::detail::createComp(name);
        // Cast it to python object
        // It fails because there is no matching registration for lm::Component
        auto instPy = py::cast(inst);
        // Check reference
        std::cout << instPy.ref_count() << std::endl;
        // Return it to python
        return instPy;
    });
}

// unique_ptr with custom deleter
template <typename T>
using UniquePtr = std::unique_ptr<T, std::function<void(T*)>>;

// Wraps dereferencing of python object in the deleter
template <typename T, typename... Args>
UniquePtr<T> create(const char* name, Args... opt) {
    const auto instPy = py::globals()[name](opt...);
    return UniquePtr<T>(instPy.cast<T*>(), [p = py::reinterpret_borrow<py::object>(instPy)](T*){});
}

TEST_CASE("Python component plugin")
{
    Py_SetPythonHome(LM_TEST_PYTHON_ROOT);
    py::scoped_interpreter guard{};
  
    try {
        // Import test module
        py::exec(R"(
            import test_comp
        )", py::globals());

        SUBCASE("Create and use component inside python script") {
            auto locals = py::dict();
            py::exec(R"(
                p = test_comp.createA1()
                r1 = p.f1()
                r2 = p.f2(1, 2)
            )", py::globals(), locals);
            CHECK(locals["r1"].cast<int>() == 42);
            CHECK(locals["r2"].cast<int>() == 3);
        }

        SUBCASE("Use component inside native plugin") {
            lm::comp::detail::ScopedLoadPlugin pluginGuard("lm_test_plugin");
            REQUIRE(pluginGuard.valid());
            auto locals = py::dict();
            py::exec(R"(
                p = test_comp.createTestPlugin()
                r1 = p.f()
            )", py::globals(), locals);
            CHECK(locals["r1"].cast<int>() == 42);
        }

        SUBCASE("Extend component from python") {
            py::exec(R"(
                class A2(test_comp.A):
                    def f1(self):
                        return 43
                    def f2(self, a, b):
                        return a * b

                class A3(test_comp.A):
                    def __init__(self, v):
                        # We need explicit initialization (not super()) of the base class
                        # See https://pybind11.readthedocs.io/en/stable/advanced/classes.html
                        test_comp.A.__init__(self)
                        self.v = v
                    def f1(self):
                        return self.v
                    def f2(self, a, b):
                        return self.v * self.v
            )", py::globals());

            SUBCASE("Instantiate inside python script and use it in python") {
                auto locals = py::dict();
                py::exec(R"(
                    p = A2()
                    r1 = p.f1()
                    r2 = p.f2(2, 3)
                )", py::globals(), locals);
                CHECK(locals["r1"].cast<int>() == 43);
                CHECK(locals["r2"].cast<int>() == 6);
            }

            SUBCASE("Instantiate inside python script and use it in C++") {
                auto locals = py::dict();
                py::exec(R"(
                    p = A2()
                    r1 = test_comp.useA(p)
                )", py::globals(), locals);
                CHECK(locals["r1"].cast<int>() == 86);
            }

            SUBCASE("Instantiate in C++ and use it in C++") {
                // Extract A2
                auto A2 = py::globals()["A2"];
                // Create instance of A2 (python object)
                auto p_py = A2();
                // Cast to A*
                auto p = p_py.cast<A*>();
                // Call function
                const auto result = p->f1();
                CHECK(result == 43);
            }

            SUBCASE("Manage instances in C++") {
                SUBCASE("Without constructor") {
                    auto p = create<A>("A2");
                    REQUIRE(p);
                    CHECK(p->f1() == 43);
                }
                SUBCASE("With constructor") {
                    auto p = create<A>("A3", 1);
                    REQUIRE(p);
                    CHECK(p->f1() == 1);
                }
                SUBCASE("In C++ vector") {
                    std::vector<UniquePtr<A>> v;
                    for (int i = 0; i < 10; i++) {
                        v.push_back(create<A>("A3", i));
                        REQUIRE(v.back());
                    }
                    for (int i = 0; i < 10; i++) {
                        CHECK(v[i]->f1() == i);
                    }
                }
            }
        }

        SUBCASE("Create component factory") {
            auto locals = py::dict();
            py::exec(R"(
                p = test_comp.createComp('test::comp::a1')
                
            )", py::globals(), locals);
            CHECK(locals["r1"].cast<int>() == 43);
            CHECK(locals["r2"].cast<int>() == 6);
        }
    }
    catch (const std::runtime_error& e) {
        std::cerr << e.what() << std::endl;
        REQUIRE(false);
    }
}

// ----------------------------------------------------------------------------

LM_NAMESPACE_END(LM_TEST_NAMESPACE)
