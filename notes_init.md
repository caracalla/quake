# Quake Initialization Notes

## `main`

1. Initialization:
	1. initializes quake params (parms) to 0
	1. `COM_InitArgv` inits arguments
	1. sets parms.argv and parms.argc with the output from COM_InitArgv
	1. sets parms.memsize according to if GLQUAKE is defined (where is that defined though?)
		* if `-mem` is in the args, sets parms.memsize according to that instead
	1. allocates parms.membase
		* this is the only place memory is allocated for the game.  other allocations happen for VCR recording and X video
	1. `fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY)` ???
		* 0 is stdin
		* `F_GETFL` reads fd flags
		* `F_SETFL` sets fd flags
		* `FNDELAY` sets `read()` to be non-blocking (returns -1 without input)
	1. `Host_Init` does a crapload of setup
	1. `Sys_Init` does nothing
1. Game loop:
	* See `notes_loop.md`

## `COM_InitArgv`

1. for up to 50 (`MAX_NUM_ARGVS`) args, and up to a total of 255 characters (`CMDLINE_LENGTH`), all args are fed into `com_cmdline`, delimited by spaces
1. all args are fed into `largv`
1. if `-safe` is passed in, `safeargvs` are added to `largv`
	* this is equivalent to passing in:
		* `-stdvid`
		* `-nolan`
		* `-nosound`
		* `-nocdaudio`
		* `-nojoy`
		* `-nomouse`
		* `-dibonly`
	* for some reason, a single space string is added to the end of `largv` (`argvdummy`)
1. `largv` is copied into `com_argv`
1. `rogue` and `hipnotic` args are checked

## `COM_CheckParm`

Returns the index in `com_argv` of the desired arg, returns 0 if not found

## `Host_Init`

1. `Memory_Init` - inits the hunk, cache, and zone memory
1. `Cbuf_Init` - allocates 8192 bytes for the command buffer `cmd_text`
1. `Cmd_Init` - adds commands like exec, echo, alias, cmd, wait
1. `V_Init` - adds commands and registers a ton of cvars related to player view
1. `Chase_Init` - registers camera chase cvars
1. `Host_InitVCR` - inits playback or record if desired
1. `COM_Init` - checks for endianness, registers some cvars, adds path command, inits filesystem, and does registration check
	1. `COM_InitFilesystem` - loads files from `basedir`, sets up `cachedir`, adds game directories
		* what's the deal with `cachedir`?
		1. `COM_AddGameDirectory` - iterates through `pakN.pak` files in the directory
			1. `COM_LoadPackFile` - reads header, checks if it's been modified (`com_modified`), load references to packfiles into hunk, keeps the packfile open
1. `Host_InitLocal` - adds a crapton of commands, registers more cvars, sets host time (what's a think?)
1. `W_LoadWadFile` - loads gfx.wad (what's `LittleLong`? a cast?)
1. `Key_Init` - registers keys that can't be rebound, shifted keys, and binding commands
1. `Con_Init` - inits the console and adds relevant commands
1. `M_Init` - adds commands for menu subsystem
1. `PR_Init` - adds commands to print edict info and registers cvars
1. `Mod_Init` - sets `mod_novis` to all 1s
1. `NET_Init` - inits networking, sets cvars, adds commands
1. `SV_Init` - inits server cvars, sets up `localmodels`
1. `R_InitTextures` - sets up textures, including the checkerboard default
1. loads base palette and colormap
1. `IN_Init` - registers cvars, sets mouse position to 0, 0
1. `VID_Init` - initialize X with the base palette
1. `Draw_Init` - init drawing information from WAD
1. `SCR_Init` - register screen cvars, draws pics from WAD (what's the turtle?)
1. `R_Init` - registers rendering cvars
1. `S_Init` - init sound
1. `CDAudio_Init` - init CD audio playback
1. `Sbar_Init` - draws the status bar
1. `CL_Init` - initializes client
1. add `exec quake.rc` to command buffer
1. sets the hunk low mark?
1. sets `host_initialized` to true, marking the end of initialization, and preventing any further commands from being added

### Commands Added in Host_Init
* command processing
	* stuffcmds - `Cmd_StuffCmds_f`
	* exec - `Cmd_Exec_f`
	* echo - `Cmd_Echo_f`
	* alias - `Cmd_Alias_f`
	* cmd - `Cmd_ForwardToServer`
	* wait - `Cmd_Wait_f`
* view
	* v_cshift - `V_cshift_f`
	* bf - `V_BonusFlash_f`
	* centerview - `V_StartPitchDrift`
* filepath
	* path - `COM_Path_f`
* host
	* status - `Host_Status_f`
	* quit - `Host_Quit_f`
	* god - `Host_God_f`
	* notarget - `Host_Notarget_f`
	* fly - `Host_Fly_f`
	* map - `Host_Map_f`
	* restart - `Host_Restart_f`
	* changelevel - `Host_Changelevel_f`
	* changelevel2 - `Host_Changelevel2_f` (quake 2 only)
	* connect - `Host_Connect_f`
	* reconnect - `Host_Reconnect_f`
	* name - `Host_Name_f`
	* noclip - `Host_Noclip_f`
	* version - `Host_Version_f`
	* please - `Host_Please_f` (idgods only)
	* say - `Host_Say_f`
	* say_team - `Host_Say_Team_f`
	* tell - `Host_Tell_f`
	* color - `Host_Color_f`
	* kill - `Host_Kill_f`
	* pause - `Host_Pause_f`
	* spawn - `Host_Spawn_f`
	* begin - `Host_Begin_f`
	* prespawn - `Host_PreSpawn_f`
	* kick - `Host_Kick_f`
	* ping - `Host_Ping_f`
	* load - `Host_Loadgame_f`
	* save - `Host_Savegame_f`
	* give - `Host_Give_f`
	* startdemos - `Host_Startdemos_f`
	* demos - `Host_Demos_f`
	* stopdemo - `Host_Stopdemo_f`
	* viewmodel - `Host_Viewmodel_f`
	* viewframe - `Host_Viewframe_f`
	* viewnext - `Host_Viewnext_f`
	* viewprev - `Host_Viewprev_f`
	* mcache - `Mod_Print`
* key bindings
	* bind - `Key_Bind_f`
	* unbind - `Key_Unbind_f`
	* unbindall - `Key_Unbindall_f`
* console
	* toggleconsole - `Con_ToggleConsole_f`
	* messagemode - `Con_MessageMode_f`
	* messagemode2 - `Con_MessageMode2_f`
	* clear - `Con_Clear_f`
* menu
	* togglemenu - `M_ToggleMenu_f`
	* menu_main - `M_Menu_Main_f`
	* menu_singleplayer - `M_Menu_SinglePlayer_f`
	* menu_load - `M_Menu_Load_f`
	* menu_save - `M_Menu_Save_f`
	* menu_multiplayer - `M_Menu_MultiPlayer_f`
	* menu_setup - `M_Menu_Setup_f`
	* menu_options - `M_Menu_Options_f`
	* menu_keys - `M_Menu_Keys_f`
	* menu_video - `M_Menu_Video_f`
	* help - `M_Menu_Help_f`
	* menu_quit - `M_Menu_Quit_f`
* edits
	* edict - `ED_PrintEdict_f`
	* edicts - `ED_PrintEdicts`
	* edictcount - `ED_Count`
	* profile - `PR_Profile_f`
* net
	* slist - `NET_Slist_f`
	* listen - `NET_Listen_f`
	* maxplayers - `MaxPlayers_f`
	* port - `NET_Port_f`
* screen
	* screenshot - `SCR_ScreenShot_f`
	* sizeup - `SCR_SizeUp_f`
	* sizedown - `SCR_SizeDown_f`
* rendering?
	* timerefresh - `R_TimeRefresh_f`
	* pointfile - `R_ReadPointFile_f`
* sound
	* play - `S_Play`
	* playvol - `S_PlayVol`
	* stopsound - `S_StopAllSoundsC`
	* soundlist - `S_SoundList`
	* soundinfo - `S_SoundInfo_f`
* status bar
	* +showscores - `Sbar_ShowScores`
	* -showscores - `Sbar_DontShowScores`
* client
	* entities - `CL_PrintEntities_f`
	* disconnect - `CL_Disconnect_f`
	* record - `CL_Record_f`
	* stop - `CL_Stop_f`
	* playdemo - `CL_PlayDemo_f`
	* timedemo - `CL_TimeDemo_f`

## `SV_SpawnServer`

Called when loading a save or starting a map

1. sets `scr_centertime_off` to 0 (??)
1. sets `svs.changelevel_issued` to false, allowing another changelevel?
1. tells all clients to reconnect
1. sets `skill` and `current_skill`
1. `Host_ClearMemory` and `memset(&sv, 0, sizeof(sv))`
1. sets `sv.name` with the level's name or whatever
1. `PR_LoadProgs()` - sets all the `pr_*` globals

```c
strcpy(sv.name, server);  // the level name
sv.max_edicts = MAX_EDICTS;
sv.edicts = Hunk_AllocName(sv.max_edicts * pr_edict_size, "edicts");
sv.datagram.maxsize = sizeof(sv.datagram_buf);
sv.datagram.cursize = 0;
sv.datagram.data = sv.datagram_buf;
sv.reliable_datagram.maxsize = sizeof(sv.reliable_datagram_buf);
sv.reliable_datagram.cursize = 0;
sv.reliable_datagram.data = sv.reliable_datagram_buf;
sv.signon.maxsize = sizeof(sv.signon_buf);
sv.signon.cursize = 0;
sv.signon.data = sv.signon_buf;
sv.num_edicts = svs.maxclients + 1;
sv.state = ss_loading;
sv.paused = false;
sv.time = 1.0;
sprintf(sv.modelname, "maps/%s.bsp", server);
sv.worldmodel = Mod_ForName(sv.modelname, false); // model_t*
	model_t* model = Mod_FindName(sv.modelname);
		// walks `mod_known` to find a free spot and puts the model_t* there
		strcpy(model->name, sv.modelname);
		return model;
	return Mod_LoadModel(mod, false);
		// I really don't get this, it allocates the model into 1024 bytes?
		// then it calls the appropriate `Mod_Load[...]Model` function
sv.models[1] = sv.worldmodel;
sv.sound_precache[0] = pr_strings;
sv.model_precache[0] = pr_strings;
sv.model_precache[1] = sv.modelname;
for (int i = 1; i < sv.worldmodel->numsubmodels; i++) {
	sv.model_precache[1 + i] = localmodels[i];
	sv.models[i + 1] = Mod_ForName (localmodels[i], false);
}



edict_t* ent = EDICT_NUM(0);
memset(&ent->v, 0, progs->entityfields * 4);
ent->free = false;
ent->v.model = sv.worldmodel->name - pr_strings;
ent->v.modelindex = 1;  // world model
ent->v.solid = SOLID_BSP;
ent->v.movetype = MOVETYPE_PUSH;
```
