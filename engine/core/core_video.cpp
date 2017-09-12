// SimpleGraphic Engine
// (c) David Gowor, 2014
//
// Module: Core Video
//

#include "common.h"
#include "system.h"

#include "core_video.h"

// ======
// Locals
// ======

#define VID_NUMMODES 11
static const int vid_modeList[VID_NUMMODES][2] = {
	1280, 1024,	// 5:4
	1024, 768,	// 4:3
	1280, 960,
	1600, 1200,
	1920, 1440,
	1280, 800,	// 16:10
	1680, 1050,
	1920, 1200,
	1280, 720,	// 16:9
	1600, 900,
	1920, 1080
};

// =====================
// core_IVideo Interface
// =====================

class core_video_c: public core_IVideo, public conCmdHandler_c {
public: 
	// Interface
	void	Apply(bool shown = true);
	void	Save();

	// Encapsulated
	core_video_c(sys_IMain* sysHnd);

	sys_IMain* sys;

	conVar_c* vid_mode;
	conVar_c* vid_display;
	conVar_c* vid_fullscreen;
	conVar_c* vid_resizable;
	conVar_c* vid_last;

	void	C_Vid_Apply(IConsole* conHnd, args_c &args);
	void	C_Vid_ModeList(IConsole* conHnd, args_c &args);
};

core_IVideo* core_IVideo::GetHandle(sys_IMain* sysHnd)
{
	return new core_video_c(sysHnd);
}

void core_IVideo::FreeHandle(core_IVideo* hnd)
{
	delete (core_video_c*)hnd;
}

core_video_c::core_video_c(sys_IMain* sysHnd)
	: conCmdHandler_c(sysHnd->con), sys(sysHnd)
{
	vid_mode		= sys->con->Cvar_Add("vid_mode", CV_ARCHIVE|CV_CLAMP, CFG_VID_DEFMODE, -1, VID_NUMMODES-1);
	vid_display		= sys->con->Cvar_Add("vid_display", CV_ARCHIVE|CV_CLAMP, CFG_VID_DEFDISPLAY, -1, 15);
	vid_fullscreen	= sys->con->Cvar_Add("vid_fullscreen", CV_ARCHIVE, CFG_VID_DEFFULLSCREEN);
	vid_resizable	= sys->con->Cvar_Add("vid_resizable", CV_ARCHIVE|CV_CLAMP, CFG_VID_DEFRESIZABLE, 0, 3);
	vid_last		= sys->con->Cvar_Add("vid_last", CV_ARCHIVE, "");

	Cmd_Add("vid_apply", 0, "", this, &core_video_c::C_Vid_Apply);
	Cmd_Add("vid_modeList", 0, "", this, &core_video_c::C_Vid_ModeList);
}

// =============
// Video Manager
// =============

void core_video_c::Apply(bool shown)
{
	// Apply video settings
	sys_vidSet_s set;
	set.shown = shown;
	set.flags = 0;
	if (vid_fullscreen->intVal) {
		set.flags|= VID_FULLSCREEN;
	} else if (vid_resizable->intVal) {
		set.flags|= VID_RESIZABLE;
		if (vid_resizable->intVal == 2) {
			set.flags|= VID_MAXIMIZE;
		} else if (vid_resizable->intVal == 3) {
			if (sscanf(vid_last->strVal, "%d,%d,%d,%d,%d", set.save.size + 0, set.save.size + 1, set.save.pos + 0, set.save.pos + 1, (int*)&set.save.maximised) == 5) {
				set.flags|= VID_USESAVED;
			} else {
				set.flags|= VID_MAXIMIZE;
			}
		}
	}
	set.display = vid_display->intVal;
	if (vid_mode->intVal >= 0) {
		set.mode[0] = __max(vid_modeList[vid_mode->intVal][0], CFG_VID_MINWIDTH);
		set.mode[1] = __max(vid_modeList[vid_mode->intVal][1], CFG_VID_MINHEIGHT);
	} else {
		set.mode[0] = 0;
		set.mode[1] = 0;
	}
	set.depth = 0;
	set.minSize[0] = CFG_VID_MINWIDTH;
	set.minSize[1] = CFG_VID_MINHEIGHT;
	sys->video->Apply(&set);
}

void core_video_c::Save()
{
	// Save video size/pos if needed
	if (vid_resizable->intVal == 3) {
		char spec[64];
		sprintf(spec, "%d,%d,%d,%d,%d", sys->video->vid.size[0], sys->video->vid.size[1], sys->video->vid.pos[0], sys->video->vid.pos[1], sys->video->vid.maximised);
		vid_last->Set(spec);
	}
}

void core_video_c::C_Vid_Apply(IConsole* conHnd, args_c &args)
{
	Apply();
}

void core_video_c::C_Vid_ModeList(IConsole* conHnd, args_c &args)
{
	sys->con->Printf("Mode -1: Use desktop resolution\n");
	for (int m = 0; m < VID_NUMMODES; m++) {
		sys->con->Printf("Mode %d: %dx%d\n", m, vid_modeList[m][0], vid_modeList[m][1]);
	}
}
