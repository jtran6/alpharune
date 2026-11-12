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

class ForDemacia_1111 : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState&, PlayerId, GameObjectId) const override {
        return FinalizeChoice::No;
    }
    bool hasLegalTargets(const GameState& state, PlayerId) const override {
        return !state.battlefields.empty();
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        std::vector<std::string> labels;
        for (const auto& bf : ctx.state.battlefields) {
            labels.push_back(ctx.state.objectExists(bf.card_object_id)
                ? ctx.state.getObject(bf.card_object_id).name.get()
                : "Battlefield #" + std::to_string(static_cast<int>(bf.id.raw())));
        }
        if (labels.empty()) { if (finalizingChoices(ctx)) return; return; }
        int mode = pickMode(ctx, "For Demacia!: a battlefield", static_cast<int>(labels.size()), labels);
        if (mode < 0) return;
        if (finalizingChoices(ctx)) return;
        LocationId there{BattlefieldLocation{ctx.state.battlefields[mode].id}};
        std::vector<GameObjectId> friends;
        for (auto& [id, obj] : ctx.state.objects.atLocation(there))
            if (obj.isUnit() && obj.controller == ctx.controller) friends.push_back(id);
        for (auto id : friends) ctx.executor.giveTemporaryKeyword(id, Keyword::Shield, 2);
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1111;
        d.def_id = R"RB(rad-117-167)RB";
        d.name = R"RB(For Demacia!)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-117/167)RB";
        d.collector_number = 117;
        d.artist = R"RB(Original Force)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Order};
        d.tags = {};
        d.energy_cost = 3;
        d.power_cost = 0;
        d.might = 0;
        d.rarity = Rarity::Common;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Choose a battlefield. Give friendly units there [Shield 2] this turn. (+2 [M] while they're defenders.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/5d119653fcba423c150815de3a412922c4e481e8-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1111(CardRegistry& r) {
    r.registerCard(1111, std::make_unique<ForDemacia_1111>());
}

}  // namespace riftbound
