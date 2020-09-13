# Quake Main Loop Notes

## `main`

```
while (1) {
	elapsed_time = Sys_FloatTime() - oldtime;

	Host_Frame(elapsed_time);
}
```

1. Initialization:
	* See `notes_init.md`
1. Game loop:
	1. gets time spent rendering the last frame
	1. Call `Host_Frame` with time spent rendering



## `Host_Frame`

* if `serverprofile` is not 0, displays number of clients and uptime
* calls `_Host_Frame` with time spent rendering from `main`



## `_Host_Frame`

```
void _Host_Frame(double elapsed_time) {
	// keep the random time dependent
	rand();

	// decide the simulation time
	if (!Host_FilterTime(elapsed_time)) {
		// don't run too fast, or packets will flood out
		return;
	}

	// get new key events
	Sys_SendKeyEvents();

	// allow mice or other external controllers to add commands
	IN_Commands();

	// process console commands
	Cbuf_Execute();

	// make intentions now
	CL_SendCmd ();

	// check for commands typed to the host
	Host_GetConsoleCommands();

	Host_ServerFrame();

	host_time += host_frametime;

	// update video
	SCR_UpdateScreen();

	// update audio
	if (cls.signon == SIGNONS) {
		S_Update(r_origin, vpn, vright, vup);
		CL_DecayLights();
	} else {
		S_Update(vec3_origin, vec3_origin, vec3_origin, vec3_origin);
	}

	host_framecount++;
}

void Host_ServerFrame(void) {
	// run the world state
	pr_global_struct->frametime = host_frametime;

	// read client messages
	// this basically just runs SV_ClientThink()
	SV_RunClients();

	// move things around and think
	// always pause in single player if in console or menus
	if (!sv.paused && (svs.maxclients > 1 || key_dest == key_game) ) {
		SV_Physics();
	}

	// send all messages to the clients
	SV_SendClientMessages();
}
```

1. update random
1. check if enough time has passed to render a new frame
1. `Sys_SendKeyEvents` - get new key (and mouse?) events
	* while SDL has events pending, reads them into `keyq` with `ProcessEvent`
	* each event in `keyq` is sent to `ProcessKey`
1. `IN_Commands` - turn mouse events into key events
1. `Cbuf_Execute` - execute commands in command buffer
	* reads commands from global sizebuf `cmd_text`
	* calls `Cmd_ExecuteString` with each command
1. `NET_Poll` - network stuff
1. `CL_SendCmd` - make intentions if a client
1. `Host_GetConsoleCommands` - get commands typed to the host
1. `Host_ServerFrame` - does server stuff for clients
1. `SCR_UpdateScreen` - ???
1. update audio
1. lots of time manipulation spread around in here

## Host_ServerFrame

1. SV_ClearDatagram() - clears out the datagram buffer
1. SV_CheckForNewClients()
1. SV_RunClients() - for each client:
	* SV_ReadClientMessage()
	* SV_ClientThink() - handle movement
1. SV_Physics()
	* PR_ExecuteProgram(pr_global_struct->StartFrame)
	* for each edict runs physics
		* for clients, SV_Physics_Client()
			* PR_ExecuteProgram(pr_global_struct->PlayerPreThink)
			* SV_RunThink()
				* PR_ExecuteProgram(ent->v.think)
			* SV_AddGravity()
			* SV_CheckStuck()
			* SV_WalkMove()
			* PR_ExecuteProgram(pr_global_struct->PlayerPostThink);
1. SV_SendClientMessages()
