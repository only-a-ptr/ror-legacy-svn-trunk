/*
This source file is part of Rigs of Rods
Copyright 2005,2006,2007,2008,2009 Pierre-Michel Ricordel
Copyright 2007,2008,2009 Thomas Fischer

For more information, see http://www.rigsofrods.com/

Rigs of Rods is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3, as 
published by the Free Software Foundation.

Rigs of Rods is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Rigs of Rods.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"
#include <vector>

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

// for all others, include the necessary headers
#ifndef WX_PRECOMP
    #include "wx/frame.h"
    #include "wx/stattext.h"
    #include "wx/log.h"
    #include "wx/app.h"
	#include "wx/statbmp.h"
#endif



typedef struct stream_desc_t_
{
	wxString title;
	wxString path;
	wxString conflict;
	wxString desc;
	wxString group;
	wxBitmap icon;
	bool checked;
	bool forcecheck;
	bool hidden;
	bool disabled;
	bool del;
	bool beta;
	bool overwrite;
	unsigned long size;
} stream_desc_t;

class ConfigManager
{
public:
	ConfigManager();
	bool isFirstInstall();
	bool isLicenceAccepted();

	void setAction(int ac);
	int getAction();


	int uninstall();

	// stream things
	std::vector < stream_desc_t > *getStreamset();
	int getOnlineStreams();
	void setStreamSelection(stream_desc_t* desc, bool selection);
	void loadStreamSubscription();
	void saveStreamSubscription();

	// registry things
	wxString getPersistentConfig(wxString name);
	void setPersistentConfig(wxString name, wxString value);
	void setInstallPath(wxString pth);
	wxString getInstallationPath();
	wxString getInstallPath();

	void setStartupMode(int val) { this->statupMode = val; };
	int getStartupMode() { return this->statupMode; };

private:
	int statupMode;
	wxString installPath;
	int installeraction;
	int dlerror;
	std::vector < stream_desc_t > streams;
	void clearStreamset();
	void setInstallationPath();
};

#endif
