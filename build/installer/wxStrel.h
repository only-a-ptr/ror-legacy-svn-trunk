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
#ifndef WXSTREL_H
#define WXSTREL_H

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

// for all others, include the necessary headers
#ifndef WX_PRECOMP
    #include "wx/frame.h"
    #include "wx/panel.h"
    #include "wx/stattext.h"
    #include "wx/sizer.h"
	#include "wx/statbmp.h"
#endif

#include "ConfigManager.h"

#define STREL_HEIGHT 60

class wxStrel : public wxPanel
{
public:
    wxStrel(wxWindow *parent, stream_desc_t* desc) : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, STREL_HEIGHT), wxBORDER_SIMPLE)
	{
		this->desc=desc;
		if(desc->beta)
			SetBackgroundColour(wxColour(255,200,200,255));
		else
			SetBackgroundColour(*wxWHITE);
        
		wxBoxSizer *mainSizer = new wxBoxSizer(wxHORIZONTAL);
		SetSizer(mainSizer);
		mainSizer->Add(chk=new wxCheckBox(this, wxID_ANY, _T("")), 0, wxALL|wxALIGN_CENTER, 5);
		chk->SetValue(desc->checked);
		chk->Enable(!desc->disabled);
		
		mainSizer->Add(new wxStaticBitmap(this, wxID_ANY, desc->icon), 0, wxALL, 5);
        wxBoxSizer *textSizer = new wxBoxSizer(wxVERTICAL);
		wxStaticText *tst;
        
		textSizer->Add(tst=new wxStaticText(this, wxID_ANY, desc->title), 0, wxALL, 5);
		wxFont dfont=tst->GetFont();
		dfont.SetWeight(wxFONTWEIGHT_BOLD);
		dfont.SetPointSize(dfont.GetPointSize()+3);
		tst->SetFont(dfont);
		tst->Wrap(300);
        textSizer->Add(tst=new wxStaticText(this, wxID_ANY, desc->desc), 0, wxALL, 5);
		tst->Wrap(300);
		
		mainSizer->Add(textSizer, 1, wxALL|wxEXPAND , 5);
		//mainSizer->Fit(this);
	}
	stream_desc_t *getDesc() {return desc;}
	bool getSelection() {return chk->GetValue();}

private:
	wxCheckBox *chk;
	stream_desc_t *desc;
};

#endif
