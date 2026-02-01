#include <iostream>
#include <vector>
#include <format>
#include <stdint.h>
#include "hyx_autoseq.hpp"

int main()
{
	// ---------------------------------------------------------
	// 1. 构造与初始化
	// 使用 lambda 定义递推公式: S_n = S_{n-1} + S_{n-2}
	// 并提供初始项: S_0=0, S_1=1
	// ---------------------------------------------------------
	hyx::autoseq<uint64_t> fib([](auto F)
	{
		return F[F.n() - 1] + F[F.n() - 2];
	}, 0, 1);
	hyx::autoseq<uint64_t> sum([&fib](auto F)
	{
		return F[F.n() - 1] + fib[F.n()];
	}, 0);

	std::cout << "--- Basic Access ---\n";
	// ---------------------------------------------------------
	// 2. 索引访问 (operator[] 或 at())
	// ---------------------------------------------------------
	std::cout << std::format("S[5]  = {}\n", sum[5]); // 应为 12
	std::cout << std::format("S[10] = {}\n", sum.at(10)); // 应为 143
	std::cout << std::format("Current Cache Size: {}\n", sum.size()); // 此时应为 11 (0 到 10)

	std::cout << "\n--- Range-based Access ---\n";
	// ---------------------------------------------------------
	// 3. 多下标切片访问 [start, end)
	// ---------------------------------------------------------
	auto slice(sum[3, 8]);
	std::cout << "slice S[3, 8): ";
	for(const auto& val : slice)
		std::cout << val << ' '; // 4 7 12 20 33
	std::cout << '\n';

	std::cout << "\n--- Preloading and Capacity Management ---\n";
	// ---------------------------------------------------------
	// 4. 预计算与预留空间
	// ---------------------------------------------------------
	sum.reserve(100); // 预留内存
	sum.prefetch_up_to(20); // 强制计算到第 20 项
	std::cout << std::format("Size After Precomputation: {}\n", sum.size());

	std::cout << "\n--- Views and Iterators ---\n";
	// ---------------------------------------------------------
	// 5. 视图 (span) 与迭代器 (只遍历已缓存部分)
	// ---------------------------------------------------------
	auto v(sum.view());
	std::cout << std::format("View front: {}, back: {}\n", v.front(), v.back());

	std::cout << "Iterate over cached data using iterators: ";
	for(auto it(sum.begin()); it != sum.end(); ++it)
	{
		if(*it > 100) break;
		std::cout << std::format("{} ", *it);
	}
	std::cout << "\nIterate over all data: ";
	for(auto &it : sum)
	{
		std::cout << std::format("{} ", it);
	}
	std::cout << '\n';

	std::cout << "\n--- Snapshots and Move Semantics ---\n";
	// ---------------------------------------------------------
	// 6. Snapshot (拷贝或移动)
	// ---------------------------------------------------------
	// 左值调用：执行拷贝
	std::vector<uint64_t> copy_vec(sum.snapshot());
	std::cout << std::format("Snapshot Copy Size: {}\n", copy_vec.size());

	// 右值调用：执行移动 (注意：移动后原 fib_sum 的 cache 会失效，通常用于生命周期结束时)
	auto moved_vec(std::move(sum).snapshot());
	std::cout << std::format("Vector size after move: {}\n", moved_vec.size());
	// 此时 fib_sum 的 cache_ 已被置空
}
