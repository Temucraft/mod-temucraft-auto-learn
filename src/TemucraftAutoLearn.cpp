/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

// Learning new class spells without visiting a trainer, as each player chooses. What can be learned is read from the
// class trainers themselves (trainer / trainer_spell, Type 0), so every change made there (other levels, new spells,
// other modules' trainer rules) applies here too. Optionally at the trainers' price.
//
// - Per player on or off (.temucraft autolearn on|off, the spellbook's checkbox, or at a class trainer); off by default.
// - When on: new spells are learned on level up and at login.
// - Anytime: ".temucraft autolearn now", the spellbook's "Train" button or a class trainer's "Teach me everything" learn all
//   that is missing at once.
// - Spellbook button and checkbox: addon messages, prefix TAL.

#include "Chat.h"
#include "CommandScript.h"
#include "Config.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "TemucraftAddonMessage.h"
#include "TemucraftModuleText.h"
#include "Trainer.h"
#include <algorithm>
#include <map>
#include <unordered_map>
#include <vector>

using namespace Acore::ChatCommands;

namespace
{
    constexpr char const* MODULE = "mod-temucraft-auto-learn";
    constexpr char const* PREFIX = "TAL";
    constexpr uint32 SENDER_AUTO_LEARN = 0x7A1E;
    enum Action : uint32 { ACTION_LEARN_ALL = 1, ACTION_TURN_ON = 2, ACTION_TURN_OFF = 3 };

    enum Text : uint32
    {
        T_LEARNED = 1, T_NOTHING, T_CANT_AFFORD, T_ON, T_OFF, T_NO_CHOICE, T_STATUS, T_GOSSIP_LEARN, T_GOSSIP_ON,
        T_GOSSIP_OFF, T_DISABLED, T_LIST_HEAD, T_LIST_ROW, T_HELP
    };
    char const* Say(Player* player, uint32 id, char const* fallback) { return TemucraftModuleText(MODULE, player, id, fallback); }

    struct Settings
    {
        bool enable = true;
        bool defaultOn = false;
        bool allowChoice = true;
        bool cost = false;
        uint8 maxLevel = 80;
        bool atLogin = true;
    } settings;

    struct ClassSpell
    {
        uint32 trainerId;
        Trainer::Spell spell;
    };
    std::unordered_map<uint8, std::vector<ClassSpell>> classSpells;   // class -> spells its trainers teach
    std::unordered_map<uint32, bool> choice;                           // player guid -> on/off (absent: default)

    void LoadSettings()
    {
        settings.enable = sConfigMgr->GetOption<bool>("Temucraft.AutoLearn.Enable", true);
        settings.defaultOn = sConfigMgr->GetOption<bool>("Temucraft.AutoLearn.Default", false);
        settings.allowChoice = sConfigMgr->GetOption<bool>("Temucraft.AutoLearn.AllowChoice", true);
        settings.cost = sConfigMgr->GetOption<bool>("Temucraft.AutoLearn.Cost", false);
        settings.maxLevel = uint8(std::clamp<uint32>(sConfigMgr->GetOption<uint32>("Temucraft.AutoLearn.MaxLevel", 80), 1, 255));
        settings.atLogin = sConfigMgr->GetOption<bool>("Temucraft.AutoLearn.AtLogin", true);
    }

    void LoadTrainerSpells()
    {
        classSpells.clear();
        QueryResult result = WorldDatabase.Query("SELECT t.`Requirement`, ts.`TrainerId`, ts.`SpellId`, ts.`MoneyCost`, ts.`ReqSkillLine`, "
            "ts.`ReqSkillRank`, ts.`ReqAbility1`, ts.`ReqAbility2`, ts.`ReqAbility3`, ts.`ReqLevel` FROM `trainer` t "
            "JOIN `trainer_spell` ts ON ts.`TrainerId` = t.`Id` WHERE t.`Type` = 0");
        if (!result)
            return;
        std::unordered_map<uint8, std::map<uint32, ClassSpell>> byClass;
        uint32 count = 0;
        do
        {
            Field* f = result->Fetch();
            uint8 cls = uint8(f[0].Get<uint32>());
            ClassSpell cs;
            cs.trainerId = f[1].Get<uint32>();
            cs.spell.SpellId = f[2].Get<uint32>();
            cs.spell.MoneyCost = f[3].Get<uint32>();
            cs.spell.ReqSkillLine = f[4].Get<uint32>();
            cs.spell.ReqSkillRank = f[5].Get<uint32>();
            cs.spell.ReqAbility = { f[6].Get<uint32>(), f[7].Get<uint32>(), f[8].Get<uint32>() };
            cs.spell.ReqLevel = f[9].Get<uint8>();
            if (!cls || cls >= MAX_CLASSES || !sSpellMgr->GetSpellInfo(cs.spell.SpellId))
                continue;
            auto [itr, added] = byClass[cls].try_emplace(cs.spell.SpellId, cs);
            if (!added && cs.spell.MoneyCost < itr->second.spell.MoneyCost)
                itr->second = cs;
            count += added;
        } while (result->NextRow());

        for (auto& [cls, spells] : byClass)
        {
            std::vector<ClassSpell>& list = classSpells[cls];
            for (auto& [id, cs] : spells)
                list.push_back(cs);
            std::sort(list.begin(), list.end(), [](ClassSpell const& a, ClassSpell const& b)
            {
                return a.spell.ReqLevel != b.spell.ReqLevel ? a.spell.ReqLevel < b.spell.ReqLevel : a.spell.SpellId < b.spell.SpellId;
            });
        }
        LOG_INFO("server.loading", "{}: {} class trainer spells.", MODULE, count);
    }

    bool IsOn(Player* player)
    {
        if (!settings.enable)
            return false;
        if (settings.allowChoice)
            if (auto itr = choice.find(player->GetGUID().GetCounter()); itr != choice.end())
                return itr->second;
        return settings.defaultOn;
    }

    // As a class trainer would show it to this player (and other modules' trainer rules through the core's hook).
    bool CanLearnNow(Player* player, ClassSpell const& cs)
    {
        Trainer::Spell const& s = cs.spell;
        Trainer::SpellState state = Trainer::SpellState::Available;
        SpellInfo const* info = sSpellMgr->GetSpellInfo(s.SpellId);
        if (!info || player->HasSpell(s.SpellId))
            state = Trainer::SpellState::Known;
        else if (!player->IsSpellFitByClassAndRace(s.SpellId)
            || (s.ReqSkillLine && player->GetBaseSkillValue(s.ReqSkillLine) < s.ReqSkillRank)
            || player->GetLevel() < s.ReqLevel || s.ReqLevel > settings.maxLevel)
            state = Trainer::SpellState::Unavailable;
        else
        {
            for (uint32 req : s.ReqAbility)
                if (req && !player->HasSpell(req))
                    state = Trainer::SpellState::Unavailable;
            bool hasLearn = false, knowsAll = true;
            for (SpellEffectInfo const& effect : info->GetEffects())
            {
                if (!effect.IsEffect(SPELL_EFFECT_LEARN_SPELL))
                    continue;
                hasLearn = true;
                if (!player->HasSpell(effect.TriggerSpell))
                    knowsAll = false;
                else
                    continue;
                // ranks in order: the previous one first
                if (SpellInfo const* learned = sSpellMgr->GetSpellInfo(effect.TriggerSpell))
                    if (SpellInfo const* prev = learned->GetPrevRankSpell())
                        if (!player->HasSpell(prev->Id))
                            state = Trainer::SpellState::Unavailable;
                // a profession's first rank needs a free profession slot: never picked automatically
                if (SpellInfo const* learned = sSpellMgr->GetSpellInfo(effect.TriggerSpell))
                    if (learned->IsPrimaryProfessionFirstRank())
                        state = Trainer::SpellState::Unavailable;
            }
            if (hasLearn && knowsAll)
                state = Trainer::SpellState::Known;
            if (!hasLearn)
                if (SpellInfo const* prev = info->GetPrevRankSpell())
                    if (!player->HasSpell(prev->Id))
                        state = Trainer::SpellState::Unavailable;
        }
        sScriptMgr->OnPlayerGetTrainerSpellState(player, cs.trainerId, s.SpellId, state);
        return state == Trainer::SpellState::Available;
    }

    std::vector<ClassSpell const*> Missing(Player* player)
    {
        std::vector<ClassSpell const*> out;
        auto itr = classSpells.find(player->getClass());
        if (itr == classSpells.end())
            return out;
        for (ClassSpell const& cs : itr->second)
            if (CanLearnNow(player, cs))
                out.push_back(&cs);
        return out;
    }

    uint32 PriceOf(ClassSpell const& cs, float discount) { return settings.cost ? uint32(cs.spell.MoneyCost * discount) : 0; }

    void Teach(Player* player, uint32 spellId)
    {
        Trainer::Spell probe;
        probe.SpellId = spellId;
        if (!probe.IsCastable())
            player->learnSpell(spellId, false);
        else if (sScriptMgr->OnPlayerCanLearnSpell(player, spellId))
            player->CastSpell(player, spellId, true);
    }

    void SendList(Player* player);

    // Learns all that can be learned now. Rank chains are followed (learning rank 2 opens rank 3 of the same level).
    // With Cost on, every spell is paid at its trainer price and what can't be paid stays for later.
    void LearnAll(Player* player, float discount, bool quiet)
    {
        uint32 learned = 0, owed = 0, left = 0;
        for (int pass = 0; pass < 8; ++pass)
        {
            bool progress = false;
            for (ClassSpell const* cs : Missing(player))
            {
                uint32 price = PriceOf(*cs, discount);
                if (price && !player->HasEnoughMoney(price))
                    continue;
                if (price)
                    player->ModifyMoney(-int32(price));
                Teach(player, cs->spell.SpellId);
                ++learned;
                progress = true;
            }
            if (!progress)
                break;
        }
        for (ClassSpell const* cs : Missing(player))
        {
            ++left;
            owed += PriceOf(*cs, discount);
        }

        ChatHandler handler(player->GetSession());
        if (learned)
            handler.PSendSysMessage(Say(player, T_LEARNED, "|cff33ff99New spells:|r you learned {} spell(s)."), learned);
        if (left && settings.cost)
            handler.PSendSysMessage(Say(player, T_CANT_AFFORD, "|cff33ff99New spells:|r {} more can be learned for {} gold {} silver."),
                left, owed / GOLD, (owed % GOLD) / SILVER);
        else if (!learned && !quiet)
            handler.SendSysMessage(Say(player, T_NOTHING, "|cff33ff99New spells:|r there is nothing new to learn right now."));
        SendList(player);
    }

    void SetChoice(Player* player, bool on)
    {
        uint32 guid = player->GetGUID().GetCounter();
        choice[guid] = on;
        CharacterDatabase.Execute("REPLACE INTO `mod_temucraft_character_setting` (`guid`, `setting`, `value`) VALUES ({}, 'autolearn', {})", guid, on ? 1 : 0);
        ChatHandler(player->GetSession()).SendSysMessage(on
            ? Say(player, T_ON, "|cff33ff99New spells:|r new class spells will be learned as soon as you can learn them.")
            : Say(player, T_OFF, "|cff33ff99New spells:|r new class spells are learned at your trainer again."));
        if (on)
            LearnAll(player, 1.0f, true);
        else
            SendList(player);
    }

    // S~enabled~on~canChoose~cost, BEGIN, M~spell~cost (each learnable spell), END~total
    void SendList(Player* player)
    {
        if (!player->GetSession())
            return;
        TemucraftAddonSend(player, PREFIX, Acore::StringFormat("S~{}~{}~{}~{}", settings.enable ? 1 : 0, IsOn(player) ? 1 : 0,
            settings.allowChoice ? 1 : 0, settings.cost ? 1 : 0));
        if (!settings.enable)
            return;
        TemucraftAddonSend(player, PREFIX, "BEGIN");
        uint32 total = 0;
        for (ClassSpell const* cs : Missing(player))
        {
            uint32 price = PriceOf(*cs, 1.0f);
            total += price;
            TemucraftAddonSend(player, PREFIX, Acore::StringFormat("M~{}~{}", cs->spell.SpellId, price));
        }
        TemucraftAddonSend(player, PREFIX, Acore::StringFormat("END~{}", total));
    }

    bool IsClassTrainerFor(Player* player, Creature* creature)
    {
        if (!creature->HasNpcFlag(UNIT_NPC_FLAG_TRAINER))
            return false;
        Trainer::Trainer* trainer = sObjectMgr->GetTrainer(creature->GetEntry());
        return trainer && trainer->GetTrainerType() == Trainer::Type::Class && trainer->IsTrainerValidForPlayer(player);
    }
}

class TemucraftAutoLearnWorld : public WorldScript
{
public:
    TemucraftAutoLearnWorld() : WorldScript("TemucraftAutoLearnWorld", { WORLDHOOK_ON_AFTER_CONFIG_LOAD, WORLDHOOK_ON_STARTUP }) { }

    void OnAfterConfigLoad(bool reload) override
    {
        LoadSettings();
        if (reload)
            LoadTrainerSpells();
    }

    void OnStartup() override
    {
        LoadTrainerSpells();
    }
};

class TemucraftAutoLearnPlayer : public PlayerScript
{
public:
    TemucraftAutoLearnPlayer() : PlayerScript("TemucraftAutoLearnPlayer", { PLAYERHOOK_ON_LOGIN, PLAYERHOOK_ON_LOGOUT,
        PLAYERHOOK_ON_LEVEL_CHANGED, PLAYERHOOK_ON_DELETE, PLAYERHOOK_CAN_PLAYER_USE_PRIVATE_CHAT }) { }

    void OnPlayerLogin(Player* player) override
    {
        uint32 guid = player->GetGUID().GetCounter();
        if (QueryResult r = CharacterDatabase.Query("SELECT `value` FROM `mod_temucraft_character_setting` WHERE `guid` = {} AND `setting` = 'autolearn'", guid))
            choice[guid] = r->Fetch()[0].Get<int32>() != 0;
        if (settings.atLogin && IsOn(player))
            LearnAll(player, 1.0f, true);
    }

    void OnPlayerLogout(Player* player) override
    {
        choice.erase(player->GetGUID().GetCounter());
    }

    void OnPlayerDelete(ObjectGuid guid, uint32 /*accountId*/) override
    {
        CharacterDatabase.Execute("DELETE FROM `mod_temucraft_character_setting` WHERE `guid` = {} AND `setting` = 'autolearn'", guid.GetCounter());
    }

    void OnPlayerLevelChanged(Player* player, uint8 oldLevel) override
    {
        if (player->GetLevel() > oldLevel && IsOn(player))
            LearnAll(player, 1.0f, true);
        else
            SendList(player);
    }

    using PlayerScript::OnPlayerCanUseChat;
    bool OnPlayerCanUseChat(Player* player, uint32 type, uint32 language, std::string& msg, Player* receiver) override
    {
        std::string body;
        if (!TemucraftAddonParse(player, type, language, msg, receiver, PREFIX, body))
            return true;
        std::vector<std::string> a = TemucraftAddonFields(body);
        if (a.empty())
            return false;
        if (a[0] == "LIST")
            SendList(player);
        else if (settings.enable && a[0] == "LEARN")
            LearnAll(player, 1.0f, false);
        else if (settings.enable && settings.allowChoice && (a[0] == "ON" || a[0] == "OFF"))
            SetChoice(player, a[0] == "ON");
        return false;
    }
};

// Class trainers: "Teach me everything I can learn" and the on/off switch, above the trainer's own menu. Trainers
// with a script of their own are left alone (their script builds the menu).
class TemucraftAutoLearnTrainers : public AllCreatureScript
{
public:
    TemucraftAutoLearnTrainers() : AllCreatureScript("TemucraftAutoLearnTrainers") { }

    bool CanCreatureGossipHello(Player* player, Creature* creature) override
    {
        if (!settings.enable || creature->GetScriptId() || !IsClassTrainerFor(player, creature))
            return false;
        player->PrepareGossipMenu(creature, creature->GetCreatureTemplate()->GossipMenuId, true);
        float discount = player->GetReputationPriceDiscount(creature);
        std::vector<ClassSpell const*> missing = Missing(player);
        if (!missing.empty())
        {
            uint32 total = 0;
            for (ClassSpell const* cs : missing)
                total += PriceOf(*cs, discount);
            AddGossipItemFor(player, GOSSIP_ICON_TRAINER, Acore::StringFormat(Say(player, T_GOSSIP_LEARN,
                "Teach me everything I can learn ({} spells)."), missing.size()), SENDER_AUTO_LEARN, ACTION_LEARN_ALL, "", total, false);
        }
        if (settings.allowChoice)
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, IsOn(player)
                ? Say(player, T_GOSSIP_OFF, "I will come to you for new spells again.")
                : Say(player, T_GOSSIP_ON, "Teach me new spells as soon as I'm ready, wherever I am."),
                SENDER_AUTO_LEARN, IsOn(player) ? ACTION_TURN_OFF : ACTION_TURN_ON);
        player->SendPreparedGossip(creature);
        return true;
    }

    bool CanCreatureGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action) override
    {
        if (sender != SENDER_AUTO_LEARN)
            return false;
        CloseGossipMenuFor(player);
        if (!settings.enable)
            return true;
        if (action == ACTION_LEARN_ALL)
            LearnAll(player, player->GetReputationPriceDiscount(creature), false);
        else if (settings.allowChoice && (action == ACTION_TURN_ON || action == ACTION_TURN_OFF))
            SetChoice(player, action == ACTION_TURN_ON);
        return true;
    }
};

class TemucraftAutoLearnCommands : public CommandScript
{
public:
    TemucraftAutoLearnCommands() : CommandScript("TemucraftAutoLearnCommands") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable sub =
        {
            { "on",   HandleOn,   101950, Console::No },
            { "off",  HandleOff,  101951, Console::No },
            { "now",  HandleNow,  101952, Console::No },
            { "list", HandleList, 101953, Console::No },
            { "",     HandleStatus, 101954, Console::No },
        };
        static ChatCommandTable root = { { "temucraft autolearn", sub } };
        return root;
    }

    static bool Ready(ChatHandler* handler, Player*& player)
    {
        player = handler->GetPlayer();
        if (!player)
            return false;
        if (!settings.enable)
        {
            handler->SendSysMessage(Say(player, T_DISABLED, "Learning without a trainer is turned off on this server."));
            return false;
        }
        return true;
    }

    static bool HandleStatus(ChatHandler* handler)
    {
        Player* player;
        if (!Ready(handler, player))
            return true;
        handler->PSendSysMessage(Say(player, T_STATUS, "|cff33ff99New spells:|r automatic learning is {}; {} spell(s) can be learned now."),
            IsOn(player) ? "on" : "off", Missing(player).size());
        handler->SendSysMessage(Say(player, T_HELP, ".temucraft autolearn on | off | now | list"));
        return true;
    }

    static bool Toggle(ChatHandler* handler, bool on)
    {
        Player* player;
        if (!Ready(handler, player))
            return true;
        if (!settings.allowChoice)
        {
            handler->SendSysMessage(Say(player, T_NO_CHOICE, "This server decides that for everyone."));
            return true;
        }
        SetChoice(player, on);
        return true;
    }
    static bool HandleOn(ChatHandler* handler) { return Toggle(handler, true); }
    static bool HandleOff(ChatHandler* handler) { return Toggle(handler, false); }

    static bool HandleNow(ChatHandler* handler)
    {
        Player* player;
        if (Ready(handler, player))
            LearnAll(player, 1.0f, false);
        return true;
    }

    static bool HandleList(ChatHandler* handler)
    {
        Player* player;
        if (!Ready(handler, player))
            return true;
        std::vector<ClassSpell const*> missing = Missing(player);
        handler->PSendSysMessage(Say(player, T_LIST_HEAD, "|cff33ff99New spells:|r {} can be learned now."), missing.size());
        LocaleConstant locale = handler->GetSessionDbcLocale();
        for (ClassSpell const* cs : missing)
        {
            SpellInfo const* info = sSpellMgr->GetSpellInfo(cs->spell.SpellId);
            uint32 price = PriceOf(*cs, 1.0f);
            handler->PSendSysMessage(Say(player, T_LIST_ROW, "  {} {} (level {}, {} silver)"), info->SpellName[locale],
                info->Rank[locale], cs->spell.ReqLevel, price / SILVER);
        }
        return true;
    }
};

void AddTemucraftAutoLearnScripts()
{
    new TemucraftAutoLearnWorld();
    new TemucraftAutoLearnPlayer();
    new TemucraftAutoLearnTrainers();
    new TemucraftAutoLearnCommands();
}
