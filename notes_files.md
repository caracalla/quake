# Quake File Notes

## Headers

### `bspfile.h`
definitions of the BSP file format

* **Types:**
	* `lump_t` - ?
	* `dmodel_t` - ?
	* `dheader_t` - ?
	* `dmiptexlump_t` - ?
	* `miptex_t` - ?
	* `dvertex_t` - ?
	* `dplane_t` - ?
	* `dnode_t` - ?
	* `dclipnode_t` - ?
	* `texinfo_t` - ?
	* `dedge_t` - ?
	* `dface_t` - ?
	* `dleaf_t` - ?
	* `epair_t` - ?
	* `entity_t` - ?
* **Functions:**
	* `void ParseEntities(void)`
	* `void UnparseEntities(void)`
	* `void SetKeyValue(entity_t* ent, char* key, char* value)`
	* `char* ValueForKey(entity_t* ent, char* key)`
	* `vec_t	FloatForKey (entity_t* ent, char* key)`
	* `void 	GetVectorForKey (entity_t* ent, char* key, vec3_t vec)`
	* `epair_t* ParseEpair (void)`
	* **NOTE:** no clue where these are actually defined



### `client.h`
network client logic

* **Source files:**
	* `cl_demo.c` - demo recording and playback
	* `cl_input.c` - builds an intended movement command to send to the server
	* `cl_main.c` - client main loop
	* `cl_parse.c` - parse messages from the server
	* `cl_tent.c` - client temporary entity handling (explosions and stuff?)
* **Types:**
	* `usercmd_t` - ?
	* `lightstyle_t` - ?
	* `scoreboard_t` - ?
	* `cshift_t` - ?
	* `dlight_t` - ?
	* `beam_t` - ?
	* `cactive_t` - ?
	* `client_static_t` - ?
	* `client_state_t` - ?
	* `kbutton_t` - ?
* **Functions:**



### `d_iface.h`
rasterization drivers

* **Source files:**
	* `d_edge.c` -
	* `d_fill.c` -
	* `d_iface.h` -
	* `d_init.c` -
	* `d_local.h` -
	* `d_modech.c` -
	* `d_pat.c` -
	* `d_polyse.c` -
	* `d_scan.c` -
	* `d_sky.c` -
	* `d_sprite.c` -
	* `d_surf.c` -
	* `d_vars.c` -
	* `d_zpoint.c` -
* **Types:**
* **Functions:**



### `model.h`

* **Source



### `quakedef.h`
seems like a convenience header to hold common definitions

* **Source files:**
	* `chase.c` - chase camera logic
	* `host_cmd.c` - adds a ton of console commands in `Host_InitCommands`
	* `host.c` -
* **Types:**
* **Functions:**



### `template.h`
info

* **Source files:**
* **Types:**
* **Functions:**



### Misc

* `adivtab.h` - a table of quotients and remainders for values between -15 and 16
	* Included strategically in `d_polyse.c` to create the `adivtab` global
* `anorms.h` - precalculated normal vectors
	* Included strategically in `r_alias.c` to create the `r_avertexnormals` global
	* `r_avertexnormals` is used in `r_alias.c` for lighting and `r_part.c` for particle effects
* `cmd.h` (`cmd.c`) - script command processing
	* **Enum** `cmd_source_t` - determines whether the command came over a net connection as a clc_stringcmd (`src_client`), or from the command buffer (`src_command`)
* `common.h` (`common.c`) - misc functions used by both client and server
	* file loading:
		* `int COM_FindFile(char* filename, int* handle, FILE** file)` - optionally caches the file (?)
		* `int COM_OpenFile(char* filename, int* handle)` - calls `COM_FindFile` with a null `file`
		* `int COM_FOpenFile(char* filename, FILE** file)` - calls `COM_FindFile` with a null `handle`
		* `byte* COM_LoadFile(char* path, int usehunk)` - calls `COM_OpenFile`, then allocates a buffer based on `usehunk` and reads the file into that buffer
		* `COM_LoadHunkFile`, `COM_LoadTempFile`, `COM_LoadCacheFile`, `COM_LoadStackFile` - calls `COM_LoadFile`
* `console.h` (`console.c`) -
* `crc.h` (`crc.c`) -
* `cvar.h` (`cvar.c`) -
* `draw.h` (`draw.c`) - functions to draw to the vid buffer outside of "refresh"
	* **Functions:**
		* `void Draw_Init(void)` - loads font, disc icon, and background
		* `void Draw_Character(int x, int y, int num)` - draws a character from the font
		* `void Draw_DebugChar(char num)` - only works with direct FB access
		* `void Draw_Pic(int x, int y, qpic_t* pic)`
		* `void Draw_TransPic(int x, int y, qpic_t* pic)`
		* `void Draw_TransPicTranslate(int x, int y, qpic_t* pic, byte* translation)`
		* `void Draw_ConsoleBackground(int lines)`
		* `void Draw_BeginDisc(void)`
		* `void Draw_EndDisc(void)`
		* `void Draw_TileClear(int x, int y, int w, int h)`
		* `void Draw_Fill(int x, int y, int w, int h, int c)`
		* `void Draw_FadeScreen(void)`
		* `void Draw_String(int x, int y, char* str)`
		* `qpic_t* Draw_PicFromWad(char* name)` - used to get pic from lump names for init methods
		* `qpic_t* Draw_CachePic(char* path)` - loads pic, writethrough cache
* `input.h` - mouse and keyboard input
* `keys.h` (`keys.c`) - keyboard keys
* `mathlib.h` (`mathlib.c`) - mostly vector math
* `menu.h` (`menu.c`) - game menu handling (`menu.c` is huge!)
*
