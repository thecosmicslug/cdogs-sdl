/*
    C-Dogs SDL
    A port of the legendary (and fun) action/arcade cdogs.
    Copyright (C) 1995 Ronny Wester
    Copyright (C) 2003 Jeremy Chin
    Copyright (C) 2003-2007 Lucas Martin-King

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    This file incorporates work covered by the following copyright and
    permission notice:

    Copyright (c) 2013-2014, Cong Xu
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    Redistributions of source code must retain the above copyright notice, this
    list of conditions and the following disclaimer.
    Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.
*/
#include "prep.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <SDL_timer.h>

#include <cdogs/ai_coop.h>
#include <cdogs/actors.h>
#include <cdogs/blit.h>
#include <cdogs/config.h>
#include <cdogs/draw.h>
#include <cdogs/files.h>
#include <cdogs/font.h>
#include <cdogs/grafx.h>
#include <cdogs/input.h>
#include <cdogs/joystick.h>
#include <cdogs/keyboard.h>
#include <cdogs/music.h>
#include <cdogs/net_server.h>
#include <cdogs/pic_manager.h>
#include <cdogs/sounds.h>
#include <cdogs/utils.h>

#include "player_select_menus.h"
#include "namegen.h"
#include "weapon_menu.h"


int NumPlayersSelection(
	int *numPlayers, campaign_mode_e mode,
	GraphicsDevice *graphics, EventHandlers *handlers)
{
	MenuSystem ms;
	MenuSystemInit(
		&ms, handlers, graphics,
		Vec2iZero(),
		Vec2iNew(
			graphics->cachedConfig.Res.x,
			graphics->cachedConfig.Res.y));
	ms.allowAborts = true;
	ms.root = ms.current = MenuCreateNormal(
		"",
		"Select number of players",
		MENU_TYPE_NORMAL,
		0);
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		char buf[2];
		if (mode == CAMPAIGN_MODE_DOGFIGHT && i == 0)
		{
			// At least two players for dogfights
			continue;
		}
		sprintf(buf, "%d", i + 1);
		MenuAddSubmenu(ms.current, MenuCreateReturn(buf, i + 1));
	}
	MenuAddExitType(&ms, MENU_TYPE_RETURN);

	MenuLoop(&ms);
	const bool ok = !ms.hasAbort;
	if (ok)
	{
		*numPlayers = ms.current->u.returnCode;
	}
	MenuSystemTerminate(&ms);
	return ok;
}


static void AssignPlayerInputDevice(
	struct PlayerData *pData, input_device_e d, int idx)
{
	pData->inputDevice = d;
	pData->deviceIndex = idx;
}

static void AssignPlayerInputDevices(
	bool hasInputDevice[MAX_PLAYERS], int numPlayers,
	struct PlayerData playerDatas[MAX_PLAYERS],
	EventHandlers *handlers, InputConfig *inputConfig)
{
	bool assignedKeyboards[MAX_KEYBOARD_CONFIGS];
	bool assignedMouse = false;
	bool assignedJoysticks[MAX_JOYSTICKS];
	bool assignedNet[MAX_PLAYERS];
	int numNet = 0;
	memset(assignedKeyboards, 0, sizeof assignedKeyboards);
	memset(assignedJoysticks, 0, sizeof assignedJoysticks);
	memset(assignedNet, 0, sizeof assignedNet);

	for (int i = 0; i < numPlayers; i++)
	{
		if (hasInputDevice[i])
		{
			// Find all the assigned devices
			switch (playerDatas[i].inputDevice)
			{
			case INPUT_DEVICE_KEYBOARD:
				assignedKeyboards[playerDatas[i].deviceIndex] = true;
				break;
			case INPUT_DEVICE_MOUSE:
				assignedMouse = true;
				break;
			case INPUT_DEVICE_JOYSTICK:
				assignedJoysticks[playerDatas[i].deviceIndex] = true;
				break;
			case INPUT_DEVICE_NET:
				assignedNet[playerDatas[i].deviceIndex] = true;
				numNet++;
				break;
			default:
				// do nothing
				break;
			}
			continue;
		}

		// Try to assign devices to players
		// For each unassigned player, check if any device has button 1 pressed
		for (int j = 0; j < MAX_KEYBOARD_CONFIGS; j++)
		{
			if (KeyIsPressed(
				&handlers->keyboard,
				inputConfig->PlayerKeys[j].Keys.button1) &&
				!assignedKeyboards[j])
			{
				hasInputDevice[i] = 1;
				AssignPlayerInputDevice(
					&playerDatas[i], INPUT_DEVICE_KEYBOARD, j);
				assignedKeyboards[j] = true;
				SoundPlay(&gSoundDevice, StrSound("hahaha"));
				continue;
			}
		}
		if (MouseIsPressed(&handlers->mouse, SDL_BUTTON_LEFT) &&
			!assignedMouse)
		{
			hasInputDevice[i] = 1;
			AssignPlayerInputDevice(&playerDatas[i], INPUT_DEVICE_MOUSE, 0);
			assignedMouse = true;
			SoundPlay(&gSoundDevice, StrSound("hahaha"));
			continue;
		}
		for (int j = 0; j < handlers->joysticks.numJoys; j++)
		{
			if (JoyIsPressed(
				&handlers->joysticks.joys[j], CMD_BUTTON1) &&
				!assignedJoysticks[j])
			{
				hasInputDevice[i] = 1;
				AssignPlayerInputDevice(
					&playerDatas[i], INPUT_DEVICE_JOYSTICK, j);
				assignedJoysticks[j] = true;
				SoundPlay(&gSoundDevice, StrSound("hahaha"));
				continue;
			}
		}
		if ((int)gNetServer.server->connectedPeers > numNet)
		{
			hasInputDevice[i] = true;
			AssignPlayerInputDevice(&playerDatas[i], INPUT_DEVICE_NET, 0);
			// Send the current campaign details over
			NetServerSendMsg(
				&gNetServer,
				gNetServer.peerId - 1,
				SERVER_MSG_CAMPAIGN_DEF, &gCampaign.Entry);
			assignedNet[playerDatas[i].deviceIndex] = true;
			numNet++;
			SoundPlay(&gSoundDevice, StrSound("hahaha"));
			continue;
		}
	}
}

typedef struct
{
	int numPlayers;
	bool hasInputDevice[MAX_PLAYERS];
	PlayerSelectMenu menus[MAX_PLAYERS];
	NameGen g;
	char prefixes[CDOGS_PATH_MAX];
	char suffixes[CDOGS_PATH_MAX];
	char suffixnames[CDOGS_PATH_MAX];
	bool IsOK;
} PlayerSelectionData;
static GameLoopResult PlayerSelectionUpdate(void *data);
static void PlayerSelectionDraw(void *data);
bool PlayerSelection(const int numPlayers)
{
	PlayerSelectionData data;
	memset(&data, 0, sizeof data);
	data.numPlayers = numPlayers;
	data.IsOK = true;
	GetDataFilePath(data.prefixes, "data/prefixes.txt");
	GetDataFilePath(data.suffixes, "data/suffixes.txt");
	GetDataFilePath(data.suffixnames, "data/suffixnames.txt");
	NameGenInit(&data.g, data.prefixes, data.suffixes, data.suffixnames);
	for (int i = 0; i < numPlayers; i++)
	{
		PlayerSelectMenusCreate(
			&data.menus[i], numPlayers, i,
			&gCampaign.Setting.characters.players[i], &gPlayerDatas[i],
			&gEventHandlers, &gGraphicsDevice, &gConfig.Input, &data.g);
	}

	NetServerOpen(&gNetServer);
	GameLoopData gData = GameLoopDataNew(
		&data, PlayerSelectionUpdate,
		&data, PlayerSelectionDraw);
	GameLoop(&gData);

	if (data.IsOK)
	{
		// For any player slots not picked, turn them into AIs
		for (int i = 0; i < numPlayers; i++)
		{
			if (!data.hasInputDevice[i])
			{
				AssignPlayerInputDevice(&gPlayerDatas[i], INPUT_DEVICE_AI, 0);
			}
		}
	}

	// If no net input devices selected, close the connection
	bool hasNetInput = false;
	for (int i = 0; i < numPlayers; i++)
	{
		if (data.hasInputDevice[i] &&
			gPlayerDatas[i].inputDevice == INPUT_DEVICE_NET)
		{
			hasNetInput = true;
			break;
		}
	}

	if (!hasNetInput)
	{
		// TODO: support net players joining mid-game
		NetServerTerminate(&gNetServer);
	}

	for (int i = 0; i < numPlayers; i++)
	{
		MenuSystemTerminate(&data.menus[i].ms);
	}
	NameGenTerminate(&data.g);
	return data.IsOK;
}
static GameLoopResult PlayerSelectionUpdate(void *data)
{
	PlayerSelectionData *pData = data;

	// Check if anyone pressed escape
	int cmds[MAX_PLAYERS];
	memset(cmds, 0, sizeof cmds);
	GetPlayerCmds(&gEventHandlers, &cmds, gPlayerDatas);
	if (EventIsEscape(
		&gEventHandlers, cmds, GetMenuCmd(&gEventHandlers, gPlayerDatas)))
	{
		pData->IsOK = false;
		return UPDATE_RESULT_EXIT;
	}

	// Menu input
	for (int i = 0; i < pData->numPlayers; i++)
	{
		if (pData->hasInputDevice[i] &&
			!MenuIsExit(&pData->menus[i].ms) &&
			cmds[i])
		{
			MenuProcessCmd(&pData->menus[i].ms, cmds[i]);
		}
	}

	// Conditions for exit: at least one player has selected "Done",
	// and no other players, if any, are still selecting their player
	// The "players" with no input device are turned into AIs
	bool hasAtLeastOneInput = false;
	bool isDone = true;
	for (int i = 0; i < pData->numPlayers; i++)
	{
		if (pData->hasInputDevice[i])
		{
			hasAtLeastOneInput = true;
		}
		if (strcmp(pData->menus[i].ms.current->name, "Done") != 0 &&
			pData->hasInputDevice[i])
		{
			isDone = false;
		}
	}
	if (isDone && hasAtLeastOneInput)
	{
		return UPDATE_RESULT_EXIT;
	}

	AssignPlayerInputDevices(
		pData->hasInputDevice, pData->numPlayers,
		gPlayerDatas, &gEventHandlers, &gConfig.Input);

	return UPDATE_RESULT_DRAW;
}
static void PlayerSelectionDraw(void *data)
{
	const PlayerSelectionData *pData = data;

	GraphicsBlitBkg(&gGraphicsDevice);
	const int w = gGraphicsDevice.cachedConfig.Res.x;
	const int h = gGraphicsDevice.cachedConfig.Res.y;
	for (int i = 0; i < pData->numPlayers; i++)
	{
		if (pData->hasInputDevice[i])
		{
			MenuDisplay(&pData->menus[i].ms);
		}
		else
		{
			Vec2i center = Vec2iZero();
			const char *prompt = "Press Fire to join...";
			const Vec2i offset = Vec2iScaleDiv(FontStrSize(prompt), -2);
			switch (pData->numPlayers)
			{
			case 1:
				// Center of screen
				center = Vec2iNew(w / 2, h / 2);
				break;
			case 2:
				// Side by side
				center = Vec2iNew(i * w / 2 + w / 4, h / 2);
				break;
			case 3:
			case 4:
				// Four corners
				center = Vec2iNew(
					(i & 1) * w / 2 + w / 4, (i / 2) * h / 2 + h / 4);
				break;
			default:
				CASSERT(false, "not implemented");
				break;
			}
			FontStr(prompt, Vec2iAdd(center, offset));
		}
	}
}

typedef struct
{
	WeaponMenu menus[MAX_PLAYERS];
	int numPlayers;
	bool IsOK;
} PlayerEquipData;
static GameLoopResult PlayerEquipUpdate(void *data);
static void PlayerEquipDraw(void *data);
bool PlayerEquip(const int numPlayers)
{
	PlayerEquipData data;
	memset(&data, 0, sizeof data);
	data.numPlayers = numPlayers;
	data.IsOK = true;
	for (int i = 0; i < numPlayers; i++)
	{
		WeaponMenuCreate(
			&data.menus[i], numPlayers, i,
			&gCampaign.Setting.characters.players[i], &gPlayerDatas[i],
			&gEventHandlers, &gGraphicsDevice, &gConfig.Input);
		// For AI players, pre-pick their weapons and go straight to menu end
		if (gPlayerDatas[i].inputDevice == INPUT_DEVICE_AI)
		{
			const int lastMenuIndex =
				(int)data.menus[i].ms.root->u.normal.subMenus.size - 1;
			data.menus[i].ms.current = CArrayGet(
				&data.menus[i].ms.root->u.normal.subMenus, lastMenuIndex);
			gPlayerDatas[i].weapons[0] = AICoopSelectWeapon(
				i, &gMission.missionData->Weapons);
			gPlayerDatas[i].weaponCount = 1;
			// TODO: select more weapons, or select weapons based on mission
		}
	}

	GameLoopData gData = GameLoopDataNew(
		&data, PlayerEquipUpdate, &data, PlayerEquipDraw);
	GameLoop(&gData);

	for (int i = 0; i < numPlayers; i++)
	{
		MenuSystemTerminate(&data.menus[i].ms);
	}

	return data.IsOK;
}
static GameLoopResult PlayerEquipUpdate(void *data)
{
	PlayerEquipData *pData = data;

	// Check if anyone pressed escape
	int cmds[MAX_PLAYERS];
	memset(cmds, 0, sizeof cmds);
	GetPlayerCmds(&gEventHandlers, &cmds, gPlayerDatas);
	if (EventIsEscape(
		&gEventHandlers, cmds, GetMenuCmd(&gEventHandlers, gPlayerDatas)))
	{
		pData->IsOK = false;
		return UPDATE_RESULT_EXIT;
	}

	// Update menus
	for (int i = 0; i < pData->numPlayers; i++)
	{
		if (!MenuIsExit(&pData->menus[i].ms))
		{
			MenuProcessCmd(&pData->menus[i].ms, cmds[i]);
		}
		else if (gPlayerDatas[i].weaponCount == 0)
		{
			// Check exit condition; must have selected at least one weapon
			// Otherwise reset the current menu
			pData->menus[i].ms.current = pData->menus[i].ms.root;
		}
	}

	bool isDone = true;
	for (int i = 0; i < pData->numPlayers; i++)
	{
		if (strcmp(pData->menus[i].ms.current->name, "(End)") != 0)
		{
			isDone = false;
		}
	}
	if (isDone)
	{
		return UPDATE_RESULT_EXIT;
	}

	return UPDATE_RESULT_DRAW;
}
static void PlayerEquipDraw(void *data)
{
	const PlayerEquipData *pData = data;
	GraphicsBlitBkg(&gGraphicsDevice);
	for (int i = 0; i < pData->numPlayers; i++)
	{
		MenuDisplay(&pData->menus[i].ms);
	}
}