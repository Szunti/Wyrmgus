//       _________ __                 __
//      /   _____//  |_____________ _/  |______     ____  __ __  ______
//      \_____  \\   __\_  __ \__  \\   __\__  \   / ___\|  |  \/  ___/
//      /        \|  |  |  | \// __ \|  |  / __ \_/ /_/  >  |  /\___ |
//     /_______  /|__|  |__|  (____  /__| (____  /\___  /|____//____  >
//             \/                  \/          \//_____/            \/
//  ______________________                           ______________________
//                        T H E   W A R   B E G I N S
//         Stratagus - A free fantasy real time strategy game engine
//
/**@name action_upgradeto.cpp - The unit upgrading to new action. */
//
//      (c) Copyright 1998-2005 by Lutz Sammer and Jimmy Salmon
//
//      This program is free software; you can redistribute it and/or modify
//      it under the terms of the GNU General Public License as published by
//      the Free Software Foundation; only version 2 of the License.
//
//      This program is distributed in the hope that it will be useful,
//      but WITHOUT ANY WARRANTY; without even the implied warranty of
//      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//      GNU General Public License for more details.
//
//      You should have received a copy of the GNU General Public License
//      along with this program; if not, write to the Free Software
//      Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
//      02111-1307, USA.
//

//@{

/*----------------------------------------------------------------------------
--  Includes
----------------------------------------------------------------------------*/

#include "stratagus.h"

#include "action/action_upgradeto.h"

#include "ai.h"
#include "animation.h"
#include "iolib.h"
#include "map.h"
#include "player.h"
#include "script.h"
#include "spells.h"
//Wyrmgus start
#include "tileset.h"
//Wyrmgus end
#include "translate.h"
#include "unit.h"
#include "unittype.h"
//Wyrmgus start
#include "upgrade.h"
//Wyrmgus end

/// How many resources the player gets back if canceling upgrade
#define CancelUpgradeCostsFactor   100


/*----------------------------------------------------------------------------
--  Functions
----------------------------------------------------------------------------*/

/* static */ COrder *COrder::NewActionTransformInto(CUnitType &type)
{
	COrder_TransformInto *order = new COrder_TransformInto;

	order->Type = &type;
	return order;
}

/* static */ COrder *COrder::NewActionUpgradeTo(CUnit &unit, CUnitType &type)
{
	COrder_UpgradeTo *order = new COrder_UpgradeTo;

	// FIXME: if you give quick an other order, the resources are lost!
	unit.Player->SubUnitType(type);
	order->Type = &type;
	return order;
}

/**
**  Transform a unit in another.
**
**  @param unit     unit to transform.
**  @param newtype  new type of the unit.
**
**  @return 0 on error, 1 if nothing happens, 2 else.
*/
static int TransformUnitIntoType(CUnit &unit, const CUnitType &newtype)
{
	const CUnitType &oldtype = *unit.Type;
	if (&oldtype == &newtype) { // nothing to do
		return 1;
	}
	const Vec2i pos = unit.tilePos + oldtype.GetHalfTileSize() - newtype.GetHalfTileSize();
	CUnit *container = unit.Container;

	if (container) {
		MapUnmarkUnitSight(unit);
	} else {
		SaveSelection();
		unit.Remove(NULL);
		if (!UnitTypeCanBeAt(newtype, pos)) {
			unit.Place(unit.tilePos);
			RestoreSelection();
			// FIXME unit is not modified, try later ?
			return 0;
		}
	}
	CPlayer &player = *unit.Player;
	player.UnitTypesCount[oldtype.Slot]--;
	player.UnitTypesCount[newtype.Slot]++;

	player.Demand += newtype.Demand - oldtype.Demand;
	player.Supply += newtype.Supply - oldtype.Supply;

	// Change resource limit
	for (int i = 0; i < MaxCosts; ++i) {
		if (player.MaxResources[i] != -1) {
			player.MaxResources[i] += newtype.Stats[player.Index].Storing[i] - oldtype.Stats[player.Index].Storing[i];
			player.SetResource(i, player.StoredResources[i], STORE_BUILDING);
		}
	}

	//  adjust Variables with percent.
	const CUnitStats &newstats = newtype.Stats[player.Index];
	//Wyrmgus start
	const CUnitStats &oldstats = oldtype.Stats[player.Index];
	//Wyrmgus end

	for (unsigned int i = 0; i < UnitTypeVar.GetNumberVariable(); ++i) {
		//Wyrmgus start
		/*
		if (unit.Variable[i].Max && unit.Variable[i].Value) {
			unit.Variable[i].Value = newstats.Variables[i].Max *
									 unit.Variable[i].Value / unit.Variable[i].Max;
		} else {
			unit.Variable[i].Value = newstats.Variables[i].Value;
		}
		if (i == KILL_INDEX || i == XP_INDEX) {
			unit.Variable[i].Value = unit.Variable[i].Max;
		} else {
			unit.Variable[i].Max = newstats.Variables[i].Max;
			unit.Variable[i].Increase = newstats.Variables[i].Increase;
			unit.Variable[i].Enable = newstats.Variables[i].Enable;
		}
		*/
		if (unit.Variable[i].Max && unit.Variable[i].Value) {
			unit.Variable[i].Value += newstats.Variables[i].Max - oldstats.Variables[i].Max;
		} else {
			unit.Variable[i].Value += newstats.Variables[i].Value - oldstats.Variables[i].Value;
		}
		if (i == KILL_INDEX || i == XP_INDEX) {
			unit.Variable[i].Value = unit.Variable[i].Max;
		} else if (i == POINTS_INDEX) {
			unit.Variable[i].Value = newstats.Variables[i].Value;
		} else {
			unit.Variable[i].Max += newstats.Variables[i].Max - oldstats.Variables[i].Max;
			unit.Variable[i].Increase += newstats.Variables[i].Increase - oldstats.Variables[i].Increase;
			unit.Variable[i].Enable = newstats.Variables[i].Enable;
		}
		//Wyrmgus end
	}

	//Wyrmgus start
	//change variation if upgrading (new unit type may have different variations)
	VariationInfo *current_varinfo = oldtype.VarInfo[unit.Variation];
	int TypeVariationCount = 0;
	int LocalTypeVariations[VariationMax];
	for (int i = 0; i < VariationMax; ++i) {
		VariationInfo *varinfo = newtype.VarInfo[i];
		if (!varinfo) {
			continue;
		}
		if (!varinfo->Tileset.empty() && varinfo->Tileset != Map.Tileset->Name) {
			continue;
		}
		bool UpgradesCheck = true;
		for (int u = 0; u < VariationMax; ++u) {
			if (!varinfo->UpgradesRequired[u].empty() && UpgradeIdentAllowed(player, varinfo->UpgradesRequired[u].c_str()) != 'R' && unit.LearnedAbilities[CUpgrade::Get(varinfo->UpgradesRequired[u])->ID] == false) {
				UpgradesCheck = false;
				break;
			}
			if (!varinfo->UpgradesForbidden[u].empty() && (UpgradeIdentAllowed(player, varinfo->UpgradesForbidden[u].c_str()) == 'R' || unit.LearnedAbilities[CUpgrade::Get(varinfo->UpgradesForbidden[u])->ID] == true)) {
				UpgradesCheck = false;
				break;
			}
		}
		if (UpgradesCheck == false) {
			continue;
		}
		if (current_varinfo && varinfo->VariationId.find(current_varinfo->VariationId) != std::string::npos) { // if the old variation's ident is included in that of a viable one of the new unit type, chose the latter automatically
			unit.Variation = i;
			TypeVariationCount = 0;
			break;
		}
		LocalTypeVariations[TypeVariationCount] = i;
		TypeVariationCount += 1;
	}
	if (TypeVariationCount > 0) {
		unit.Variation = LocalTypeVariations[SyncRand(TypeVariationCount)];
	}
	//Wyrmgus end
	
	unit.Type = const_cast<CUnitType *>(&newtype);
	unit.Stats = &unit.Type->Stats[player.Index];
	
	//Wyrmgus start
	//change personal name if new unit type's civilization is different from old unit type's civilization
	if (
		(!oldtype.Civilization.empty() && !newtype.Civilization.empty() && oldtype.Civilization != newtype.Civilization)
		|| (!oldtype.PersonalNames[0].empty() || !oldtype.PersonalNamePrefixes[0].empty()) && (!newtype.PersonalNames[0].empty() || !newtype.PersonalNamePrefixes[0].empty())
	) {
		unit.GeneratePersonalName();
	}
	//Wyrmgus end

	if (newtype.CanCastSpell && !unit.AutoCastSpell) {
		unit.AutoCastSpell = new char[SpellTypeTable.size()];
		unit.SpellCoolDownTimers = new int[SpellTypeTable.size()];
		memset(unit.AutoCastSpell, 0, SpellTypeTable.size() * sizeof(char));
		memset(unit.SpellCoolDownTimers, 0, SpellTypeTable.size() * sizeof(int));
	}

	UpdateForNewUnit(unit, 1);
	//  Update Possible sight range change
	UpdateUnitSightRange(unit);
	if (!container) {
		unit.Place(pos);
		RestoreSelection();
	} else {
		MapMarkUnitSight(unit);
	}
	//
	// Update possible changed buttons.
	//
	if (IsOnlySelected(unit) || &player == ThisPlayer) {
		// could affect the buttons of any selected unit
		SelectedUnitChanged();
	}
	return 1;
}


#if 1 // TransFormInto

/* virtual */ void COrder_TransformInto::Save(CFile &file, const CUnit &unit) const
{
	file.printf("{\"action-transform-into\",");
	if (this->Finished) {
		file.printf(" \"finished\", ");
	}
	file.printf(" \"type\", \"%s\"", this->Type->Ident.c_str());
	file.printf("}");
}

/* virtual */ bool COrder_TransformInto::ParseSpecificData(lua_State *l, int &j, const char *value, const CUnit &unit)
{
	if (!strcmp(value, "type")) {
		++j;
		this->Type = UnitTypeByIdent(LuaToString(l, -1, j + 1));
	} else {
		return false;
	}
	return true;
}

/* virtual */ bool COrder_TransformInto::IsValid() const
{
	return true;
}


/* virtual */ PixelPos COrder_TransformInto::Show(const CViewport &, const PixelPos &lastScreenPos) const
{
	return lastScreenPos;
}


/* virtual */ void COrder_TransformInto::Execute(CUnit &unit)
{
	TransformUnitIntoType(unit, *this->Type);
	this->Finished = true;
}

#endif

#if 1  //  COrder_UpgradeTo

/* virtual */ void COrder_UpgradeTo::Save(CFile &file, const CUnit &unit) const
{
	file.printf("{\"action-upgrade-to\",");
	if (this->Finished) {
		file.printf(" \"finished\", ");
	}
	file.printf(" \"type\", \"%s\",", this->Type->Ident.c_str());
	file.printf(" \"ticks\", %d", this->Ticks);
	file.printf("}");
}

/* virtual */ bool COrder_UpgradeTo::ParseSpecificData(lua_State *l, int &j, const char *value, const CUnit &unit)
{
	if (!strcmp(value, "type")) {
		++j;
		this->Type = UnitTypeByIdent(LuaToString(l, -1, j + 1));
	} else if (!strcmp(value, "ticks")) {
		++j;
		this->Ticks = LuaToNumber(l, -1, j + 1);
	} else {
		return false;
	}
	return true;
}

/* virtual */ bool COrder_UpgradeTo::IsValid() const
{
	return true;
}

/* virtual */ PixelPos COrder_UpgradeTo::Show(const CViewport &, const PixelPos &lastScreenPos) const
{
	return lastScreenPos;
}


static void AnimateActionUpgradeTo(CUnit &unit)
{
	//Wyrmgus start
//	CAnimations &animations = *unit.Type->Animations;
	VariationInfo *varinfo = unit.Type->VarInfo[unit.Variation];
	if (varinfo && varinfo->Animations) {
		UnitShowAnimation(unit, varinfo->Animations->Upgrade ? varinfo->Animations->Upgrade : varinfo->Animations->Still);
	} else {
		UnitShowAnimation(unit, unit.Type->Animations->Upgrade ? unit.Type->Animations->Upgrade : unit.Type->Animations->Still);
	}
	//Wyrmgus end

	//Wyrmgus start
//	UnitShowAnimation(unit, animations.Upgrade ? animations.Upgrade : animations.Still);
	//Wyrmgus end
}

/* virtual */ void COrder_UpgradeTo::Execute(CUnit &unit)
{
	AnimateActionUpgradeTo(unit);
	if (unit.Wait) {
		unit.Wait--;
		return ;
	}
	CPlayer &player = *unit.Player;
	const CUnitType &newtype = *this->Type;
	const CUnitStats &newstats = newtype.Stats[player.Index];

	this->Ticks += std::max(1, player.SpeedUpgrade / SPEEDUP_FACTOR);
	if (this->Ticks < newstats.Costs[TimeCost]) {
		unit.Wait = CYCLES_PER_SECOND / 6;
		return ;
	}

	if (unit.Anim.Unbreakable) {
		this->Ticks = newstats.Costs[TimeCost];
		return ;
	}

	if (TransformUnitIntoType(unit, newtype) == 0) {
		//Wyrmgus start
		//I think it is too much to notify the player whenever an individual upgrade is cancelled
//		player.Notify(NotifyYellow, unit.tilePos, _("Upgrade to %s canceled"), newtype.Name.c_str());
		//Wyrmgus end
		this->Finished = true;
		return ;
	}
	//Wyrmgus start
	//I think it is too much to notify the player whenever an individual upgrade is completed
//	player.Notify(NotifyGreen, unit.tilePos, _("Upgrade to %s complete"), unit.Type->Name.c_str());
	//Wyrmgus end

	//  Warn AI.
	if (player.AiEnabled) {
		AiUpgradeToComplete(unit, newtype);
	}
	this->Finished = true;
}

/* virtual */ void COrder_UpgradeTo::Cancel(CUnit &unit)
{
	CPlayer &player = *unit.Player;

	player.AddCostsFactor(this->Type->Stats[player.Index].Costs, CancelUpgradeCostsFactor);
}

/* virtual */ void COrder_UpgradeTo::UpdateUnitVariables(CUnit &unit) const
{
	Assert(unit.CurrentOrder() == this);

	unit.Variable[UPGRADINGTO_INDEX].Value = this->Ticks;
	unit.Variable[UPGRADINGTO_INDEX].Max = this->Type->Stats[unit.Player->Index].Costs[TimeCost];
}

#endif
//@}
