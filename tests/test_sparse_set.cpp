#include "test_framework.h"
#include "scene/sparse_set.h"

using mg::scene::SparseSet;

MG_TEST(sparse_set_add_has_get) {
    SparseSet<int> set;
    MG_EXPECT(!set.Has(1));
    set.Add(1, 100);
    set.Add(5, 500);
    set.Add(3, 300);
    MG_EXPECT(set.Has(1));
    MG_EXPECT(set.Has(5));
    MG_EXPECT(!set.Has(2));
    MG_EXPECT_EQ(set.Count(), 3u);
    MG_EXPECT_EQ(set.Get(5), 500);
    MG_EXPECT_EQ(*set.TryGet(3), 300);
    MG_EXPECT(set.TryGet(42) == nullptr);
}

MG_TEST(sparse_set_remove_keeps_dense_integrity) {
    SparseSet<int> set;
    for (uint32_t i = 1; i <= 10; ++i) set.Add(i, (int)i * 10);

    // 删除中间元素，触发"最后一个元素填补空缺"路径
    set.Remove(5);
    MG_EXPECT(!set.Has(5));
    MG_EXPECT_EQ(set.Count(), 9u);

    // 其余元素必须仍然可达（swap 后 sparse 映射正确）
    for (uint32_t i = 1; i <= 10; ++i) {
        if (i == 5) continue;
        MG_EXPECT(set.Has(i));
        MG_EXPECT_EQ(set.Get(i), (int)i * 10);
    }

    // 删除头部与尾部
    set.Remove(1);
    set.Remove(10);
    MG_EXPECT_EQ(set.Count(), 7u);
    MG_EXPECT(!set.Has(1));
    MG_EXPECT(!set.Has(10));
}

MG_TEST(sparse_set_remove_nonexistent_is_noop) {
    SparseSet<int> set;
    set.Add(1, 1);
    set.Remove(99); // 不应崩溃、不应改变内容
    MG_EXPECT_EQ(set.Count(), 1u);
    MG_EXPECT(set.Has(1));
}

MG_TEST(sparse_set_iteration_matches_entities) {
    SparseSet<int> set;
    set.Add(7, 70);
    set.Add(2, 20);
    set.Add(9, 90);
    const auto& entities = set.GetEntities();
    MG_EXPECT_EQ(entities.size(), 3u);
    // dense 顺序与 entities 一一对应
    size_t idx = 0;
    for (int v : set) {
        MG_EXPECT_EQ(v, (int)entities[idx] * 10);
        ++idx;
    }
}

MG_TEST(sparse_set_clear) {
    SparseSet<int> set;
    set.Add(1, 1);
    set.Add(2, 2);
    set.Clear();
    MG_EXPECT_EQ(set.Count(), 0u);
    MG_EXPECT(!set.Has(1));
    // Clear 后可以重新 Add 同 ID（sparse 已重置）
    set.Add(1, 111);
    MG_EXPECT_EQ(set.Get(1), 111);
}
