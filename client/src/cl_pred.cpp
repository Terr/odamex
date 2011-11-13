// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 2000-2006 by Sergey Makovkin (CSDoom .62).
// Copyright (C) 2006-2010 by The Odamex Team.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	Client-side prediction of local player, other players and moving sectors
//
//-----------------------------------------------------------------------------


#include "doomtype.h"
#include "doomstat.h"
#include "d_player.h"
#include "g_game.h"
#include "p_local.h"
#include "gi.h"
#include "i_system.h"
#include "c_dispatch.h"
#include "st_stuff.h"
#include "m_argv.h"
#include "c_console.h"
#include "cl_main.h"
#include "cl_demo.h"
#include "vectors.h"

// Prediction debugging info
#define _PRED_DBG

void P_MovePlayer (player_t *player);
void P_CalcHeight (player_t *player);

angle_t cl_angle[MAXSAVETICS];
angle_t cl_pitch[MAXSAVETICS];
fixed_t cl_viewheight[MAXSAVETICS];
fixed_t cl_deltaviewheight[MAXSAVETICS];
fixed_t cl_jumpTics[MAXSAVETICS];
int     cl_reactiontime[MAXSAVETICS];
byte    cl_waterlevel[MAXSAVETICS];

extern int last_player_update;
int extrapolation_tics;
bool predicting;

extern NetDemo netdemo;

TArray <plat_pred_t> real_plats;

//
// CL_ResetSectors
//
void CL_ResetSectors (void)
{
	for(size_t i = 0; i < real_plats.Size(); i++)
	{
		plat_pred_t *pred = &real_plats[i];
		sector_t *sec = &sectors[pred->secnum];

		if(!sec->floordata && !sec->ceilingdata)
		{
			real_plats.Pop(real_plats[i]);

            if (!real_plats.Size())
                break;

			continue;
		}

		// Pillars and elevators set both floordata and ceilingdata
		if(sec->ceilingdata && sec->ceilingdata->IsA(RUNTIME_CLASS(DPillar)))
		{
			DPillar *Pillar = (DPillar *)sec->ceilingdata;
            
            sec->floorheight = pred->floorheight;
            sec->ceilingheight = pred->ceilingheight;
            P_ChangeSector(sec, false);

            Pillar->m_Type = (DPillar::EPillar)pred->Both.m_Type;
            Pillar->m_FloorSpeed = pred->Both.m_FloorSpeed;
            Pillar->m_CeilingSpeed = pred->Both.m_CeilingSpeed;
            Pillar->m_FloorTarget = pred->Both.m_FloorTarget;
            Pillar->m_CeilingTarget = pred->Both.m_CeilingTarget;
            Pillar->m_Crush = pred->Both.m_Crush;
            
            continue;
		}

        if (sec->ceilingdata && sec->ceilingdata->IsA(RUNTIME_CLASS(DElevator)))
        {
            DElevator *Elevator = (DElevator *)sec->ceilingdata;

            sec->floorheight = pred->floorheight;
            sec->ceilingheight = pred->ceilingheight;
            P_ChangeSector(sec, false);

            Elevator->m_Type = (DElevator::EElevator)pred->Both.m_Type;
            Elevator->m_Direction = pred->Both.m_Direction;
            Elevator->m_FloorDestHeight = pred->Both.m_FloorDestHeight;
            Elevator->m_CeilingDestHeight = pred->Both.m_CeilingDestHeight;
            Elevator->m_Speed = pred->Both.m_Speed;
            
            continue;
        }

		if (sec->floordata && sec->floordata->IsA(RUNTIME_CLASS(DFloor)))
        {
            DFloor *Floor = (DFloor *)sec->floordata;

            sec->floorheight = pred->floorheight;
            P_ChangeSector(sec, false);

            Floor->m_Type = (DFloor::EFloor)pred->Floor.m_Type;
            Floor->m_Crush = pred->Floor.m_Crush;
            Floor->m_Direction = pred->Floor.m_Direction;
            Floor->m_NewSpecial = pred->Floor.m_NewSpecial;
            Floor->m_Texture = pred->Floor.m_Texture;
            Floor->m_FloorDestHeight = pred->Floor.m_FloorDestHeight;
            Floor->m_Speed = pred->Floor.m_Speed;
            Floor->m_ResetCount = pred->Floor.m_ResetCount;
            Floor->m_OrgHeight = pred->Floor.m_OrgHeight;
            Floor->m_Delay = pred->Floor.m_Delay;
            Floor->m_PauseTime = pred->Floor.m_PauseTime;
            Floor->m_StepTime = pred->Floor.m_StepTime;
            Floor->m_PerStepTime = pred->Floor.m_PerStepTime;
        }

		if(sec->floordata && sec->floordata->IsA(RUNTIME_CLASS(DPlat)))
		{
			DPlat *Plat = (DPlat *)sec->floordata;
            
            sec->floorheight = pred->floorheight;
            P_ChangeSector(sec, false);

            Plat->m_Speed = pred->Floor.m_Speed;
            Plat->m_Low = pred->Floor.m_Low;
            Plat->m_High = pred->Floor.m_High;
            Plat->m_Wait = pred->Floor.m_Wait;
            Plat->m_Count = pred->Floor.m_Count;
            Plat->m_Status = (DPlat::EPlatState)pred->Floor.m_Status;
            Plat->m_OldStatus = (DPlat::EPlatState)pred->Floor.m_OldStatus;
            Plat->m_Crush = pred->Floor.m_Crush;
            Plat->m_Tag = pred->Floor.m_Tag;
            Plat->m_Type = (DPlat::EPlatType)pred->Floor.m_Type;
		}

		if (sec->ceilingdata && sec->ceilingdata->IsA(RUNTIME_CLASS(DCeiling)))
        {
            DCeiling *Ceiling = (DCeiling *)sec->ceilingdata;

            sec->ceilingheight = pred->ceilingheight;
            P_ChangeSector(sec, false);

            Ceiling->m_Type = (DCeiling::ECeiling)pred->Ceiling.m_Type;
            Ceiling->m_BottomHeight = pred->Ceiling.m_BottomHeight;
            Ceiling->m_TopHeight = pred->Ceiling.m_TopHeight;
            Ceiling->m_Speed = pred->Ceiling.m_Speed;
            Ceiling->m_Speed1 = pred->Ceiling.m_Speed1;
            Ceiling->m_Speed2 = pred->Ceiling.m_Speed2;
            Ceiling->m_Crush = pred->Ceiling.m_Crush;
            Ceiling->m_Silent = pred->Ceiling.m_Silent;
            Ceiling->m_Direction = pred->Ceiling.m_Direction;
            Ceiling->m_Texture = pred->Ceiling.m_Texture;
            Ceiling->m_NewSpecial = pred->Ceiling.m_NewSpecial;
            Ceiling->m_Tag = pred->Ceiling.m_Tag;
            Ceiling->m_OldDirection = pred->Ceiling.m_OldDirection;
        }

		if (sec->ceilingdata && sec->ceilingdata->IsA(RUNTIME_CLASS(DDoor)))
        {
            DDoor *Door = (DDoor *)sec->ceilingdata;

            sec->ceilingheight = pred->ceilingheight;
            P_ChangeSector(sec, false);

            Door->m_Type = (DDoor::EVlDoor)pred->Ceiling.m_Type;
            Door->m_TopHeight = pred->Ceiling.m_TopHeight;
            Door->m_Speed = pred->Ceiling.m_Speed;
            Door->m_Direction = pred->Ceiling.m_Direction;
            Door->m_TopWait = pred->Ceiling.m_TopWait;
            Door->m_TopCountdown = pred->Ceiling.m_TopCountdown;
			Door->m_Status = (DDoor::EVlDoorState)pred->Ceiling.m_Status;
            Door->m_Line = pred->Ceiling.m_Line;
        }
	}
}

//
// CL_PredictSectors
//
void CL_PredictSectors (int predtic)
{
	for(size_t i = 0; i < real_plats.Size(); i++)
	{
		plat_pred_t *pred = &real_plats[i];
		sector_t *sec = &sectors[pred->secnum];

		if(pred->tic < predtic)
		{
            if (sec->ceilingdata && sec->ceilingdata->IsA(RUNTIME_CLASS(DPillar)))
            {
                sec->ceilingdata->RunThink();

                continue;
            } 

            if (sec->ceilingdata && sec->ceilingdata->IsA(RUNTIME_CLASS(DElevator)))
            {
                sec->ceilingdata->RunThink();

                continue;
            }  

            if (sec->floordata && sec->floordata->IsKindOf(RUNTIME_CLASS(DMovingFloor)))
            {
                sec->floordata->RunThink();
            }

            if (sec->ceilingdata && sec->ceilingdata->IsKindOf(RUNTIME_CLASS(DMovingCeiling)))
            {
                sec->ceilingdata->RunThink();
            }
		}
	}
}


//
// CL_ResetPlayer
//
// Resets a player's position to their last known position according
// to the server
//

void CL_ResetPlayer(player_t &p)
{
	if (!p.mo || p.spectator)
		return;
		
	// set the position
	CL_MoveThing (p.mo, p.real_origin[0], p.real_origin[1], p.real_origin[2]);

	// set the velocity
	p.mo->momx = p.real_velocity[0];
	p.mo->momy = p.real_velocity[1];
	p.mo->momz = p.real_velocity[2];
}



//
// CL_PredictLocalPlayer
//
// Processes all of consoleplayer's ticcmds since the last ticcmd the server
// has acknowledged processing.
void CL_PredictLocalPlayer (int predtic)
{
	player_t &p = consoleplayer();
	
	if (!p.ingame() || !p.mo || p.spectator || p.tic >= predtic)
		return;
		
	int bufpos = predtic % MAXSAVETICS;

	// backup the consoleplayer's ticcmd
	ticcmd_t *cmd = &(p.cmd);
	memcpy(cmd, &localcmds[bufpos], sizeof(ticcmd_t));

	p.mo->angle			= cl_angle[bufpos];
	p.mo->pitch			= cl_pitch[bufpos];
	p.viewheight		= cl_viewheight[bufpos];
	p.deltaviewheight	= cl_deltaviewheight[bufpos];
	p.jumpTics			= cl_jumpTics[bufpos];
	p.mo->reactiontime	= cl_reactiontime[bufpos];
	p.mo->waterlevel	= cl_waterlevel[bufpos];

	if (p.playerstate != PST_DEAD)
		P_MovePlayer(&p);

	if (!predicting)
		P_PlayerThink(&p);

	p.mo->RunThink();
}


//
// CL_IsValidOtherPlayer
//
// Indicates if a player is in the game and not the console player
//
bool CL_IsValidOtherPlayer(player_t &player)
{
	return (player.id != consoleplayer().id &&
			player.ingame() &&
			player.mo && !player.spectator);
}

//
// CL_ExtrapolatePlayers
//
void CL_ExtrapolatePlayers ()
{
	const int maximum_extrapolation = 1;

	extrapolation_tics = gametic - last_player_update;
	if (extrapolation_tics > maximum_extrapolation)
		extrapolation_tics = maximum_extrapolation;

	if (!last_player_update)
		extrapolation_tics = 0;

	for (size_t i = 0; i < players.size(); i++)
	{
		if (CL_IsValidOtherPlayer(players[i]))
			CL_ResetPlayer(players[i]);
	}

    #ifdef _PRED_DBG
	Printf(PRINT_HIGH, "tic %d, extrapolating %d tics.\n", gametic, extrapolation_tics);
	#endif // _PRED_DBG

	for (int j = 1; j <= extrapolation_tics; j++)
	{
		for (size_t i = 0; i < players.size(); i++)
		{
			if (CL_IsValidOtherPlayer(players[i]))
				players[i].mo->RunThink();
		}
	}
}


//
// CL_NudgeThing
//
// Moves a thing's position a percentage of the way between it's initial
// position and dest_pos, determined by the parameter amount,
// where amount is between 0 and 1.  If the difference between the initial
// position and dest_pos is below nudge_threshold, the position will be snapped
// all the way to dest_pos.
//

void CL_NudgeThing(AActor *thing, vec3_t dest_pos, float amount = 0.1f)
{
	const float nudge_threshold = 2.0f;

	if (amount < 0.0f || amount > 1.0f)
		amount = 1.0f;

	vec3_t start_pos, delta;
	VectorPosition(thing, start_pos);
	VectorSubtract(dest_pos, start_pos, delta);

    #ifdef _PRED_DBG
	if (VectorLength(delta) >= 0.01f)
		Printf(PRINT_HIGH, "Prediction off by %0.2f units.\n", VectorLength(delta));
	#endif // _PRED_DBG

	if (VectorLength(delta) <= nudge_threshold)
		amount = 1.0f;	// snap directly to dest_pos since it won't be noticable

	vec3_t scaled_delta, nudged_pos;
	VectorScale(delta, amount, scaled_delta);

	VectorAdd(start_pos, scaled_delta, nudged_pos);
	PositionVector(nudged_pos, thing);
}

//
// CL_PredictMove
//
// Main function for client-side prediction.
// 
void CL_PredictMove (void)
{
	player_t *p = &consoleplayer();

	if (!validplayer(*p) || !p->mo || noservermsgs || netdemo.isPaused())
		return;
	
	if (!p->spectator && !p->tic)	// No verified position from the server
		return;

	// Save player angle, viewheight,deltaviewheight and jumpTics.
	// Will use it later to predict movements
	int bufpos = gametic % MAXSAVETICS;
	
	cl_angle[bufpos]			= p->mo->angle;
	cl_pitch[bufpos]			= p->mo->pitch;
	cl_viewheight[bufpos]		= p->viewheight;
	cl_deltaviewheight[bufpos]	= p->deltaviewheight;
	cl_jumpTics[bufpos]			= p->jumpTics;
	cl_reactiontime[bufpos]		= p->mo->reactiontime;
    cl_waterlevel[bufpos]		= p->mo->waterlevel;

	// Backup the position predicted in the previous tic 
	vec3_t last_predicted_pos;
	VectorPosition(p->mo, last_predicted_pos);

	// Disable sounds, etc, during prediction
	predicting = true;

	// Set predictable items to their last received positions
	CL_ResetSectors();

	CL_ExtrapolatePlayers();

	CL_ResetPlayer(consoleplayer());
	int predtic = consoleplayer().tic >= 0 ? consoleplayer().tic : 0;

	// Predict each tic
	while(++predtic < gametic)
	{
		CL_PredictSectors(predtic);
		CL_PredictLocalPlayer(predtic);
	}

	predicting = false;

	vec3_t corrected_pos;
	VectorPosition(p->mo, corrected_pos);	// backup the corrected prediction
	PositionVector(last_predicted_pos, p->mo);	// restore the previous tic's prediction

	// Move the player's position towards the corrected position predicted based
	// on recent position data from the server.  This avoids the disorienting
	// "jerk" when the corrected position is drastically different from
	// the previously predicted position.
	const float nudge_distance_percentage = 0.1f;
	CL_NudgeThing(p->mo, corrected_pos, nudge_distance_percentage);
	
	CL_PredictLocalPlayer(gametic);
    //CL_PredictSectors(gametic);

	// Ensure the viewheight is correct for whatever player we're viewing 
	P_CalcHeight(&displayplayer());
}

VERSION_CONTROL (cl_pred_cpp, "$Id$")

