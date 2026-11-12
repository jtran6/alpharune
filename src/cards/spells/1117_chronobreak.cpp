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
        // The Power is paid ahead of the spell's [2] Energy. A ready rune is
        // exhausted for that Energy before it is recycled, so paying X equal
        // to the number of ready runes still leaves the spell payable.
        if (x > 0) {
            ctx.executor.setOwedCost(EffectExecutor::OwedCost{
                ctx.controller, def().energy_cost, 0,
                resource::maskOf(def().domains), SpendFor::Spell});
            const bool paid = ctx.executor.payAnyPower(ctx.controller, x, SpendFor::Spell);
            ctx.executor.setOwedCost(std::nullopt);
            if (!paid) x = 0;
        }
        if (ctx.state.objectExists(ctx.source))
            ctx.state.getObject(ctx.source).card_counters["__chrono_x"] = x;
        return {};
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.chain.resuming.has_value()) return;
        auto& ri = *ctx.state.chain.resuming;
        auto ensure = [&](std::size_t n) {
            while (ri.resume_data.size() < n) ri.resume_data.push_back(0);
        };
        ensure(12);
        // Progress lives on the chain item. A later cast of this object starts
        // with an empty item, so a previous banish cannot be replayed.
        if (ri.resume_data[8] == 0) {
            int x = 0;
            if (ctx.state.objectExists(ctx.source))
                x = ctx.state.getObject(ctx.source).card_counters["__chrono_x"];
            if (x < 0) x = 0;
            auto units = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) {
                return u.controller == ctx.controller && !u.isToken();
            });
            std::vector<GameObjectId> picked;
            for (int i = 0; i < x; ++i) {
                std::vector<GameObjectId> left;
                for (auto id : units)
                    if (std::find(picked.begin(), picked.end(), id) == picked.end())
                        left.push_back(id);
                if (left.empty()) break;
                auto one = selectObject(ctx, "Chronobreak: banish a unit you control",
                                       left, static_cast<std::size_t>(40 + 2 * i), false);
                if (!one) return;
                if (*one == kInvalidId) break;
                picked.push_back(*one);
            }
            ensure(80 + static_cast<std::size_t>(2 * static_cast<int>(picked.size()) + 2));
            int n = 0;
            for (auto id : picked) {
                if (!ctx.state.objectExists(id)) continue;
                const auto& unit = ctx.state.getObject(id);
                if (unit.isToken() || !unit.location.has_value()) continue;
                ri.resume_data[80 + 2 * n] = static_cast<int32_t>(id.raw());
                ri.resume_data[81 + 2 * n] = encodeChronoLocation(*unit.location);
                ctx.executor.banishObject(id);
                ++n;
            }
            ri.resume_data[9] = n;
            ri.resume_data[8] = 1;
            ri.resume_data[11] = 0;
        }
        const int n = ri.resume_data[9];
        ensure(200 + static_cast<std::size_t>(n) + 2);
        const std::vector<PlayerId> order{ctx.state.turn.turn_player,
                                          opponent(ctx.state.turn.turn_player)};
        while (ri.resume_data[11] < n) {
            const int step = ri.resume_data[11];
            int chosen = -1;
            PlayerId who = order.front();
            for (PlayerId owner : order) {
                std::vector<int> idx;
                for (int i = 0; i < n; ++i) {
                    if (ri.resume_data[200 + i] != 0) continue;
                    const GameObjectId id = GameObjectId::fromRaw(ri.resume_data[80 + 2 * i]);
                    if (!ctx.state.objectExists(id) || ctx.state.getObject(id).owner != owner) continue;
                    if (ctx.state.getObject(id).isToken()) {
                        ri.resume_data[200 + i] = 1;
                        continue;
                    }
                    idx.push_back(i);
                }
                if (idx.empty()) continue;
                who = owner;
                if (idx.size() == 1) {
                    chosen = idx.front();
                    break;
                }
                std::vector<GameObjectId> opts;
                for (int i : idx)
                    opts.push_back(GameObjectId::fromRaw(ri.resume_data[80 + 2 * i]));
                auto next = selectObjectAs(ctx, who, "Chronobreak: play this unit next",
                                           opts, static_cast<std::size_t>(120 + 2 * step));
                if (!next) return;
                for (int i : idx)
                    if (GameObjectId::fromRaw(ri.resume_data[80 + 2 * i]) == *next) chosen = i;
                if (chosen < 0) chosen = idx.front();
                break;
            }
            if (chosen < 0) break;
            const GameObjectId id = GameObjectId::fromRaw(ri.resume_data[80 + 2 * chosen]);
            const int code = ri.resume_data[81 + 2 * chosen];
            auto where = pickValueAs(ctx, who, "Chronobreak: play it to the same location or your base",
                                     {"The same location", "Your base"},
                                     PromptKind::Location, static_cast<std::size_t>(160 + 2 * step));
            if (!where) return;
            const LocationId dest = *where == 1
                ? LocationId{BaseLocation{who}}
                : decodeChronoLocation(code, who);
            ri.resume_data[200 + chosen] = 1;
            ri.resume_data[11] = step + 1;
            if (ctx.state.objectExists(id) && !ctx.state.getObject(id).isToken())
                ctx.executor.playIgnoringCost(who, id, dest);
        }
    }

private:
    static int encodeChronoLocation(const LocationId& loc) {
        if (const auto* base = std::get_if<BaseLocation>(&loc))
            return -2 - static_cast<int>(base->player);
        if (const auto* bf = std::get_if<BattlefieldLocation>(&loc))
            return bf->id.raw() + 1;
        return -2;
    }
    static LocationId decodeChronoLocation(int code, PlayerId owner) {
        if (code > 0)
            return LocationId{BattlefieldLocation{BattlefieldId::fromRaw(code - 1)}};
        const int p = code <= -2 ? -code - 2 : static_cast<int>(owner);
        return LocationId{BaseLocation{static_cast<PlayerId>(p < 0 ? 0 : p)}};
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
