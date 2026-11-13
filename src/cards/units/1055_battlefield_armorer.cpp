#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/deck_ops.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class BattlefieldArmorer_1055 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "When a friendly gear at a battlefield dies, you may pay [1] to return
    // it to its owner's hand." The gear has already left the board when this
    // is judged (LeftBoardEvent): "at a battlefield" is where it died
    // (last_known), and the return is from its owner's trash. A token ceased
    // to exist and is not in a trash, so it is not offered.
    TriggerType triggerType() const override { return TriggerType::WhenAFriendlyGearDies; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    static bool inOwnerTrash(const GameState& state, GameObjectId gear) {
        if (gear == kInvalidId || !state.objectExists(gear)) return false;
        const auto& trash = state.player(state.getObject(gear).owner).trash;
        return std::find(trash.begin(), trash.end(), gear) != trash.end();
    }
    bool triggerCondition(const GameState& state, GameObjectId /*source*/, PlayerId /*controller*/,
                          TriggerType /*trigger*/, GameObjectId subject) const override {
        if (!inOwnerTrash(state, subject)) return false;
        const auto& gear = state.getObject(subject);
        if (!gear.last_known.valid || !gear.last_known.died || !gear.last_known.location.has_value())
            return false;
        return std::holds_alternative<BattlefieldLocation>(*gear.last_known.location);
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const GameObjectId gear = ctx.state.chain.resuming
            ? ctx.state.chain.resuming->triggering_subject : kInvalidId;
        auto still = [&] {
            return inOwnerTrash(ctx.state, gear) && canPayEnergy(ctx, ctx.controller, 1);
        };
        const int pay = confirmOptional(ctx,
            "Battlefield Armorer: pay [1] to return the gear to its owner's hand?", still);
        if (pay < 1) return;
        if (costDueNow(ctx) && !payEnergy(ctx, ctx.controller, 1)) return;
        if (finalizingChoices(ctx)) return;
        if (!inOwnerTrash(ctx.state, gear)) return;
        auto& trash = ctx.state.player(ctx.state.getObject(gear).owner).trash;
        trash.erase(std::remove(trash.begin(), trash.end(), gear), trash.end());
        deck::putInHand(ctx.state, gear);
        RIFT_TRACE(ctx.events, "BATTLEFIELD ARMORER: paid [1], returned " +
                             ctx.state.getObject(gear).name + " to its owner's hand");
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1055;
        d.def_id = R"RB(rad-130-167)RB";
        d.name = R"RB(Battlefield Armorer)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-130/167)RB";
        d.collector_number = 130;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::None;
        d.domains = {Domain::Order};
        d.tags = {};
        d.energy_cost = 3;
        d.power_cost = 0;
        d.might = 3;
        d.rarity = Rarity::Rare;

        d.ability_text = R"RB(When a friendly gear at a battlefield dies, you may pay [1] to return it to its owner's hand.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/eb04c39763de8fb8e56153d1f0e4f799e15dced9-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1055(CardRegistry& r) {
    r.registerCard(1055, std::make_unique<BattlefieldArmorer_1055>());
}

}  // namespace riftbound
