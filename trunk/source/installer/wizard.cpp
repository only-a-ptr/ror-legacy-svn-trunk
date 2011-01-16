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

#include "wizard.h"
#include "cevent.h"
#include "installerlog.h"
// For compilers that support precompilation, includes "wx/wx.h".
#include <wx/wxprec.h>
#include <wx/filename.h>
#include <wx/dataobj.h>
#include <wx/cmdline.h>
#include <wx/dir.h>
#include <wx/thread.h>
#include <wx/event.h>
#include <wx/fs_inet.h>
#include <wx/html/htmlwin.h>
#include <wx/settings.h>
#include "wthread.h"
#include "cevent.h"
#include "installerlog.h"

// for all others, include the necessary headers
#ifndef WX_PRECOMP
	#include "wx/frame.h"
	#include "wx/stattext.h"
	#include "wx/log.h"
	#include "wx/app.h"
	#include "wx/checkbox.h"
	#include "wx/checklst.h"
	#include "wx/msgdlg.h"
	#include "wx/radiobox.h"
	#include "wx/menu.h"
	#include "wx/sizer.h"
	#include "wx/textctrl.h"
	#include "wx/button.h"
	#include "wx/dirdlg.h"
	#include "wx/filename.h"
	#include "wx/dir.h"
	#include "wx/choice.h"
	#include "wx/gauge.h"
	#include "wx/timer.h"
	#include "wx/scrolwin.h"
#endif

#include "wx/wizard.h"
#include "wxStrel.h"
#include "wx/filename.h"
#include <wx/clipbrd.h>

#include "ConfigManager.h"

#include "welcome.xpm"
#include "licence.xpm"
#include "dest.xpm"
#include "streams.xpm"
#include "action.xpm"
#include "download.xpm"
#include "finished.xpm"

#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
	#include <shlobj.h> // for the special path functions
	#include "symlink.h"
	#include <shellapi.h> // for executing the binaries
#endif //OGRE_PLATFORM


BEGIN_EVENT_TABLE(DownloadPage, wxWizardPageSimple)
	EVT_TIMER(ID_TIMER, DownloadPage::OnTimer)
	EVT_MYSTATUS(wxID_ANY, DownloadPage::OnStatusUpdate )
END_EVENT_TABLE()

BEGIN_EVENT_TABLE(MyWizard, wxWizard)
	EVT_WIZARD_PAGE_CHANGING(ID_WIZARD, MyWizard::OnPageChanging)
END_EVENT_TABLE()


IMPLEMENT_APP(MyApp)


// some helper
// from wxWidetgs wiki: http://wiki.wxwidgets.org/Calling_The_Default_Browser_In_WxHtmlWindow
class HtmlWindow: public wxHtmlWindow
{
public:
	HtmlWindow(wxWindow *parent, wxWindowID id = -1,
		const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
		long style = wxHW_SCROLLBAR_NEVER|wxHW_NO_SELECTION|wxBORDER_SUNKEN, const wxString& name = _T("htmlWindow"));
	void OnLinkClicked(const wxHtmlLinkInfo& link);
};

HtmlWindow::HtmlWindow(wxWindow *parent, wxWindowID id, const wxPoint& pos,
	const wxSize& size, long style, const wxString& name)
: wxHtmlWindow(parent, id, pos, size, style, name)
{
  this->SetBorders(1);
}

void HtmlWindow::OnLinkClicked(const wxHtmlLinkInfo& link)
{
	wxString linkhref = link.GetHref();
    if(!wxLaunchDefaultBrowser(linkhref))
          // failed to launch externally, so open internally
          wxHtmlWindow::OnLinkClicked(link);
}


// ----------------------------------------------------------------------------
// the application class
// ----------------------------------------------------------------------------

static const wxCmdLineEntryDesc g_cmdLineDesc [] =
{
#if wxCHECK_VERSION(2, 9, 0)
     { wxCMD_LINE_SWITCH, ("h"), ("help"),      ("displays help on the command line parameters"), wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP },
     { wxCMD_LINE_SWITCH, ("n"), ("noupdate"),  ("ignore available updates"), wxCMD_LINE_VAL_NONE  },
     { wxCMD_LINE_SWITCH, ("d"), ("hash"),      ("put installer hash into clipboard"), wxCMD_LINE_VAL_NONE  },
#else
     { wxCMD_LINE_SWITCH, wxT("h"), wxT("help"),      wxT("displays help on the command line parameters"), wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP },
#endif //wxCHECK_VERSION
	 { wxCMD_LINE_NONE }
};

// `Main program' equivalent: the program execution "starts" here
bool MyApp::OnInit()
{
	autoUpdateEnabled=true;

    // call default behaviour (mandatory)
    if (!wxApp::OnInit())
        return false;

	MyWizard wizard(startupMode, NULL, autoUpdateEnabled);

	wizard.RunWizard(wizard.GetFirstPage());

	// we're done
	exit(0);

	// this crashes the app:
	return true;
}

void MyApp::OnInitCmdLine(wxCmdLineParser& parser)
{
    parser.SetDesc (g_cmdLineDesc);
    // must refuse '/' as parameter starter or cannot use "/path" style paths
    parser.SetSwitchChars (wxT("-"));
}

bool MyApp::OnCmdLineParsed(wxCmdLineParser& parser)
{
	// ignore auto-update function
    if(parser.Found(wxT("n"))) autoUpdateEnabled=false;
	

#if wxCHECK_VERSION(2, 9, 0)
	// special mode: put our hash into the clipboard
	if(parser.Found(wxT("d")))
	{
		if (wxTheClipboard->Open())
		{
			wxString txt = ConfigManager::getExecutablePath() + wxT(" ") + conv(ConfigManager::getOwnHash()) + wxT(" ") + wxT(__TIME__) + wxT(" ") + wxT(__DATE__);
			wxTheClipboard->SetData(new wxTextDataObject(txt));
			wxTheClipboard->Flush();
			wxTheClipboard->Close();
		}
		exit(0);
	}
#endif
    return true;
}


// ----------------------------------------------------------------------------
// MyWizard
// ----------------------------------------------------------------------------


MyWizard::MyWizard(int startupMode, wxFrame *frame, bool _autoUpdateEnabled, bool useSizer)
        : wxWizard(frame,ID_WIZARD,_T("Rigs of Rods Update Assistant"),
                   wxBitmap(licence_xpm),wxDefaultPosition,
                   wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER), startupMode(startupMode),
				   autoUpdateEnabled(_autoUpdateEnabled)
{
	// first thing to do: remove old installer file if possible
	if(boost::filesystem::exists("installer.exe.old"))
		boost::filesystem::remove("installer.exe.old");
	// now continue with normal startup

	new ConfigManager();

	// create log
	boost::filesystem::path iPath = boost::filesystem::path(conv(CONFIG->getInstallationPath()));
	boost::filesystem::path lPath = iPath / std::string("wizard.log");
	new InstallerLog(lPath);
	LOG("installer log created");

	// check if there is a newer installer available
	if(autoUpdateEnabled)
		CONFIG->checkForNewUpdater();

    PresentationPage *presentation = new PresentationPage(this);
	DownloadPage *download = new DownloadPage(this);
	LastPage *last = new LastPage(this);

	m_page1 = presentation;

	wxWizardPageSimple::Chain(presentation, download);
	wxWizardPageSimple::Chain(download, last);

    if ( useSizer )
    {
        // allow the wizard to size itself around the pages
        GetPageAreaSizer()->Add(presentation);
    }
}

void MyWizard::OnPageChanging(wxWizardEvent &event)
{
	wxWizardPage *wp=event.GetPage();
	EnterLeavePage *elp=dynamic_cast<EnterLeavePage*>(wp);
	if (elp)
	{
		if (event.GetDirection())
		{
			//forward
			bool b=elp->OnLeave(true);
			if (!b) {event.Veto();return;}
			wxWizardPage *nwp=wp->GetNext();
			EnterLeavePage *nelp=dynamic_cast<EnterLeavePage*>(nwp);
			if (nelp)
			{
				bool b=nelp->OnEnter(true);
				if (!b) event.Veto();
			}
		}
		else
		{
			//backward
			bool b=elp->OnLeave(false);
			if (!b) {event.Veto();return;}
			wxWizardPage *nwp=wp->GetPrev();
			EnterLeavePage *nelp=dynamic_cast<EnterLeavePage*>(nwp);
			if (nelp)
			{
				bool b=nelp->OnEnter(false);
				if (!b) event.Veto();
			}
		}
	}
}


// utils for the wizard
inline void setControlEnable(wxWizard *wiz, int id, bool enable)
{
	wxWindow *win = wiz->FindWindowById(id);
	if(win) win->Enable(enable);
}

// ----------------------------------------------------------------------------
// Wizard pages
// ----------------------------------------------------------------------------
//// PresentationPage
PresentationPage::PresentationPage(wxWizard *parent) : wxWizardPageSimple(parent)
{
	m_bitmap = wxBitmap(welcome_xpm);
	wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);
	wxStaticText *tst;
	//GetParent()->SetBackgroundColour(*wxWHITE);
	mainSizer->Add(tst=new wxStaticText(this, wxID_ANY, _T("Welcome to the online updater of Rigs of Rods\n")), 0, wxALL, 5);
	wxFont dfont=tst->GetFont();
	dfont.SetWeight(wxFONTWEIGHT_BOLD);
	dfont.SetPointSize(dfont.GetPointSize()+4);
	tst->SetFont(dfont);
	tst->Wrap(TXTWRAP);
	mainSizer->Add(tst=new wxStaticText(this, wxID_ANY, _T("This program will help you update Rigs of Rods on your computer.\n")), 0, wxALL, 5);
	tst->Wrap(TXTWRAP);
	mainSizer->Add(tst=new wxStaticText(this, wxID_ANY, _T("Before updating, make sure Rigs of Rods is not running, and that your internet connection is available.\n")), 0, wxALL, 5);
	tst->Wrap(TXTWRAP);
	mainSizer->Add(tst=new wxStaticText(this, wxID_ANY, _T("If you are using a firewall, please allow this program to access the Internet.\n")), 0, wxALL, 5);
	tst->Wrap(TXTWRAP);
	mainSizer->Add(tst=new wxStaticText(this, wxID_ANY, _T("Click on Next to continue.\n")), 0, wxALL, 5);
	tst->Wrap(TXTWRAP);

	SetSizer(mainSizer);
	mainSizer->Fit(this);
}

//// DownloadPage
DownloadPage::DownloadPage(wxWizard *parent) : wxWizardPageSimple(parent), wizard(parent)
{
	threadStarted=false;
	isDone=false;
	m_bitmap = wxBitmap(download_xpm);
	wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);
	
    mainSizer->Add(txtTitle=new wxStaticText(this, wxID_ANY, _T("Downloading")), 0, wxALL, 5);
	wxFont dfont=txtTitle->GetFont();
	dfont.SetWeight(wxFONTWEIGHT_BOLD);
	dfont.SetPointSize(dfont.GetPointSize()+3);
	txtTitle->SetFont(dfont);
	txtTitle->Wrap(TXTWRAP);
     

	// status text and progress bar
	statusList = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxSize(20, 160));
	mainSizer->Add(statusList, 0, wxALL|wxEXPAND, 0);
	mainSizer->Add(10, 10);
	progress=new wxGauge(this, wxID_ANY, 1000, wxDefaultPosition, wxDefaultSize, wxGA_HORIZONTAL|wxGA_SMOOTH);
	mainSizer->Add(progress, 0, wxALL|wxEXPAND, 0);
	progress->Pulse();

 	// now the information thingy
	wxGridSizer *wxg = new wxGridSizer(3, 2, 2, 5);

    wxStaticText *txt;
	/*
	// removed for now
	// download time
	txt = new wxStaticText(this, wxID_ANY, _T("Download time: "));
	wxg->Add(txt, 0, wxALL|wxEXPAND, 0);
	txt_dltime = new wxStaticText(this, wxID_ANY, _T("n/a"));
	wxg->Add(txt_dltime, 0, wxALL|wxEXPAND, 0);
	*/

	// download time
	txt = new wxStaticText(this, wxID_ANY, _T("Remaining time: "));
	wxg->Add(txt, 0, wxALL|wxEXPAND, 0);
	txt_remaintime = new wxStaticText(this, wxID_ANY, _T("n/a"));
	wxg->Add(txt_remaintime, 0, wxALL|wxEXPAND, 0);

	// traffic
	txt = new wxStaticText(this, wxID_ANY, _T("Data transferred: "));
	wxg->Add(txt, 0, wxALL|wxEXPAND, 0);
	txt_traf = new wxStaticText(this, wxID_ANY, _T("n/a"));
	wxg->Add(txt_traf, 0, wxALL|wxEXPAND, 0);

	// speed
	txt = new wxStaticText(this, wxID_ANY, _T("Average Download Speed: "));
	wxg->Add(txt, 0, wxALL|wxEXPAND, 0);
	txt_speed = new wxStaticText(this, wxID_ANY, _T("n/a"));
	wxg->Add(txt_speed, 0, wxALL|wxEXPAND, 0);

	// Local Path
	txt = new wxStaticText(this, wxID_ANY, _T("Sync Path: "));
	wxg->Add(txt, 0, wxALL|wxEXPAND, 0);
	txt_localpath = new wxStaticText(this, wxID_ANY, _T("n/a"));
	wxg->Add(txt_localpath, 0, wxALL|wxEXPAND, 0);

	// Server used
	txt = new wxStaticText(this, wxID_ANY, _T("Server used: "));
	wxg->Add(txt, 0, wxALL|wxEXPAND, 0);
	txt_server = new wxStaticText(this, wxID_ANY, _T("only main server"));
	wxg->Add(txt_server, 0, wxALL|wxEXPAND, 0);

	// Server used
	txt = new wxStaticText(this, wxID_ANY, _T("Download Jobs: "));
	wxg->Add(txt, 0, wxALL|wxEXPAND, 0);
	txt_concurr = new wxStaticText(this, wxID_ANY, wxT("none"));
	wxg->Add(txt_concurr, 0, wxALL|wxEXPAND, 0);

	// version information
	txt = new wxStaticText(this, wxID_ANY, _T("Version Info: "));
	wxg->Add(txt, 0, wxALL|wxEXPAND, 0);
	hlink = new wxHyperlinkCtrl(this, wxID_ANY, wxT("none"), wxT(""));
	wxg->Add(hlink, 0, wxALL|wxEXPAND, 0);


	// grid end
	mainSizer->Add(wxg, 0, wxALL|wxEXPAND, 5);


	mainSizer->Add(10, 10);
    // important to be able to load web URLs
    wxFileSystem::AddHandler( new wxInternetFSHandler );
    htmlinfo = new HtmlWindow(this, wxID_ANY, wxDefaultPosition, wxSize(50, 50), wxBORDER_NONE);
	mainSizer->Add(htmlinfo, 0, wxALL|wxEXPAND);
    htmlinfo->SetPage(_("."));
    timer = new wxTimer(this, ID_TIMER);
    timer->Start(10000);
    
	// FINISHED text
	txtFinish = new wxStaticText(this, wxID_ANY, _T("Finished downloading, please continue by pressing next."));
    wxFont dfont2=txtFinish->GetFont();
	dfont2.SetWeight(wxFONTWEIGHT_BOLD);
	dfont2.SetPointSize(dfont2.GetPointSize()+2);
	txtFinish->SetFont(dfont2);
	//txtFinish->Wrap(TXTWRAP);
	mainSizer->Add(txtFinish, 0, wxALL|wxEXPAND, 0);

	SetSizer(mainSizer);
	mainSizer->Fit(this);
}


void DownloadPage::startThread()
{
	if(threadStarted) return;
	threadStarted=true;
	// XXX ENABLE DEBUG
	bool debugEnabled = false;
	// XXX

	// hardcoded streams for now
	std::vector < stream_desc_t > streams;
	stream_desc_t sd;
	sd.platform = wxT("ALL");
	
	sd.path     = wxT(INSTALLER_VERSION);
	sd.path    += wxT("/");
	sd.path    += wxT(INSTALLER_PLATFORM);
	
	sd.checked  = true;
	sd.disabled = false;
	sd.del      = true;
	sd.overwrite = true;
	sd.size     = 1024;
	streams.push_back(sd);

	m_pThread = new WsyncThread(this, CONFIG->getInstallationPath(), streams);
	if ( m_pThread->Create() != wxTHREAD_NO_ERROR )
	{
		wxLogError(wxT("Can't create the thread!"));
		delete m_pThread;
		m_pThread = NULL;
	}
	else
	{
		if (m_pThread->Run() != wxTHREAD_NO_ERROR )
		{
			wxLogError(wxT("Can't create the thread!"));
			delete m_pThread;
			m_pThread = NULL;
		}

		// after the call to wxThread::Run(), the m_pThread pointer is "unsafe":
		// at any moment the thread may cease to exist (because it completes its work).
		// To avoid dangling pointers OnThreadExit() will set m_pThread
		// to NULL when the thread dies.
	}
}

bool DownloadPage::OnEnter(bool forward)
{
	txtFinish->Hide();
	//disable forward and backward buttons
	setControlEnable(wizard, wxID_FORWARD, false);
	setControlEnable(wizard, wxID_BACKWARD, false);

	// fill in version information
	std::string fromVersion = CONFIG->readVersionInfo();
	std::string toVersion   = CONFIG->getOnlineVersion();
	std::string versionText = std::string("updating from ") + fromVersion + std::string(" to ") + toVersion;
	if(fromVersion == "unkown")
		versionText = toVersion;
	if(fromVersion == toVersion)
		versionText = toVersion;
	std::string versionURL  = std::string(CHANGELOGURL) + toVersion;
	hlink->SetLabel(conv(versionText));
	hlink->SetURL(conv(versionURL));

    htmlinfo->LoadPage(wxT("http://api.rigsofrods.com/didyouknow/"));

    startThread();
	return true;
}

bool DownloadPage::OnLeave(bool forward)
{
	if(isDone && !forward) return false; //when done, only allow to go forward
	return isDone;
}

void DownloadPage::OnStatusUpdate(MyStatusEvent &ev)
{
	switch(ev.GetId())
	{
	case MSE_STARTING:
	case MSE_UPDATE_TEXT:
		statusList->Append(ev.GetString());
		statusList->SetSelection(statusList->GetCount()-1);
		break;
	case MSE_UPDATE_PROGRESS:
		progress->SetValue(ev.GetProgress() * 1000.0f);
		break;
	case MSE_DOWNLOAD_PROGRESS:
	{
		wxString str = ev.GetString();
		if(ev.GetProgress() > 0)
		{
			for(int i=statusList->GetCount()-1;i>=0;--i)
			{
				wxString str_comp = statusList->GetString(i).SubString(0, str.size()-1);
				if(str_comp == str)
				{
					statusList->SetString(i, str + wxString::Format(wxT(" (%3.1f%% downloaded)"), ev.GetProgress() * 100.0f));
					break;
				}
			}
		}
	}
		break;
	case MSE_DOWNLOAD_DONE:
	{
		wxString str = ev.GetString();
		for(int i=statusList->GetCount()-1;i>=0;--i)
		{
			wxString str_comp = statusList->GetString(i).SubString(0, str.size()-1);
			if(str_comp == str)
			{
				statusList->SetString(i, str + wxT(" (DONE)"));
				break;
			}
		}
	}
		break;
	case MSE_ERROR:
		statusList->Append(_("error: ") + ev.GetString());
		statusList->SetSelection(statusList->GetCount()-1);
		progress->SetValue(1000);
		wxMessageBox(ev.GetString(), _("Error"), wxICON_ERROR | wxOK, this);
		break;
	case MSE_UPDATE_TIME:
		//txt_dltime->SetLabel(ev.GetString());
		break;
	case MSE_UPDATE_TIME_LEFT:
		txt_remaintime->SetLabel(ev.GetString());
		break;
	case MSE_UPDATE_SPEED:
		txt_speed->SetLabel(ev.GetString());
		break;
	case MSE_UPDATE_TRAFFIC:
		txt_traf->SetLabel(ev.GetString());
		break;
	case MSE_UPDATE_PATH:
		txt_localpath->SetLabel(ev.GetString());
		break;
	case MSE_UPDATE_SERVER:
		txt_server->SetLabel(ev.GetString());
		break;
	case MSE_UPDATE_CONCURR:
		txt_concurr->SetLabel(ev.GetString());
		break;
	case MSE_UPDATE_TITLE:
		txtTitle->SetLabel(ev.GetString());
		break;
	case MSE_DONE:
		// normal end
		statusList->Append(wxT("Finished downloading."));
		statusList->SetSelection(statusList->GetCount()-1);
		progress->SetValue(1000);
		txt_remaintime->SetLabel(wxT("finished!"));
		isDone=true;
		CONFIG->writeVersionInfo(); // write the version to the file, since we updated
		// enableforward button
		txtFinish->Show();
        htmlinfo->Hide();
		setControlEnable(wizard, wxID_FORWARD, true);
		break;
	}
}
void DownloadPage::OnTimer(wxTimerEvent& event)
{
    if(htmlinfo)
      htmlinfo->LoadPage(wxT("http://api.rigsofrods.com/didyouknow/"));
}


//// LastPage
LastPage::LastPage(wxWizard *parent) : wxWizardPageSimple(parent), wizard(parent)
{
	m_bitmap = wxBitmap(finished_xpm);
	wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);
	wxStaticText *tst;
	//GetParent()->SetBackgroundColour(*wxWHITE);
	mainSizer->Add(tst=new wxStaticText(this, wxID_ANY, _T("Finished\n")), 0, wxALL, 5);
	wxFont dfont=tst->GetFont();
	dfont.SetWeight(wxFONTWEIGHT_BOLD);
	dfont.SetPointSize(dfont.GetPointSize()+4);
	tst->SetFont(dfont);
	tst->Wrap(TXTWRAP);

#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
	mainSizer->Add(tst=new wxStaticText(this, wxID_ANY, _T("Thank you for downloading Rigs of Rods.\nWhat do you want to do now?")), 0, wxALL, 5);
	tst->Wrap(TXTWRAP);


	chk_runtime = new wxCheckBox(this, wxID_ANY, _T("Install required runtime libraries now"));
	mainSizer->Add(chk_runtime, 0, wxALL|wxALIGN_LEFT, 5);
	chk_runtime->SetValue(false);

	chk_upgrade_configs = new wxCheckBox(this, wxID_ANY, _T("Update User Configurations (important)"));
	mainSizer->Add(chk_upgrade_configs, 0, wxALL|wxALIGN_LEFT, 5);

	// always enable this box, thus force the users to update always
	chk_upgrade_configs->SetValue(true);
	chk_upgrade_configs->Enable();

	chk_desktop = new wxCheckBox(this, wxID_ANY, _T("Create Desktop shortcuts"));
	mainSizer->Add(chk_desktop, 0, wxALL|wxALIGN_LEFT, 5);

	chk_startmenu = new wxCheckBox(this, wxID_ANY, _T("Create Start menu shortcuts"));
	mainSizer->Add(chk_startmenu, 0, wxALL|wxALIGN_LEFT, 5);

	chk_viewmanual = new wxCheckBox(this, wxID_ANY, _T("View manual"));
	mainSizer->Add(chk_viewmanual, 0, wxALL|wxALIGN_LEFT, 5);

	chk_configurator = new wxCheckBox(this, wxID_ANY, _T("Run the Configurator"));
	mainSizer->Add(chk_configurator, 0, wxALL|wxALIGN_LEFT, 5);

	chk_changelog = new wxCheckBox(this, wxID_ANY, _T("View the Changelog"));
	mainSizer->Add(chk_changelog, 0, wxALL|wxALIGN_LEFT, 5);

	chk_filetypes = new wxCheckBox(this, wxID_ANY, _T("Associate file types"));
	mainSizer->Add(chk_filetypes, 0, wxALL|wxALIGN_LEFT, 5);

#else
	// TODO: add linux options
	mainSizer->Add(tst=new wxStaticText(this, wxID_ANY, _T("Thank you for downloading Rigs of Rods.\n")), 0, wxALL, 5);
	tst->Wrap(TXTWRAP);
#endif // OGRE_PLATFORM
	SetSizer(mainSizer);
	mainSizer->Fit(this);
}

bool LastPage::OnEnter(bool forward)
{
	setControlEnable(wizard, wxID_BACKWARD, false);
	setControlEnable(wizard, wxID_CANCEL, false);
#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32

	chk_desktop->SetValue(true);
	if(CONFIG->getPersistentConfig(wxT("installer.create_desktop_shortcuts")) == wxT("no"))
		chk_desktop->SetValue(false);

	chk_startmenu->SetValue(true);
	if(CONFIG->getPersistentConfig(wxT("installer.create_start_menu_shortcuts")) == wxT("no"))
		chk_startmenu->SetValue(false);

	chk_viewmanual->SetValue(false);

	chk_configurator->SetValue(true);
	if(CONFIG->getPersistentConfig(wxT("installer.run_configurator")) == wxT("no"))
		chk_configurator->SetValue(false);

	chk_changelog->SetValue(true);
	if(CONFIG->getPersistentConfig(wxT("installer.view_changelog")) == wxT("no"))
		chk_changelog->SetValue(false);

	chk_filetypes->SetValue(true);
	if(CONFIG->getPersistentConfig(wxT("installer.associate_filetypes")) == wxT("no"))
		chk_filetypes->SetValue(false);

	
#endif // OGRE_PLATFORM
	return true;
}

bool LastPage::OnLeave(bool forward)
{
	if(!forward) return false;
#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
	// do last things
	CONFIG->setPersistentConfig(wxT("installer.create_desktop_shortcuts"), chk_desktop->IsChecked()?wxT("yes"):wxT("no"));
	CONFIG->setPersistentConfig(wxT("installer.create_start_menu_shortcuts"), chk_startmenu->IsChecked()?wxT("yes"):wxT("no"));
	CONFIG->setPersistentConfig(wxT("installer.run_configurator"), chk_configurator->IsChecked()?wxT("yes"):wxT("no"));
	CONFIG->setPersistentConfig(wxT("installer.view_changelog"), chk_changelog->IsChecked()?wxT("yes"):wxT("no"));
	CONFIG->setPersistentConfig(wxT("installer.associate_filetypes"), chk_filetypes->IsChecked()?wxT("yes"):wxT("no"));

	
	if(chk_desktop->IsChecked() || chk_startmenu->IsChecked())
		CONFIG->createProgramLinks(chk_desktop->IsChecked(), chk_startmenu->IsChecked());

	if(chk_runtime->IsChecked())
		CONFIG->installRuntime();

	if(chk_upgrade_configs->IsChecked())
		CONFIG->updateUserConfigs();

	if(chk_configurator->IsChecked())
		CONFIG->startConfigurator();

	if(chk_viewmanual->IsChecked())
		CONFIG->viewManual();

	if(chk_changelog->IsChecked())
		CONFIG->viewChangelog();

	if(chk_filetypes->IsChecked())
		CONFIG->associateFileTypes();

#endif // OGRE_PLATFORM
	return true;
}




