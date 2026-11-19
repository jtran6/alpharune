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
    // The units are targets, chosen as the spell is played (CR 355.5). X is
    // chosen then too; this pays the amount that pass stored. Neither choice
    // reduces the spell's cost.
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState&, PlayerId,
                                           GameObjectId) const override {
        return FinalizeChoice::No;
    }
    PlayCostAdjust payAdditionalPlayCost(
        CardContext& ctx, const std::function<bool(const PlayCostAdjust&)>&) override {
        int x = 0;
        if (ctx.state.objectExists(ctx.source))
            x = ctx.state.getObject(ctx.source).card_counters["__chrono_x"];
        if (x < 0) x = 0;
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
        // The banish picks occupy 220+2*i. The play-back order prompts,
        // location prompts, done-flags and the saved units each follow that
        // in their own span, so the 21st unit is not stored in the first
        // order prompt (cells 120 and 121) and a longer list does not run
        // into the location prompt or the done-flags.
        auto playBack = [](int count) {
            struct Span { int order, location, done, store; };
            Span s;
            s.order = 220 + 2 * count;
            s.location = s.order + 2 * count;
            s.done = s.location + 2 * count;
            s.store = s.done + count;
            return s;
        };
        if (ri.resume_data[8] == 0) {
            int x = 0;
            if (ri.finalizing) {
                const int cap = ctx.executor.maxPayableAnyPower(ctx.controller, 100000, SpendFor::Spell);
                x = pickXAmount(ctx, "Chronobreak: pay any amount of Power", 0, cap);
                if (x < 0) return;
                if (ctx.state.objectExists(ctx.source))
                    ctx.state.getObject(ctx.source).card_counters["__chrono_x"] = x;
            } else if (ctx.state.objectExists(ctx.source)) {
                x = ctx.state.getObject(ctx.source).card_counters["__chrono_x"];
            }
            if (x < 0) x = 0;
            auto units = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) {
                return u.controller == ctx.controller;
            });
            std::vector<GameObjectId> picked;
            for (int i = 0; i < x; ++i) {
                std::vector<GameObjectId> left;
                for (auto id : units)
                    if (std::find(picked.begin(), picked.end(), id) == picked.end())
                        left.push_back(id);
                if (left.empty()) break;
                // Slot 40 is [Show Off]. These picks stay clear of it.
                auto one = selectObject(ctx, "Chronobreak: banish a unit you control",
                                       left, static_cast<std::size_t>(220 + 2 * i), false);
                if (!one) return;
                if (*one == kInvalidId) break;
                if (ri.finalizing &&
                    std::find(ri.chosen_objects.begin(), ri.chosen_objects.end(), *one) ==
                        ri.chosen_objects.end())
                    commitChosenObject(ctx, *one);
                picked.push_back(*one);
            }
            if (finalizingChoices(ctx)) return;
            struct Saved { int32_t id; int code; };
            std::vector<Saved> saved;
            for (auto id : picked) {
                if (!ctx.state.stillTheChosenObject(ri, id)) continue;
                if (!ctx.state.objectExists(id)) continue;
                const auto& unit = ctx.state.getObject(id);
                if (!unit.location.has_value()) continue;
                saved.push_back({static_cast<int32_t>(id.raw()),
                                 encodeChronoLocation(*unit.location)});
                ctx.executor.banishObject(id);
            }
            const int n = static_cast<int>(saved.size());
            const auto span = playBack(n);
            ensure(static_cast<std::size_t>(span.store + 2 * n + 2));
            for (int i = 0; i < n; ++i) {
                ri.resume_data[span.store + 2 * i] = saved[i].id;
                ri.resume_data[span.store + 2 * i + 1] = saved[i].code;
            }
            ri.resume_data[9] = n;
            ri.resume_data[8] = 1;
            ri.resume_data[11] = 0;
        }
        const int n = ri.resume_data[9];
        const auto span = playBack(n);
        ensure(static_cast<std::size_t>(span.store + 2 * n + 2));
        const std::vector<PlayerId> order{ctx.state.turn.turn_player,
                                          opponent(ctx.state.turn.turn_player)};
        while (ri.resume_data[11] < n) {
            const int step = ri.resume_data[11];
            int chosen = -1;
            PlayerId who = order.front();
            for (PlayerId owner : order) {
                std::vector<int> idx;
                for (int i = 0; i < n; ++i) {
                    if (ri.resume_data[span.done + i] != 0) continue;
                    const GameObjectId id = GameObjectId::fromRaw(ri.resume_data[span.store + 2 * i]);
                    if (!ctx.state.objectExists(id) || ctx.state.getObject(id).owner != owner) continue;
                    // A token banished this way has ceased to exist (CR 186.1)
                    // and is not played back. One that is already gone is the
                    // same.
                    if (ctx.state.getObject(id).isToken()) {
                        ri.resume_data[span.done + i] = 1;
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
                    opts.push_back(GameObjectId::fromRaw(ri.resume_data[span.store + 2 * i]));
                auto next = selectObjectAs(ctx, who, "Chronobreak: play this unit next",
                                           opts, static_cast<std::size_t>(span.order + 2 * step));
                if (!next) return;
                for (int i : idx)
                    if (GameObjectId::fromRaw(ri.resume_data[span.store + 2 * i]) == *next) chosen = i;
                if (chosen < 0) chosen = idx.front();
                break;
            }
            if (chosen < 0) break;
            const GameObjectId id = GameObjectId::fromRaw(ri.resume_data[span.store + 2 * chosen]);
            const int code = ri.resume_data[span.store + 2 * chosen + 1];
            // "The same location" only when the owner can play a unit there.
            // Another player's base, a battlefield units can't be played to,
            // and "units only to base" are not that choice. A failed play is
            // not rewritten as the owner's base.
            const bool same_ok = chronoSameLocationPlayable(ctx, code, who);
            std::vector<std::string> places;
            if (same_ok) places.push_back("The same location");
            places.push_back("Your base");
            auto where = pickValueAs(ctx, who, "Chronobreak: play it to the same location or your base",
                                     places, PromptKind::Location,
                                     static_cast<std::size_t>(span.location + 2 * step));
            if (!where) return;
            const bool picked_same = same_ok && *where == 0;
            LocationId dest = picked_same
                ? decodeChronoLocation(code, who)
                : LocationId{BaseLocation{who}};
            bool played = false;
            if (ctx.state.objectExists(id) && !ctx.state.getObject(id).isToken())
                played = ctx.executor.playIgnoringCost(who, id, dest);
            (void)played;
            ri.resume_data[span.done + chosen] = 1;
            ri.resume_data[11] = step + 1;
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
    static bool chronoSameLocationPlayable(CardContext& ctx, int code, PlayerId owner) {
        return ctx.executor.unitMayBePlayedTo(owner, decodeChronoLocation(code, owner));
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
