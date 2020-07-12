// SimpleGraphic Engine
// (c) David Gowor, 2014
//
// Module: Engine Core
//

#include "common.h"
#include "system.h"

#include "core_config.h"
#include "core_video.h"

#include "core_main.h"

#include "ui.h"

// ====================
// core_IMain Interface
// ====================

class core_main_c: public core_IMain {
public:
	// Interface
	void	Init(int argc, char** argv);
	void	Frame();
	void	Shutdown();
	void	KeyEvent(int key, int type);
	bool	CanExit();

	// Encapsulated
	core_main_c(sys_IMain* sysHnd);

	sys_IMain*		sys;

	core_IConfig*	config;
	core_IVideo*	video;
	ui_IMain*		ui;

	bool			initialised;
};

core_IMain* core_IMain::GetHandle(sys_IMain* sysHnd)
{
	return new core_main_c(sysHnd);
}

void core_IMain::FreeHandle(core_IMain* hnd)
{
	delete (core_main_c*)hnd;
}

core_main_c::core_main_c(sys_IMain* sysHnd)
	: sys(sysHnd)
{
	initialised = false;
}

// =================
// Engine Core Class
// =================

void core_main_c::Init(int argc, char** argv)
{
	// Initialise config system
	config = core_IConfig::GetHandle(sys);

	// Initialise UI Manager
	ui = ui_IMain::GetHandle(sys, config);
	ui->Init(argc, argv);
	sys->con->ExecCommands(true);

	// Hide the sysconsole
	sys->conWin->SetVisible(false);

	// Initialise video
	video = core_IVideo::GetHandle(sys);
	video->Apply();

	initialised = true;
}

void core_main_c::Frame()
{
	sys->con->Printf("Messages...\n");

	// Execute commands
	sys->con->ExecCommands();

	// Run UI
	ui->Frame();
}

void core_main_c::Shutdown()
{	
	initialised = false;

	// Shutdown video
	sys->video->SetVisible(false);
	video->Save();
	core_IVideo::FreeHandle(video);

	// Shutdown UI Manager
	ui->Shutdown();
	ui_IMain::FreeHandle(ui);

	// Shutdown config system
	core_IConfig::FreeHandle(config);

	sys->con->Printf("Engine shutdown complete.\n");
}

void core_main_c::KeyEvent(int key, int type)
{
	if ( !initialised ) return;

	ui->KeyEvent(key, type);
}

bool core_main_c::CanExit()
{
	if ( !initialised ) return true;

	return ui->CanExit();
}
