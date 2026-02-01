/**
 * @file hyx_autoseq.hpp
 * @brief C++23 动态数学数列容器 (0-based)
 * @note 只允许单线程调用，迭代器仅代表已缓存范围
 *
 * @version 1.0.0-beta.1
 * @author Heylyx841
 * @date 2026-02-01
 * @license MIT License
 *
 * Copyright (c) 2026 Heylyx841
 */

#ifndef HYX_AUTOSEQ_HPP
#define HYX_AUTOSEQ_HPP 1

#include <utility>      // std::forward, std::move
#include <vector>       // std::vector
#include <span>         // std::span
#include <functional>   // std::move_only_function
#include <concepts>     // std::convertible_to
#include <type_traits>  // std::is_invocable_r_v, etc.
#include <bit>          // std::bit_ceil
#include <cassert>      // assert
#include <cstddef>      // size_t

/**
 * @namespace hyx
 */
namespace hyx
{

/**
 * @namespace autoseq_detail
 * @brief 内部实现细节
 */
namespace autoseq_detail
{
/**
 * @class MathContext
 * @brief 数学公式执行上下文
 *
 * 在生成器 lambda 中作为参数传递。它既代表当前索引 n，也提供对历史项的访问。
 * @tparam T 数列元素类型
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
//		if(history.empty()) [[unlikely]] throw std::out_of_range("hyx::autoseq: Cannot access last() on empty autoseq.");
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
		return history[i];
	}
};

template <typename...> constexpr bool always_false = false;

/**
 * @brief 智能签名适配器
 *
 * 利用 C++ 编译期特性探测用户提供的 lambda 签名。
 * 优先匹配 MathContext 模式，以支持 h[i] 和 h.last() 等直观写法。
 */
template <typename T, typename F>
constexpr auto make_dispatch(F&& f)
{
	using Context = MathContext<T>;

	// 匹配模式 A: 双参数原始模式 (size_t n, std::span history)
	if constexpr(std::is_invocable_r_v<T, F, size_t, std::span<const T>>)
	{
		return [f = std::forward<F>(f)](size_t n, std::span<const T> h) -> T
		{
			return static_cast<T>(f(n, h));
		};
	}
	// 匹配模式 B: 单参数数学上下文模式 (MathContext)
	// 该模式支持泛型 auto 参数，提供最优雅的数学书写体验
	else if constexpr(std::is_invocable_r_v<T, F, Context>)
	{
		return [f = std::forward<F>(f)](size_t n, std::span<const T> h) -> T
		{
			return static_cast<T>(f(Context{n, h}));
		};
	}
	else
	{
		static_assert(always_false<F>, "hyx::autoseq: Unrecognized formula signature.");
		return nullptr;
	}
}
} // namespace autoseq_detail

/**
 * @class autoseq
 * @brief 动态数学数列容器
 *
 * 仅在访问时根据公式计算新项。
 * 内置缓存机制，确保每一项仅计算一次。
 *
 * @tparam T 数值类型
 */
template <typename T>
class autoseq
{
	static_assert(!std::is_reference_v<T>, "hyx::autoseq: Element type cannot be a reference.");
	static_assert(std::is_copy_constructible_v<T> || std::is_move_constructible_v<T>, "hyx::autoseq: Element type must be constructible.");

private:
	/** @brief 项数据缓存 */
	mutable std::vector<T> cache_;
	/** @brief 生成公式封装 */
	mutable std::move_only_function<T(size_t, std::span<const T>) const> formula_;

	/**
	 * @brief 确保计算达到指定的数学索引
	 * @param target_index 目标索引
	 */
	void ensure_calculated(size_t target_index) const
	{
		if(target_index < cache_.size()) return;

		size_t needed_size(target_index + 1);
		// 优化内存分配：采用指数增长预留策略
		size_t old_cap(cache_.capacity());
		if(needed_size > old_cap)
		{
			// 结合 1.5 倍增长与跨度保护
			size_t new_cap(old_cap + (old_cap >> 1));
			if(new_cap < needed_size) new_cap = needed_size;
			// 对于小规模增长，对齐到 8, 16, 32... 等边界有助于减少零碎分配
			if(new_cap < 1024)
				new_cap = std::bit_ceil(new_cap);
			cache_.reserve(new_cap);
		}

		// 循环补充计算缺失的数列项
		while(cache_.size() < needed_size)
		{
			T next_val(formula_(cache_.size(), std::span<const T>(cache_)));
			cache_.emplace_back(std::move(next_val));
		}
	}

public:
	/**
	 * @brief 构造函数
	 * @tparam Gen 公式 lambda 类型
	 * @tparam InitArgs 初始项参数包类型
	 * @param g 生成公式。示例: [](auto F){ return F.last() + F[F.n()-2]; }
	 * @param init_values 初始项序列
	 */
	template <typename Gen, typename... InitArgs>
	requires(std::convertible_to<InitArgs, T> && ...)
	explicit autoseq(Gen&& g, InitArgs&&... init_values) : formula_(autoseq_detail::make_dispatch<T>(std::forward<Gen>(g)))
	{
		if constexpr(sizeof...(init_values) > 0)
		{
			cache_.reserve(sizeof...(init_values));
			(cache_.push_back(static_cast<T>(init_values)), ...);
		}
	}

	/** @brief 禁用拷贝以防止昂贵的隐式缓存复制 */
	autoseq(const autoseq&) = delete;
	autoseq& operator=(const autoseq&) = delete;

	/** @brief 支持移动语义 */
	autoseq(autoseq&&) noexcept = default;
	autoseq& operator=(autoseq&&) noexcept = default;

	/**
	 * @brief 访问数列第 n 项 (a_n)
	 */
	[[nodiscard]] const T& operator[](size_t n) const
	{
		ensure_calculated(n);
		return cache_[n];
	}
	[[nodiscard]] const T& at(size_t n) const
	{
		ensure_calculated(n);
		return cache_.at(n);
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
	 * @param start 起始索引
	 * @param end 截止索引
	 */
	[[nodiscard]] std::span<const T> operator[](size_t start, size_t end) const
	{
//		if(start > end) [[unlikely]] throw std::out_of_range("hyx::autoseq: Invalid range.");
		assert(start <= end && "hyx::autoseq: Invalid range.");
		if(start == end) return {};
		ensure_calculated(end - 1);
		return std::span<const T>(cache_.data() + start, end - start);
	}

	/**
	 * @brief 获取当前已缓存数据的只读视图 (零拷贝)
	 */
	[[nodiscard]] std::span<const T> view() const noexcept
	{
		return std::span(cache_);
	}

	/**
	 * @brief 转换为 vector
	 * 左值调用执行拷贝，右值调用执行移动
	 */
	[[nodiscard]] std::vector<T> snapshot() const &
	{
		return cache_;
	}
	[[nodiscard]] std::vector<T> snapshot() &&
	{
		return std::move(cache_);
	}

	/** @brief 获取当前已缓存的数据项总数 */
	[[nodiscard]] size_t size() const noexcept
	{
		return cache_.size();
	}

	using value_type = T;
	using const_iterator = typename std::vector<T>::const_iterator;

	/** @brief 获取当前已缓存部分的起始迭代器 */
	[[nodiscard]] const_iterator begin() const noexcept
	{
		return cache_.begin();
	}
	/** @brief 获取当前已缓存部分的结束迭代器 */
	[[nodiscard]] const_iterator end() const noexcept
	{
		return cache_.end();
	}
};

} // namespace hyx

#endif // HYX_AUTOSEQ_HPP
