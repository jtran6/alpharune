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

class DocksideLockUp_1059 : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    std::vector<TriggerType> triggerTypes() const override { return {TriggerType::WhenYouPlayMe, TriggerType::WhenIHold}; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (ctx.state.chain.resuming && ctx.state.chain.resuming->fired_trigger == TriggerType::WhenIHold) {
            int raw = 0;
            if (ctx.state.objectExists(ctx.source))
                raw = ctx.state.getObject(ctx.source).card_counters["__locked"];
            if (costDueNow(ctx)) {
                const bool died = ctx.state.objectExists(ctx.source) && killAsCost(ctx, ctx.source);
                if (!died) markCostUnpaid(ctx);
            }
            if (finalizingChoices(ctx)) return;
            if (costUnpaid(ctx) || raw <= 0) return;
            const GameObjectId card = GameObjectId::fromRaw(raw);
            if (!ctx.state.objectExists(card) || ctx.state.getObject(card).isToken()) return;
            if (ctx.state.getObject(card).zone != ZoneType::Banishment) return;
            std::vector<std::string> labels;
            std::vector<std::optional<LocationId>> places;
            const bool deploy = ctx.state.getObject(card).isGear() &&
                                ctx.state.getObject(card).hasKeyword(Keyword::Deploy);
            const bool unit = ctx.state.getObject(card).isUnit();
            if (!deploy) {
                labels.push_back("Your base");
                places.push_back(LocationId{BaseLocation{ctx.controller}});
            }
            if (deploy || unit)
            for (const auto& bf : ctx.state.battlefields) {
                LocationId loc{BattlefieldLocation{bf.id}};
                if (ctx.state.getObject(card).isUnit() &&
                    !ctx.executor.unitMayBePlayedTo(ctx.controller, loc))
                    continue;
                labels.push_back(battlefieldLabel(ctx.state, bf));
                places.push_back(loc);
            }
            if (labels.empty()) return;
            auto where = pickValueAs(ctx, ctx.controller,
                                     "Dockside Lock-Up: play the banished card",
                                     labels, PromptKind::Location, /*slot=*/9);
            if (!where) return;
            if (*where < 0 || *where >= static_cast<int>(places.size())) return;
            ctx.executor.playIgnoringCost(ctx.controller, card, places[static_cast<std::size_t>(*where)]);
            return;
        }
        LocationId here{BaseLocation{ctx.controller}};
        if (ctx.state.objectExists(ctx.source) && ctx.state.getObject(ctx.source).location.has_value())
            here = *ctx.state.getObject(ctx.source).location;
        auto units = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) {
            return u.location.has_value() && *u.location == here;
        });
        auto t = pickTarget(ctx, "Dockside Lock-Up: banish a unit here", units);
        if (finalizingChoices(ctx)) return;
        if (t == kInvalidId) return;
        if (ctx.state.objectExists(ctx.source)) ctx.state.getObject(ctx.source).card_counters["__locked"] = t.raw();
        ctx.executor.banishObject(t);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1059;
        d.def_id = R"RB(rad-136-167)RB";
        d.name = R"RB(Dockside Lock-Up)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-136/167)RB";
        d.collector_number = 136;
        d.artist = R"RB()RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::None;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Device)RB"};
        d.energy_cost = 6;
        d.power_cost = 2;
        d.might = 0;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Deploy);
        d.ability_text = R"RB([Deploy] (Play this only to a battlefield. When an opponent holds here, kill this.)
When you play this, banish a unit here.
When you hold here, kill this to play the banished card, ignoring its cost. (Tokens aren't cards.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/4f477ccc474cb16e4e9d681b388f0673f77fae5b-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1059(CardRegistry& r) {
    r.registerCard(1059, std::make_unique<DocksideLockUp_1059>());
}

}  // namespace riftbound
