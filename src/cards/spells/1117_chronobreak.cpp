#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class Chronobreak_1117 : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    AdditionalPlayCost additionalPlayCost(const GameState&, const CardDB&, PlayerId,
                                          GameObjectId) const override {
        return {.exists = true, .mandatory = true, .payable = true, .best = {}};
    }
    PlayCostAdjust payAdditionalPlayCost(
        CardContext& ctx, const std::function<bool(const PlayCostAdjust&)>&) override {
        const int cap = ctx.executor.maxPayableAnyPower(ctx.controller, 20, SpendFor::Spell);
        std::vector<std::string> labels;
        labels.reserve(static_cast<std::size_t>(cap) + 1);
        for (int i = 0; i <= cap; ++i)
            labels.push_back("Pay " + std::to_string(i) + " Power");
        int x = ctx.executor.chooseValueNow(ctx.controller, labels,
                                            "Chronobreak: pay any amount of Power",
                                            PromptKind::XAmount);
        if (x < 0 || x > cap) x = 0;
        if (x > 0 && !ctx.executor.payResources(ctx.controller, 0, x, resource::kAnyDomain, SpendFor::Spell))
            x = 0;
        if (ctx.state.objectExists(ctx.source))
            ctx.state.getObject(ctx.source).card_counters["__chrono_x"] = x;
        return {};
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        const int x = self.card_counters["__chrono_x"];
        if (self.card_counters["__chrono_banished"] == 0) {
            auto units = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) {
                return u.controller == ctx.controller;
            });
            auto picked = selectSeveral(ctx, "Chronobreak: banish units you control",
                                        units, std::max(0, x), /*first_slot=*/9);
            if (!picked) return;
            int n = 0;
            for (auto id : *picked) {
                if (!ctx.state.objectExists(id)) continue;
                const auto& unit = ctx.state.getObject(id);
                const LocationId from = unit.location.value_or(LocationId{BaseLocation{unit.owner}});
                self.card_counters["__cu" + std::to_string(n)] = id.raw();
                self.card_counters["__cl" + std::to_string(n)] = locationCode(from);
                ++n;
                ctx.executor.banishObject(id);
            }
            self.card_counters["__cn"] = n;
            self.card_counters["__chrono_banished"] = 1;
        }
        const int n = self.card_counters["__cn"];
        std::vector<PlayerId> order{ctx.state.turn.turn_player, opponent(ctx.state.turn.turn_player)};
        for (PlayerId who : order) {
            for (int i = 0; i < n; ++i) {
                const GameObjectId id = GameObjectId::fromRaw(self.card_counters["__cu" + std::to_string(i)]);
                if (!ctx.state.objectExists(id) || ctx.state.getObject(id).owner != who) continue;
                const int code = self.card_counters["__cl" + std::to_string(i)];
                LocationId same = code == 0
                    ? LocationId{BaseLocation{who}}
                    : LocationId{BattlefieldLocation{BattlefieldId::fromRaw(code - 1)}};
                auto where = pickValueAs(ctx, who, "Chronobreak: play it to the same location or your base",
                                         {"The same location", "Your base"},
                                         PromptKind::Location, static_cast<std::size_t>(11 + 2 * i));
                if (!where) return;
                const LocationId dest = *where == 1 ? LocationId{BaseLocation{who}} : same;
                ctx.executor.playIgnoringCost(who, id, dest);
            }
        }
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1117;
        d.def_id = R"RB(rad-140-167)RB";
        d.name = R"RB(Chronobreak)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-140/167)RB";
        d.collector_number = 140;
        d.artist = R"RB(Luscinia Studio)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Fury, Domain::Mind};
        d.tags = {R"RB(Ekko)RB"};
        d.energy_cost = 2;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Epic;

        d.ability_text = R"RB(As an additional cost to play this, pay any amount of Power.
Banish that many units you control, then for each, its owner plays that unit back to the same location or to their base, ignoring its cost. (You choose the order.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/af2123a99f3e3e7e6c9b101c5fc2c9b0ff56e096-744x1040.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1117(CardRegistry& r) {
    r.registerCard(1117, std::make_unique<Chronobreak_1117>());
}

}  // namespace riftbound
