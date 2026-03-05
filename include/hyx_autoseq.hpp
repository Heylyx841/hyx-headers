#pragma once

#include <version>
#if __cplusplus < 202302L
#error "hyx_autoseq.hpp requires C++23 or later."
#endif

/**
 * @file hyx_autoseq.hpp
 * @brief C++23 动态数学数列容器
 * @note 只允许单线程调用，迭代器仅代表已缓存范围
 *
 * @version 1.1.0
 * @author Heylyx841
 * @date 2026-03-06
 * @license MIT License
 */

#include <utility>      // std::forward, std::move, std::forward_like
#include <vector>       // std::vector
#include <span>         // std::span
#include <functional>   // std::move_only_function, std::invoke
#include <concepts>     // std::convertible_to, std::regular_invocable
#include <type_traits>  // std::is_invocable_r_v
#include <bit>          // std::bit_ceil
#include <cassert>      // assert
#include <cstddef>      // size_t
#include <algorithm>    // std::max
#include <stdexcept>    // std::out_of_range, std::invalid_argument

/**
 * @namespace hyx
 */
namespace hyx
{

/**
 * @namespace autoseq_details
 * @brief 内部实现细节
 */
namespace autoseq_details
{

/**
 * @class MathContext
 * @brief 数学公式执行上下文
 */
template <typename T>
struct MathContext
{
	/** @brief 当前正在计算的项索引 n */
	size_t index_val;
	/** @brief 已计算的历史数据视图 */
	std::span<const T> history;

	/** @brief 获取当前项索引 n */
	[[nodiscard]] constexpr size_t n() const noexcept
	{
		return index_val;
	}

	/** @brief 获取前一项 */
	[[nodiscard]] constexpr const T& last() const noexcept
	{
		assert(!history.empty() && "hyx::autoseq: Cannot access last() on empty autoseq.");
		return history.back();
	}

	/**
	 * @brief 访问 a[i]
	 * @param i 数学索引，范围 [0, n-1]
	 */
	[[nodiscard]] constexpr const T& operator[](size_t i) const noexcept
	{
		assert(i < history.size() && "hyx::autoseq: Index out of range.");
		// 允许编译器在此处消除多余的检查指令，极大提升数学公式执行速度
		[[assume(i < history.size())]];
		return history[i];
	}
};

/**
 * @brief 智能签名适配
 */
template <typename T, typename F>
constexpr auto make_dispatch(F&& f)
{
	using Context = MathContext<T>;

	// 模式 A: 双参数原始模式 (size_t n, std::span history)
	if constexpr(std::is_invocable_r_v<T, F, size_t, std::span<const T>>)
	{
		return[f = std::forward<F>(f)](size_t n, std::span<const T> h) mutable -> T
		{
			return static_cast<T>(std::invoke(f, n, h));
		};
	}
	// 模式 B: 单参数数学上下文模式 (MathContext)
	else if constexpr(std::is_invocable_r_v<T, F, Context>)
	{
		return [f = std::forward<F>(f)](size_t n, std::span<const T> h) mutable -> T
		{
			return static_cast<T>(std::invoke(f, Context{n, h}));
		};
	}
	else
	{
		static_assert(false, "hyx::autoseq: Unrecognized formula signature. Expected T(size_t, span) or T(MathContext).");
	}
}

} // namespace autoseq_details

/**
 * @class autoseq
 * @brief 动态数学数列容器
 *
 * @tparam T 数值类型
 */
template <typename T>
class autoseq
{
	static_assert(!std::is_reference_v<T>, "hyx::autoseq: Element type cannot be a reference.");

private:
	/** @brief 项数据缓存 */
	mutable std::vector<T> cache_;

	/**
	 * @brief 生成公式封装
	 * @note 使用 move_only_function 允许 lambda 捕获不可拷贝对象 (如 unique_ptr)
	 */
	mutable std::move_only_function<T(size_t, std::span<const T>)> formula_;

	/**
	 * @brief 确保计算达到指定的数学索引 (核心优化函数)
	 */
	void ensure_calculated(size_t target_index) const
	{
		if(target_index < cache_.size()) [[likely]] return;

		const size_t needed_size = target_index + 1;

		// 避免 O(N^2) 内存重分配开销
		if(needed_size > cache_.capacity()) [[unlikely]]
		{
			size_t new_cap = std::max<size_t>(16, cache_.capacity());
			while(new_cap < needed_size)
			{
				new_cap += new_cap >> 1;
			}
			cache_.reserve(new_cap);
		}

		// 执行时地址稳定保证：由于上面已经 reserve，此处循环内绝对不会发生 reallocation
		// 这保证了传递给 formula_ 的 span 中的指针在执行期间严格安全
		while(cache_.size() < needed_size)
		{
			cache_.emplace_back(formula_(cache_.size(), std::span<const T> {cache_}));
		}
	}

public:
	/**
	 * @brief 构造函数
	 */
	template <typename Gen, typename... InitArgs>
	requires(std::convertible_to<InitArgs, T> && ...)
	explicit autoseq(Gen&& g, InitArgs&&... init_values)
		: formula_(autoseq_details::make_dispatch<T>(std::forward<Gen>(g)))
	{
		if constexpr(sizeof...(init_values) > 0)
		{
			cache_.reserve(sizeof...(init_values));
			// 直接使用 emplace_back 折叠表达式，省去多余的强转和复制
			(cache_.emplace_back(std::forward<InitArgs>(init_values)), ...);
		}
	}

	/** @brief 显式禁止拷贝 (因 formula_ 可能持有 move-only 对象) */
	autoseq(const autoseq&) = delete;
	autoseq& operator=(const autoseq&) = delete;

	/** @brief 支持移动语义 */
	autoseq(autoseq&&) noexcept = default;
	autoseq& operator=(autoseq&&) noexcept = default;

	/** @brief 默认析构函数 */
	~autoseq() = default;

	/**
	 * @brief 访问数列第 n 项 (a_n)
	 * @note 对于数学数列，严格保持返回值不可变(const T&)。这里使用标准的 const 成员函数以防止返回值的悬垂引用风险。
	 */
	[[nodiscard]] const T& operator[](size_t n) const noexcept
	{
		ensure_calculated(n);
		return cache_[n];
	}

	/**
	 * @brief 带边界检查访问数列第 n 项 (a_n)
	 */
	[[nodiscard]] const T& at(size_t n) const
	{
		if(n >= cache_.max_size()) [[unlikely]]
			throw std::out_of_range("hyx::autoseq: Index exceeds maximum container size.");
		ensure_calculated(n);
		return cache_[n];
	}

	/**
	 * @brief 缓存数列到第 n 项 (a_n)
	 */
	void prefetch_up_to(size_t n) const
	{
		ensure_calculated(n);
	}

	/**
	 * @brief 预分配缓存容量
	 */
	void reserve(size_t n) const
	{
		cache_.reserve(n);
	}

	/**
	 * @brief 多下标切片访问 [start, end)
	 */
	[[nodiscard]] std::span<const T> slice(size_t start, size_t end) const
	{
		if(start > end) [[unlikely]]
			throw std::invalid_argument("hyx::autoseq: Invalid slice range (start > end).");
		if(start == end) return {};

		ensure_calculated(end - 1);
		return std::span<const T> {cache_}.subspan(start, end - start);
	}

	/**
	 * @brief 获取当前已缓存数据的只读视图
	 */
	[[nodiscard]] std::span<const T> view() const noexcept
	{
		return std::span<const T> {cache_};
	}

	/**
	 * @brief 转换为 vector
	 */
	template <typename Self>
	[[nodiscard]] std::vector<T> snapshot(this Self&& self)
	{
		// 如果 self 是右值，forward_like 会将 cache_ 转为右值引用从而触发移动构造；
		// 如果 self 是左值，则退化为拷贝构造。
		return std::forward_like<Self>(self.cache_);
	}

	/** @brief 获取当前已缓存的数据项总数 */
	[[nodiscard]] size_t size() const noexcept
	{
		return cache_.size();
	}

	using value_type = T;
	using const_iterator = typename std::vector<T>::const_iterator;

	/** @brief 获取当前已缓存部分的起始/结束迭代器 */
	[[nodiscard]] const_iterator begin() const noexcept
	{
		return cache_.begin();
	}
	[[nodiscard]] const_iterator end() const noexcept
	{
		return cache_.end();
	}
};

} // namespace hyx
