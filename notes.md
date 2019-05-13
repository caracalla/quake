# Quake Notes

## `main`

1. sets quake params (parms) to 0
1. `COM_InitArgv` inits arguments
1. sets parms argv and argc with the output from COM_InitArgv
1. sets parms.memsize according to if GLQUAKE is defined (where is that defined though?)
    * if `-mem` is in the args, sets parms.memsize according to that instead
1. allocates parms.membase
    * this is the only place memory is allocated for the game.  other allocations happen for VCR recording and X video
1. `fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY)` ???
    * is 0 file descriptor stdout?
    * what is `FNDELAY`?
1. `Host_Init` does a crapload of setup
1. `Sys_Init` does nothing
1. game loop starts
    1. sets time
    1. `Host_Frame` does everything

## `COM_InitArgv`

1. for up to 50 (`MAX_NUM_ARGVS`) args, and up to a total of 255 characters (`CMDLINE_LENGTH`), all args are fed into `com_cmdline`, delimited by spaces
1. all args are fed into `largv`
1. if `-safe` is passed in, `safeargvs` are added to `largv`
    * for some reason, a single space string is added to the end of `largv` (`argvdummy`)
1. `com_argv` is set to `largv`
1. `rogue` and `hipnotic` args are checked

## `COM_CheckParm`

Returns the index in `com_argv` of the desired arg, returns 0 if not found

## `Host_Init`
1. `Memory_Init` - inits the cache and allocates the main memory zone
1. `Cbuf_Init` - allocates 8192 bytes for the command buffer
1. `Cmd_Init` - adds commands like exec, echo, alias, cmd, wait
1. `V_Init` - adds more commands, and registers a ton of cvars
1. `Chase_Init` - registers camera chase cvars
1. `Host_InitVCR` - inits playback or record if desired
1. `COM_Init` - checks for endianness, registers some cvars, adds path command, inits filesystem, and does registration check
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

### Commands Added
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

## `_Host_Frame`
1. update random
1. check if enough time has passed to render a new frame
1. `Sys_SendKeyEvents` - get new key events from X
1. `IN_Commands` - get mouse events from X
1. `Cbuf_Execute` - execute commands in command buffer
1. `NET_Poll`
1. `CL_SendCmd` - make intentions now
1. `Host_GetConsoleCommands` - get commands typed to the host
1. `Host_ServerFrame` - ???
1. `SCR_UpdateScreen` - ???
1. update audio
1. lots of time manipulation spread around in here

# areas of interest:
* hipnotic
* rogue
* cvars
* id386

```c
// common.c
// is this safe?  how could this go wrong?
// it's safe as long as both strings have null terminators
int Q_strcmp (char *s1, char *s2)
{
    while (1)
    {
        if (*s1 != *s2)
            return -1;              // strings not equal
        if (!*s1)
            return 0;               // strings are equal
        s1++;
        s2++;
    }

    return -1;
}
```

```c
// common.h
#if !defined BYTE_DEFINED
typedef unsigned char       byte;
#define BYTE_DEFINED 1
#endif

#undef true
#undef false

typedef enum {false, true}  qboolean;
```

```c
// zone.c
#define HUNK_SENTINAL   0x1df001ed

typedef struct
{
    int     sentinal;
    int     size;       // including sizeof(hunk_t), -1 = not allocated
    char    name[8];
} hunk_t;

byte    *hunk_base;
int     hunk_size;

int     hunk_low_used;
int     hunk_high_used;
```

```c
// cvar.h
typedef struct cvar_s
{
    char    *name;
    char    *string;
    qboolean archive;       // set to true to cause it to be saved to vars.rc
    qboolean server;        // notifies players when changed
    float   value;
    struct cvar_s *next;
} cvar_t;
```

```c
// quakedef.h
typedef struct
{
    char    *basedir;
    char    *cachedir;      // for development over ISDN lines
    int     argc;
    char    **argv;
    void    *membase;
    int     memsize;
} quakeparms_t;
```

```c
// zone.c
// what the hell is this doing
size = sizeof(hunk_t) + ((size+15)&~15);
```

```c
// host.c
// does this even do anything? sets the hunk low mark?
Hunk_AllocName (0, "-HOST_HUNKLEVEL-");
```
