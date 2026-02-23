#ifndef TEST_HPP_
#define TEST_HPP_

using test_signature = void (*)();

struct test_type {
	test_signature test_func = nullptr;
	const char *name         = nullptr;
	const char *file         = nullptr;
	int line                 = 0;
	bool result              = false;
};

#define CONCAT_IMPL(x, y) x##y
#define CONCAT(x, y)      CONCAT_IMPL(x, y)

#define TEST(name)                                                                                                   \
	void TestFuncDetail_##name();                                                                                    \
	__attribute__((section("test_array"), used)) constexpr test_type CONCAT(CONCAT(test_entry_, name), __LINE__) = { \
		&TestFuncDetail_##name,                                                                                      \
		#name,                                                                                                       \
		__FILE__,                                                                                                    \
		__LINE__,                                                                                                    \
	};                                                                                                               \
	void TestFuncDetail_##name()

void my_assert_fail(const char *expr, const char *file, int line);

#define CHECK(expr) \
	((expr)         \
		 ? void(0)  \
		 : my_assert_fail(#expr, __FILE__, __LINE__))
#endif
