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

class SatchelCharge_1106 : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId player,
                                           GameObjectId object) const override {
        return friendlyUnitChoice(state, player, object);
    }
    bool hasLegalTargets(const GameState& state, PlayerId player) const override {
        return !chooseableUnits(state, player, [&](const GameObject& u) {
            return u.controller == player;
        }).empty();
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto units = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) { return u.controller == ctx.controller; });
        auto t = pickTarget(ctx, "Satchel Charge: move a friendly unit", units);
        if (ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7)
            return;
        // Where the unit moves is chosen as the spell is finalized (CR 355.4).
        // The code is stored here, not in pickMode: the Bomb's battlefield is
        // that helper's one mode, asked as the token is played (CR 420.3.b.2).
        int dest = -1;
        bool have_dest = false;
        if (ctx.state.chain.resuming.has_value()) {
            auto& rd = ctx.state.chain.resuming->resume_data;
            while (rd.size() <= 25) rd.push_back(0);
            have_dest = rd[24] != 0;
            dest = rd[25];
        }
        if (!have_dest && t != kInvalidId && ctx.state.objectExists(t) &&
            ctx.state.getObject(t).location.has_value()) {
            const auto codes = moveDests(ctx.state, ctx.state.getObject(t));
            std::vector<std::string> labels;
            labels.reserve(codes.size());
            for (int code : codes) labels.push_back(destLabel(code));
            if (!codes.empty()) {
                auto which = pickValueAs(ctx, ctx.controller, "Satchel Charge: where",
                                         labels, PromptKind::Location, 18);
                if (!which) return;
                if (*which >= 0 && *which < static_cast<int>(codes.size()))
                    dest = codes[static_cast<std::size_t>(*which)];
            }
            if (ctx.state.chain.resuming.has_value()) {
                auto& rd = ctx.state.chain.resuming->resume_data;
                while (rd.size() <= 25) rd.push_back(0);
                rd[24] = 1;
                rd[25] = dest;
                have_dest = true;
            }
        }
        if (finalizingChoices(ctx)) return;
        bool already = false;
        if (ctx.state.chain.resuming.has_value())
            already = ctx.state.chain.resuming->resume_data[20] != 0;
        if (!already && have_dest && dest >= 0 && t != kInvalidId &&
            ctx.state.objectExists(t) && ctx.state.getObject(t).location.has_value()) {
            const auto& unit = ctx.state.getObject(t);
            bool same = false;
            if (dest <= 0) same = unit.isAtBase();
            else if (const auto* bf = std::get_if<BattlefieldLocation>(&*unit.location))
                same = static_cast<int>(bf->id.raw()) == dest - 1;
            if (!same) moveTo(ctx, t, dest);
            if (ctx.state.chain.resuming.has_value())
                ctx.state.chain.resuming->resume_data[20] = 1;
        }
        playBombToken(*this, ctx, "Satchel Charge: play a Bomb");
    }

private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 1106;
        d.def_id = R"RB(rad-096-167)RB";
        d.name = R"RB(Satchel Charge)RB";
        d.set_code = R"RB(RAD)RB";
        d.set_name = R"RB(Radiance)RB";
        d.public_code = R"RB(RAD-096/167)RB";
        d.collector_number = 96;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::None;
        d.domains = {Domain::Chaos};
        d.tags = {};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 0;
        d.rarity = Rarity::Common;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Move a friendly unit, then play a Bomb gear token to a battlefield. (It has [Deploy]. When it dies, deal 2 to an enemy there and kill your other Bombs there.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b07c26438f600b87c92e4ed778a1c8b073b47f5c-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_1106(CardRegistry& r) {
    r.registerCard(1106, std::make_unique<SatchelCharge_1106>());
}

}  // namespace riftbound
