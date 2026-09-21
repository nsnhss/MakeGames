#include "test_framework.h"
#include "resource/item_database.h"
#include "resource/actor_database.h"

using namespace mg;

MG_TEST(item_database_roundtrip) {
    ItemDatabase db;
    ItemDatabase::ItemDef potion;
    potion.id = "potion";
    potion.name = "药水";
    potion.description = "恢复 50 HP";
    potion.price = 50;
    db.items[potion.id] = potion;

    ItemDatabase::ItemDef sword;
    sword.id = "sword";
    sword.name = "铁剑";
    sword.price = 300;
    db.items[sword.id] = sword;

    nlohmann::json j = db.ToJson();
    auto restored = ItemDatabase::FromJson(j);
    MG_EXPECT(restored.has_value());
    MG_EXPECT_EQ(restored->items.size(), 2u);

    const auto* p = restored->Find("potion");
    MG_EXPECT(p != nullptr);
    MG_EXPECT_EQ(p->name, std::string("药水"));
    MG_EXPECT_EQ(p->description, std::string("恢复 50 HP"));
    MG_EXPECT_EQ(p->price, 50);

    const auto* s = restored->Find("sword");
    MG_EXPECT(s != nullptr);
    MG_EXPECT_EQ(s->price, 300);

    MG_EXPECT(restored->Find("nonexistent") == nullptr);
}

MG_TEST(actor_database_roundtrip) {
    ActorDatabase db;
    ActorDatabase::ActorDef hero;
    hero.id = "hero";
    hero.name = "勇者";
    hero.hp = 100;
    hero.mp = 30;
    hero.atk = 15;
    hero.def = 10;
    hero.sprite = "assets/hero.png";
    db.actors[hero.id] = hero;

    nlohmann::json j = db.ToJson();
    auto restored = ActorDatabase::FromJson(j);
    MG_EXPECT(restored.has_value());
    MG_EXPECT_EQ(restored->actors.size(), 1u);

    const auto* h = restored->Find("hero");
    MG_EXPECT(h != nullptr);
    MG_EXPECT_EQ(h->name, std::string("勇者"));
    MG_EXPECT_EQ(h->hp, 100);
    MG_EXPECT_EQ(h->mp, 30);
    MG_EXPECT_EQ(h->atk, 15);
    MG_EXPECT_EQ(h->def, 10);
    MG_EXPECT_EQ(h->sprite, std::string("assets/hero.png"));
}

MG_TEST(item_database_empty_roundtrip) {
    ItemDatabase db;
    MG_EXPECT(db.IsEmpty());
    auto restored = ItemDatabase::FromJson(db.ToJson());
    MG_EXPECT(restored.has_value());
    MG_EXPECT(restored->IsEmpty());
}

MG_TEST_MAIN()
