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
/**@name spell_demolish.cpp - The spell demolish. */
//
//      (c) Copyright 1998-2012 by Vladi Belperchinov-Shabanski, Lutz Sammer,
//                                 Jimmy Salmon, and Joris DAUPHIN
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

#include "stratagus.h"

#include "spell/spell_demolish.h"

#include "script.h"
#include "map.h"
#include "unit.h"
#include "unit_find.h"

/* virtual */ void Spell_Demolish::Parse(lua_State *l, int startIndex, int endIndex)
{
	for (int j = startIndex; j < endIndex; ++j) {
		const char *value = LuaToString(l, -1, j + 1);
		++j;
		if (!strcmp(value, "range")) {
			this->Range = LuaToNumber(l, -1, j + 1);
		} else if (!strcmp(value, "damage")) {
			this->Damage = LuaToNumber(l, -1, j + 1);
		//Wyrmgus start
		} else if (!strcmp(value, "basic-damage")) {
			this->BasicDamage = LuaToNumber(l, -1, j + 1);
		} else if (!strcmp(value, "piercing-damage")) {
			this->PiercingDamage = LuaToNumber(l, -1, j + 1);
		} else if (!strcmp(value, "damage-self")) {
			this->DamageSelf = LuaToBoolean(l, -1, j + 1);
		} else if (!strcmp(value, "damage-friendly")) {
			this->DamageFriendly = LuaToBoolean(l, -1, j + 1);
		} else if (!strcmp(value, "damage-terrain")) {
			this->DamageTerrain = LuaToBoolean(l, -1, j + 1);
		//Wyrmgus end
		} else {
			LuaError(l, "Unsupported demolish tag: %s" _C_ value);
		}
	}
}

/**
**  Cast demolish
**  @param caster       Unit that casts the spell
**  @param spell        Spell-type pointer
**  @param target       Target unit that spell is addressed to
**  @param goalPos      tilePos of target spot when/if target does not exist
**
**  @return             =!0 if spell should be repeated, 0 if not
*/
/* virtual */ int Spell_Demolish::Cast(CUnit &caster, const SpellType &, CUnit *, const Vec2i &goalPos)
{
	// Allow error margins. (Lame, I know)
	const Vec2i offset(this->Range + 2, this->Range + 2);
	//Wyrmgus start
//	Vec2i minpos = goalPos - offset;
//	Vec2i maxpos = goalPos + offset;
	Vec2i minpos = caster.tilePos - offset;
	Vec2i maxpos = caster.tilePos + offset;
	//Wyrmgus end

	Map.FixSelectionArea(minpos, maxpos);

	//
	// Terrain effect of the explosion
	//
	//Wyrmgus start
	/*
	Vec2i ipos;
	for (ipos.x = minpos.x; ipos.x <= maxpos.x; ++ipos.x) {
		for (ipos.y = minpos.y; ipos.y <= maxpos.y; ++ipos.y) {
			const CMapField &mf = *Map.Field(ipos);
			if (SquareDistance(ipos, goalPos) > square(this->Range)) {
				// Not in circle range
				continue;
			} else if (mf.isAWall()) {
				Map.RemoveWall(ipos);
			} else if (mf.RockOnMap()) {
				Map.ClearRockTile(ipos);
			} else if (mf.ForestOnMap()) {
				Map.ClearWoodTile(ipos);
			}
		}
	}
	*/

	if (this->DamageTerrain) {
		Vec2i ipos;
		for (ipos.x = minpos.x; ipos.x <= maxpos.x; ++ipos.x) {
			for (ipos.y = minpos.y; ipos.y <= maxpos.y; ++ipos.y) {
				const CMapField &mf = *Map.Field(ipos);
				if (SquareDistance(ipos, caster.tilePos) > square(this->Range)) {
					// Not in circle range
					continue;
				} else if (mf.isAWall()) {
					Map.RemoveWall(ipos);
				} else if (mf.RockOnMap()) {
					Map.ClearRockTile(ipos);
				} else if (mf.ForestOnMap()) {
					Map.ClearWoodTile(ipos);
				}
			}
		}
	}
	//Wyrmgus end

	//
	//  Effect of the explosion on units. Don't bother if damage is 0
	//
	//Wyrmgus start
	//if (this->Damage) {
	if (this->Damage || this->BasicDamage || this->PiercingDamage) {
	//Wyrmgus end
		std::vector<CUnit *> table;
		SelectFixed(minpos, maxpos, table);
		for (size_t i = 0; i != table.size(); ++i) {
			CUnit &unit = *table[i];
			if (unit.Type->UnitType != UnitTypeFly && unit.IsAlive()
				//Wyrmgus start
//				&& unit.MapDistanceTo(goalPos) <= this->Range) {
				// Don't hit flying units!
//				HitUnit(&caster, unit, this->Damage);
				&& unit.MapDistanceTo(caster.tilePos) <= this->Range && (UnitNumber(unit) != UnitNumber(caster) || this->DamageSelf) && ((!caster.IsAllied(unit) && unit.Player != caster.Player) || this->DamageFriendly)) {

				int damage = 0;
				if (this->BasicDamage || this->PiercingDamage) {
					damage = std::max<int>(this->BasicDamage - unit.Stats->Variables[ARMOR_INDEX].Value, 1);
					damage += this->PiercingDamage;
					damage -= SyncRand() % ((damage + 2) / 2);
				}
				HitUnit(&caster, unit, this->Damage + damage);
				//Wyrmgus end
			}
		}
	}

	return 1;
}



//@}
