/*****************************************************************************
   Project: CEDAR Logic Simulator
   Copyright 2006 Cedarville University, Benjamin Sprague,
                     Matt Lewellyn, and David Knierim
   All rights reserved.
   For license information see license.txt included with distribution.   

   PaletteCanvas: Renders the gateImage objects in a palette
*****************************************************************************/

#include "PaletteCanvas.h"

using namespace std;

DECLARE_APP(MainApp)


BEGIN_EVENT_TABLE(PaletteCanvas, wxScrolledWindow)
  EVT_PAINT(PaletteCanvas::OnPaint)
END_EVENT_TABLE()


PaletteCanvas::PaletteCanvas( wxWindow *parent, wxWindowID id, wxString &libName, const wxPoint &pos, const wxSize &size ) 
	: wxScrolledWindow( parent, id, pos, size, wxSUNKEN_BORDER|wxVSCROLL|wxFULL_REPAINT_ON_RESIZE ) {
    SetBackgroundColour(* wxWHITE);

    SetCursor(wxCursor(wxCURSOR_ARROW));
	SetBackgroundColour(* wxWHITE);

	libraryName = (string)((const char *)libName.c_str());  // KAS
	gateSizer = NULL;
	
	scrollPos = 0;
	init = false;
	activate = true;

}

PaletteCanvas::~PaletteCanvas() {
	for (unsigned int i = 0; i < gates.size(); i++) delete gates[i];
}

void PaletteCanvas::OnPaint( wxPaintEvent &event ) {
	wxPaintDC dc(this);
	if (!init) {
	   	map < string, LibraryGate >::iterator gateWalk = wxGetApp().libraries[libraryName].begin();
		int counter = 0;
		wxBoxSizer* lineSizer = NULL;
		gateSizer = new wxBoxSizer( wxVERTICAL );
		while (gateWalk != wxGetApp().libraries[libraryName].end()) {
			// Pedro Casanova (casanova@ujaen.es) 2020/04-12	4 columns of components instead of 3
			if (!(counter % 4)) {
				lineSizer = new wxBoxSizer( wxHORIZONTAL );
				gateSizer->Add( lineSizer, wxSizerFlags(0).Border(wxALL, 1) );			
			}	
			gateImage* newGate = new gateImage((gateWalk->first), this, wxID_ANY, wxDefaultPosition, wxSize(IMAGESIZE, IMAGESIZE));
			gates.push_back(newGate);
			lineSizer->Add( newGate, wxSizerFlags(0).Border(wxALL, 1) );
			counter++;
			gateWalk++;
		}
		this->SetSizer( gateSizer );
		this->SetScrollRate(0, IMAGESIZE + 1);
		gateSizer->Layout();
		init = true;		
	}
	if (activate) {
		this->FitInside();
		this->Scroll(0,scrollPos); // reset position because the sizer is dumb
		gateSizer->Layout();
		activate = false;
	}
}
void PaletteCanvas::Activate() {
	activate = true;
}

// Pedro Casanova (casanova@ujaen.es) 2021/01-03
// To remember scroll position
void PaletteCanvas::Deactivate() {
	scrollPos = this->GetScrollPos(1);
}