/*****************************************************************************
   Project: CEDAR Logic Simulator
   Copyright 2006 Cedarville University, Benjamin Sprague,
                     Matt Lewellyn, and David Knierim
   All rights reserved.
   For license information see license.txt included with distribution.   

   GUICanvas: Contains rendering and input functions for a page
*****************************************************************************/

#include "GUICanvas.h"
#include "MainApp.h"
#include "paramDialog.h"
#include "klsClipboard.h"
#include "guiWire.h"

// Included to use the min() and max() templates:
#include <algorithm>
#include <iostream>
using namespace std;

// Enable access to objects in the main application
DECLARE_APP(MainApp)

unsigned int renderTime = 0;
unsigned int renderNum = 0;

// GUICanvas constructor - defaults grid size to 1 unit square
GUICanvas::GUICanvas(wxWindow *parent, GUICircuit* gCircuit, wxWindowID id,
    const wxPoint& pos, const wxSize& size, long style, const wxString& name)
    : klsGLCanvas(parent, name, id, pos, size, style|wxSUNKEN_BORDER ) {

	this->gCircuit = gCircuit;
	isWithinPaste = false;
	currentDragState = DRAG_NONE;
	
	hotspotHighlight = "";
	
	drawWireHover = false;
	
	setHorizGrid(0.5);
	setVertGrid(0.5);
	
	// Add mouse object to collision checker
	mouse = new klsCollisionObject( COLL_MOUSEBOX );
	snapMouse = new klsCollisionObject( COLL_MOUSEBOX );
	collisionChecker.addObject( mouse );
	
	// Add drag selection box to collision checker
	dragselectbox = new klsCollisionObject( COLL_SELBOX );
	collisionChecker.addObject( dragselectbox );
}

GUICanvas::~GUICanvas() {
	delete snapMouse;
	delete mouse;
	delete dragselectbox;
}

// Clears the circuit by selecting all gates and wires and then running a delete command
void GUICanvas::clearCircuit() {
	selectedGates.clear();
	selectedWires.clear();
	preMove.clear();
	preMoveWire.clear();

	collisionChecker.clear();
	gateList.clear();
	wireList.clear();

	// Add mouse object to collision checker
	collisionChecker.addObject( mouse );
	
	// Add drag selection box to collision checker
	collisionChecker.addObject( dragselectbox );
	
	hotspotHighlight = "";
	potentialConnectionHotspots.clear();
	drawWireHover = false;
	isWithinPaste = false;
	saveMove = false;
}

// Inserts an existing gate onto the canvas at a particular x,y position
void GUICanvas::insertGate(unsigned long id, guiGate* gt, float x, float y) {
	if (gt == NULL) return;
	gt->setGLcoords(x, y);
	gateList[id] = gt;
	
	// Add the gate to the collision checker:
	collisionChecker.addObject( gt );
}

// Inserts an existing wire onto the canvas
void GUICanvas::insertWire(guiWire* wire) {

	if (wire == nullptr) return;

	// Make sure that each of this wire's ids are used.
	for (IDType id : wire->getIDs()) {
		wireList[id] = nullptr;
	}

	wireList[wire->getID()] = wire;

	// Add the wire to the collision checker:
	collisionChecker.addObject( wire );
}

// If the gate exists on this page, then remove it from the page
void GUICanvas::removeGate(unsigned long gid) {
	unordered_map < unsigned long, guiGate* >::iterator thisGate = gateList.find(gid);
	if (thisGate != gateList.end()) {
		// Clear a hotspot we're holding if we need to
		if (hotspotGate == gid) hotspotHighlight = "";
		
		// Take the gate out of the collision checker:
		collisionChecker.removeObject( thisGate->second );
		collisionChecker.update();

		gateList.erase(thisGate);
	}
}

// If the wire exists on this page, then remove it from the page
void GUICanvas::removeWire(unsigned long wireId) {

	if (wireList.find(wireId) == wireList.end()) return;

	guiWire *wire = wireList.at(wireId);
	collisionChecker.removeObject(wire);
	collisionChecker.update();

	// Release ID's owned by the wire.
	for (int busLineId : wire->getIDs()) {
		auto thisWire = wireList.find(busLineId);
		if (thisWire != wireList.end()) {
			wireList.erase(thisWire);
		}
	}
}

// Render the page
void GUICanvas::OnRender( bool color ) {
	glColor4f( 0.0, 0.0, 0.0, 1.0 );
	
	// Draw the wires:
	glMatrixMode (GL_MODELVIEW);
	glLoadIdentity ();
	wxStopWatch renderTimer;
	glColor4f( 0.0, 0.0, 0.0, 1.0 );
	
	// Draw the gates:
	unordered_map< unsigned long, guiGate* >::iterator thisGate = gateList.begin();
	while( thisGate != gateList.end() ) {
		// Pedro Casanova (casanova@ujaen.es) 2020/04-12
		// Deprecated components are show in magenta color
		if (color && (thisGate->second)->getLibraryName()=="Deprecated" && wxGetApp().appSettings.markDeprecated)
			glColor4f(1.0f, 0.0f, 1.0f, 1.0f);		// Magenta
		(thisGate->second)->draw(color);
		glColor4f(0.0, 0.0, 0.0, 1.0);
		thisGate++;
	}

	glLoadIdentity();
	
	// Draw the wires:
	unordered_map< unsigned long, guiWire* >::iterator thisWire = wireList.begin();
	while( thisWire != wireList.end() ) {
		if (thisWire->second != nullptr) {
			(thisWire->second)->draw(color);
		}
		thisWire++;
	}
	renderTime += renderTimer.Time();
	renderNum++;
	
	
	// Draw the basic view objects:
	glMatrixMode (GL_MODELVIEW);
	glLoadIdentity ();

	// Mouse-over hotspot
	if( hotspotHighlight.size() > 0 ) {
		// Outline the hotspots:
		GLfloat oldColor[4];
		glGetFloatv( GL_CURRENT_COLOR, oldColor );
		glColor4f( 1.0, 0.0, 0.0, 1.0 );
		glDisable( GL_LINE_STIPPLE );
	
		float diff = HOTSPOT_SCREEN_RADIUS * getZoom();
		float x, y;
		gateList[hotspotGate]->getHotspotCoords(hotspotHighlight, x, y);
		glBegin(GL_LINE_LOOP);
			glVertex2f( x - diff, y + diff );
			glVertex2f( x + diff, y + diff );
			glVertex2f( x + diff, y - diff );
			glVertex2f( x - diff, y - diff );
		glEnd();

		// Set the color back to the old color:
		glColor4f( oldColor[0], oldColor[1], oldColor[2], oldColor[3] );
	}

	// Mouse-over wire
	if( drawWireHover ) {
		// Outline the hotspot:
		GLfloat oldColor[4];
		glGetFloatv( GL_CURRENT_COLOR, oldColor );
		glColor4f( 1.0, 0.0, 0.0, 1.0 );
		glDisable( GL_LINE_STIPPLE );
	
		float diff = HOTSPOT_SCREEN_RADIUS * getZoom();
		GLPoint2f m = getMouseCoords();
		glBegin(GL_LINES);
			glVertex2f( m.x - diff, m.y + diff );
			glVertex2f( m.x + diff, m.y - diff );
		glEnd();
		glBegin(GL_LINES);
			glVertex2f( m.x + diff, m.y + diff );
			glVertex2f( m.x - diff, m.y - diff );
		glEnd();

		// Set the color back to the old color:
		glColor4f( oldColor[0], oldColor[1], oldColor[2], oldColor[3] );
	}

	// Collisions
	// Pedro Casanova (casanova@ujaen.es) 2020/04-12
	// Components collisions are not a problem. Sometines it prevents components from being together
	// It is now an application setting: ComponentCollVisible (menu and settings.ini)
	if(wxGetApp().appSettings.componentCollVisible) {
		// Draw the alpha-blended selection box over top of the overlap:
		// Pedro Casanova (casanova@ujaen.es) 2020/04-12		Added color
		if (color)
			glColor4f(0.4f, 0.1f, 0.0f, 0.3f);			// Brown
		else
			glColor4f(0, 0, 0, 0.3f);					// Grey			
		
		map< klsCollisionObjectType, CollisionGroup >::iterator ovrLists = collisionChecker.overlaps.begin();
		while( ovrLists != collisionChecker.overlaps.end() ) {
			if (ovrLists->first != COLL_GATE) { ovrLists++; continue; };
			CollisionGroup::iterator obj = (ovrLists->second).begin();
			while( obj != (ovrLists->second).end() ) {
				if ((*obj)->getType() != COLL_GATE) { obj++; continue; };
				CollisionGroup hitThings = (*obj)->getOverlaps();
				CollisionGroup::iterator hit = hitThings.begin();
				while( hit != hitThings.end() ) {
					if ((*hit)->getType() != COLL_GATE) { hit++; continue; };
					klsBBox hitBox = (*obj)->getBBox();
					hitBox = hitBox.intersect((*hit)->getBBox());
					if( !hitBox.empty() ) {
						glRectf(hitBox.getLeft(), hitBox.getBottom(), hitBox.getRight(), hitBox.getTop());
					}
					hit++;
				}
				obj++;
			}
			ovrLists++;
		}

		// Reset the color back to black:
		glColor4f( 0.0, 0.0, 0.0, 1.0 );
	}

	// Drag select box
	if (currentDragState == DRAG_SELECT) {
		// If we're in drag-select, draw the sel box

		glColor4f( 0.0, 0.4f, 1.0, 1.0 );

		// Draw the solid outline box:
		GLPoint2f start = getDragStartCoords();
		GLPoint2f end = getMouseCoords();
		glBegin(GL_LINE_LOOP);
			glVertex2f(start.x, start.y);
			glVertex2f(start.x, end.y);
			glVertex2f(end.x, end.y);
			glVertex2f(end.x, start.y);
		glEnd();


		// Draw the alpha-blended selection box over top of the gates:
		glColor4f( 0.0, 0.4f, 1.0, 0.3f );
		
		glRectf(start.x, start.y, end.x, end.y);

		// Reset the color back to black:
		glColor4f( 0.0, 0.0, 0.0, 1.0 );
	}
	// Drag-connect line 
	else if (currentDragState == DRAG_CONNECT) {
		GLPoint2f start = getDragStartCoords();
		GLPoint2f end = getMouseCoords();
		glColor4f( 0.0, 0.78f, 0.0, 1.0 );
		glBegin(GL_LINES);
			glVertex2f(start.x, start.y);
			glVertex2f(end.x, end.y);
		glEnd();
		glColor4f( 0.0, 0.0, 0.0, 1.0 );
	}
	// Draw the new gate for Drag_Newgate 
	else if (currentDragState == DRAG_NEWGATE) {
		newDragGate->draw();
	}
	
	// Draw potential connection hotspots
	glLoadIdentity ();
	glColor4f( 0.3f, 0.3f, 1.0, 1.0 );
	for (unsigned int i = 0; i < potentialConnectionHotspots.size(); i++) {
		float diff = HOTSPOT_SCREEN_RADIUS * getZoom();
		float x = potentialConnectionHotspots[i].x, y = potentialConnectionHotspots[i].y;
		glBegin(GL_LINE_LOOP);
			glVertex2f( x - diff, y + diff );
			glVertex2f( x + diff, y + diff );
			glVertex2f( x + diff, y - diff );
			glVertex2f( x - diff, y - diff );
		glEnd();
	}			
	glColor4f( 0.0, 0.0, 0.0, 1.0 );
}

void GUICanvas::mouseLeftDown(wxMouseEvent& event) {
	GLPoint2f m = getMouseCoords();

	// If I am in a paste operation then mouse-up is all I am concerned with
	if (isWithinPaste) return;

	bool handled = false;

	// Update the mouse collision object
	klsBBox mBox;
	float delta = MOUSE_HOVER_DELTA * getZoom();
	mBox.addPoint(m);
	mBox.extendTop(delta);
	mBox.extendBottom(delta);
	mBox.extendLeft(delta);
	mBox.extendRight(delta);
	mouse->setBBox(mBox);

	// Do a collision detection on all first-level objects.
	// The map collisionChecker.overlaps now contains
	// all of the objects involved in any collisions.
	collisionChecker.update();

	// Loop through all objects hit by the mouse
	//	Favor wires over gates
	CollisionGroup hitThings = mouse->getOverlaps();
	CollisionGroup::iterator hit = hitThings.begin();
	while (hit != hitThings.end() && !handled) {
		//*************************************
		//Edit by Joshua Lansford 3/16/07
		//It has been requested by students that a ctrl
		//click will select multiple gates just like
		//a shift click does.
		//thus I will replace "event.ShiftDown()"
		//everywere it appears in this file with
		//"(isLockedShiftDown()||event.ControlDown())"
		//************************************

		// Pedro Casanova (casanova@ujaen.es) 2020/04-12
		// Now permit drag from wires and hotspot
		if ((*hit)->getType() == COLL_WIRE) {
			guiWire* hitWire = ((guiWire*)(*hit));
			bool wasSelected = hitWire->isSelected();
			hitWire->unselect();
			if (hitWire->hover(m.x, m.y, WIRE_HOVER_SCREEN_DELTA * getZoom())) {
				hitWire->select();
				// ControlDown or ShiftDown to unselect gate or wire
				if ((event.ShiftDown() || event.ControlDown()) && wasSelected)
					hitWire->unselect();
				// Nor ControlDown neither ShiftDown to unselect all
				if (!(event.ShiftDown() || event.ControlDown())) {
					unselectAllWires();
					unselectAllGates();
					hitWire->select();
				}

				// Pedro Casanova (casanova@ujaen.es) 2020/04-12	It was ControlDown Only
				// ControlDown or ShiftDownto drag connection
				if ((event.ShiftDown() || event.ControlDown()) && !(this->isLocked())) {
					if (hotspotHighlight.size() == 0)
					{
						// Drag from Wire
						currentConnectionSource.isGate = false;
						currentConnectionSource.objectID = hitWire->getID();
					}
					else
					{
						// Drag from gate hotspot
						currentConnectionSource.isGate = true;
						currentConnectionSource.objectID = hotspotGate;
						currentConnectionSource.connection = hotspotHighlight;
					}
					currentDragState = DRAG_CONNECT;
				}
				else {
					// Nor ControlDown neither ShiftDown to drag wire segment
					if (!(event.ShiftDown() || event.ControlDown())) {
#ifdef _MSG_
						if (hotspotHighlight.size() == 0)
						{
							//@@@@
							_MSGGUI("wireID: %lld\n", hitWire->getID())	//@@@@
								guiWire *wire = wireList[hitWire->getID()];
							vector<wireConnection> conn(wire->getConnections());
							for (int i = 0; i < (int)conn.size(); i++) {
								_MSGGUI("... gateID: %d (%s)\n", conn[i].gid, conn[i].connection.c_str());	//@@@@
							}
							map< long, wireSegment > segm(wire->getSegmentMap());
							for (int i = 0; i < (int)segm.size(); i++) {
								_MSGGUI("... segmenID: %d (%s) (%1.1f,%1.1f)-(%1.1f,%1.1f)\n", segm[i].id, segm[i].isVertical() ? "V" : "H", segm[i].begin.x, segm[i].begin.y, segm[i].end.x, segm[i].end.y);	//@@@@
							}
							//@@@@
						}
						else
							_MSGGUI("wireID: %lld gateID: %d (%s) (%f,%f)\n", hitWire->getID(), hotspotGate, hotspotHighlight.c_str(), m.x, m.y)	//@@@@
#endif
						if (event.AltDown()) {
#ifdef _MSG_
							ostringstream oss;
							oss << "WireID:\t\t" << hitWire->getID() << "\n";
							wxMessageBox(oss.str(), "Wire properties");
#endif
						} else {
							wireHoverID = hitWire->getID();
							if (wireList[wireHoverID]->startSegDrag(snapMouse) && !(this->isLocked())) {
								currentDragState = DRAG_WIRESEG;
							}
							hitWire->unselect();
						}
					}
				}
				handled = true;
			}
			else if (wasSelected && hitThings.size() > 1) hitWire->select(); // probably dragging a selection
		}
		hit++;
	}

	// do we have a highlighted hotspot (which means we're on it now)
	if (hotspotHighlight.size() > 0 && currentDragState == DRAG_NONE && !(this->isLocked())) {
		// Start dragging a new wire:
		//gateList[hotspotGate]->select();
		_MSGGUI("gateID: %d (%s)\n", hotspotGate, hotspotHighlight.c_str())	//@@@@
		unselectAllGates();
		unselectAllWires();
		handled = true; // Don't worry about checking other events in this proc
		currentDragState = DRAG_CONNECT;
		currentConnectionSource.isGate = true;
		currentConnectionSource.objectID = hotspotGate;
		currentConnectionSource.connection = hotspotHighlight;
	}

	// Now check gate collisions
	hit = hitThings.begin();
	while (hit != hitThings.end() && !handled) {
		if ((*hit)->getType() == COLL_GATE) {
			guiGate* hitGate = ((guiGate*)(*hit));
			bool wasSelected = hitGate->isSelected();
			if ((event.ShiftDown() || event.ControlDown()) && wasSelected) {
				hitGate->unselect(); // Remove gate from selection
			}
			else if ((event.ShiftDown() || event.ControlDown()) && !wasSelected) {
				hitGate->select(); // Add gate to selection
			}
			else if (!(event.ShiftDown() || event.ControlDown()) && !wasSelected) {
				// Begin new selection group
				unselectAllGates();
				unselectAllWires();
				hitGate->select();
			}
			if (!(event.ShiftDown() || event.ControlDown()) && !(this->isLocked())) {
				_MSGGUI("gateID: %d (%s)\n", hitGate->getID(), hitGate->getLibraryGateName().c_str())	//@@@@
				currentDragState = DRAG_SELECTION; // Start dragging
			}
			handled = true;
		}
		hit++;
	}
	// If I am not in a selection group and I haven't handled a selection then unselect everything
	if (!handled && !((event.ShiftDown() || event.ControlDown()))) {
		unselectAllGates();
		unselectAllWires();
	}

	if (!handled) {
		// Otherwise initialize drag select
		currentDragState = DRAG_SELECT;
	}

	// Show the updates
	Refresh();

	// clean up the selected gates vector and saved premove state
	selectedGates.clear();
	preMove.clear();
	saveMove = false;
	unordered_map < unsigned long, guiGate* >::iterator thisGate = gateList.begin();
	while (thisGate != gateList.end()) {
		if ((thisGate->second)->isSelected()) {
			// Push back the gate's id, xy pos, angle, and select flag
			preMove.push_back(GateState((thisGate->first), 0, 0, (thisGate->second)->isSelected()));
			(thisGate->second)->getGLcoords(preMove[preMove.size() - 1].x, preMove[preMove.size() - 1].y);
			selectedGates.push_back((thisGate->first));
		}
		thisGate++;
	}
	// clean up the selected wires vector
	selectedWires.clear();
	preMoveWire.clear();
	unordered_map < unsigned long, guiWire* >::iterator thisWire = wireList.begin();
	while (thisWire != wireList.end()) {
		if (thisWire->second != nullptr) {
			if ((thisWire->second)->isSelected()) {
				// Push back the wire's id
				preMoveWire.push_back(WireState((thisWire->first), (thisWire->second)->getCenter(), (thisWire->second)->getSegmentMap()));
				selectedWires.push_back((thisWire->first));
			}
		}
		thisWire++;
	}
}

void GUICanvas::mouseRightDown(wxMouseEvent& event) {
	GLPoint2f m = getMouseCoords();
	vector < unsigned long >::iterator sGate;

	if (isWithinPaste || (currentDragState != DRAG_NONE)) return; // Left mouse up is the next event we are looking for

	// Update the mouse collision object
	klsBBox mBox;
	float delta = MOUSE_HOVER_DELTA * getZoom();
	mBox.addPoint( m );
	mBox.extendTop( delta );
	mBox.extendBottom( delta );
	mBox.extendLeft( delta );
	mBox.extendRight( delta );
	mouse->setBBox( mBox );
	
	// Do a collision detection on all first-level objects.
	// The map collisionChecker.overlaps now contains
	// all of the objects involved in any collisions.
	collisionChecker.update();

	// Go ahead and remove all selection since this is the right mouse button
	unselectAllGates();
	unselectAllWires();

	// If locked then we have nothing else to do
	if ( this->isLocked() ) return;

	// do we have a highlighted hotspot (which means we're on it now)
	if (hotspotHighlight.size() > 0) {
		// If the hotspot is connected then we disconnect it and generate a command
		if (gateList[hotspotGate]->isConnected(hotspotHighlight)) {
			// disconnect this wire
			if (gateList[hotspotGate]->getConnection(hotspotHighlight)->numConnections() > 2)
				gCircuit->GetCommandProcessor()->Submit( (wxCommand*)(new cmdDisconnectWire( gCircuit, gateList[hotspotGate]->getConnection(hotspotHighlight)->getID(), hotspotGate, hotspotHighlight )) );
			else
				gCircuit->GetCommandProcessor()->Submit( (wxCommand*)(new cmdDeleteWire( gCircuit, this, gateList[hotspotGate]->getConnection(hotspotHighlight)->getID() )) );
		}
		currentDragState = DRAG_NONE;
	} else if (currentDragState == DRAG_NONE) {
		// Not on a hotspot, so check if it's on a gate:
		// Loop through all objects hit by the mouse
		CollisionGroup hitThings = mouse->getOverlaps();
		CollisionGroup::iterator hit = hitThings.begin();
		unselectAllGates();
		unselectAllWires();
		while( hit != hitThings.end()) {
			if ((*hit)->getType() == COLL_GATE) {
				guiGate* hitGate = ((guiGate*)(*hit));
				if (event.AltDown()) {
#ifdef _MSG_
					hitGate->doPropsDialog();
#endif
				}
				else {
					// BEGIN WORKAROUND
					//	Gates that have connections cannot be rotated without sacrificing wire sanity
					map < string, GLPoint2f > gateHotspots = hitGate->getHotspotList();
					map < string, GLPoint2f >::iterator ghsWalk = gateHotspots.begin();
					bool gateConnected = false;
					while ( ghsWalk !=  gateHotspots.end() ) {
						if ( hitGate->isConnected( ghsWalk->first ) ) {
							gateConnected = true;
							break;
						}
						ghsWalk++;
					}
					if ( gateConnected ) { hit++; continue; }
					// END WORKAROUND

					map < string, string > newParams(*(hitGate->getAllGUIParams()));
					if (!(event.ShiftDown() || event.ControlDown())) {
						istringstream issAngle(newParams["angle"]);
						GLfloat angle;
						issAngle >> angle;
						angle += 90.0;
						if (angle >= 360.0) angle -= 360.0;
						ostringstream ossAngle;
						ossAngle << angle;
						newParams["angle"] = ossAngle.str();
					} else {
						// Pedro Casanova (casanova@ujaen.es) 2020/04-12
						// Shift or Control and mouseRightDown to change "mirror" GUI param
						if (newParams["mirror"] == "true")
							newParams["mirror"] = "false";
						else
							newParams["mirror"] = "true";
					}
					gCircuit->GetCommandProcessor()->Submit((wxCommand*)(new cmdSetParams(gCircuit, hitGate->getID(), paramSet(&newParams, NULL))));
				}

			}
			hit++;
		}
	}
	Refresh();	
}

void GUICanvas::OnMouseMove( GLdouble glX, GLdouble glY, bool ShiftDown, bool CtrlDown ) {
	// Keep a flag for whether things have changed.  If nothing changes, then no render is necessary.
	bool shouldRender = false;
	if (wxGetApp().appSystemTime.Time() > wxGetApp().appSettings.refreshRate) {
		wxGetApp().appSystemTime.Pause();
		if (gCircuit->panic) return;
		wxCriticalSectionLocker locker(wxGetApp().m_critsect);
		while (wxGetApp().mexMessages.TryLock() == wxMUTEX_BUSY) wxYield();
		while (wxGetApp().dLOGICtoGUI.size() > 0) {
			gCircuit->parseMessage(wxGetApp().dLOGICtoGUI.front());
			wxGetApp().dLOGICtoGUI.pop_front();
		}
		wxGetApp().mexMessages.Unlock();
		if (gCircuit->panic) return;
		// Do function of number of milliseconds that passed since last step
		gCircuit->lastTime = wxGetApp().appSystemTime.Time();
		gCircuit->lastTimeMod = wxGetApp().timeStepMod;
		gCircuit->lastNumSteps = wxGetApp().appSystemTime.Time() / wxGetApp().timeStepMod;
		gCircuit->sendMessageToCore(klsMessage::Message(klsMessage::MT_STEPSIM, new klsMessage::Message_STEPSIM(wxGetApp().appSystemTime.Time() / wxGetApp().timeStepMod)));
		gCircuit->setSimulate(false);
		wxGetApp().appSystemTime.Start(wxGetApp().appSystemTime.Time() % wxGetApp().timeStepMod);
		shouldRender = true;
	}

	GLPoint2f m = getMouseCoords();
	GLPoint2f dStart = getDragStartCoords(BUTTON_LEFT);
	GLPoint2f diff( m.x - dStart.x, m.y - dStart.y ); // What is the difference between start and now

	GLPoint2f mSnap = getSnappedPoint( m ); // Work with a snapped mouse coord
	GLPoint2f dStartSnap = getSnappedPoint( dStart );
	GLPoint2f diffSnap( mSnap.x - dStartSnap.x, mSnap.y - dStartSnap.y );

	// Update the mouse as a collision object:
	klsBBox mBox;
	float delta = MOUSE_HOVER_DELTA * getZoom();
	mBox.addPoint( m );
	mBox.extendTop( delta );
	mBox.extendBottom( delta );
	mBox.extendLeft( delta );
	mBox.extendRight( delta );
	mouse->setBBox( mBox );

	klsBBox smBox;
	smBox.addPoint( mSnap );
	snapMouse->setBBox( smBox );

	// Update the drag select box coordinates:
	klsBBox dBox;
	dBox.addPoint( dStart );
	dBox.addPoint( m );
	dragselectbox->setBBox( dBox );
	
	if ( this->isLocked() ) return;
	
	// Do a collision detection on all first-level objects.
	// The map collisionChecker.overlaps now contains
	// all of the objects involved in any collisions.
	collisionChecker.update();
	
	// Update a newly-dragged gate's position
	if (currentDragState == DRAG_NEWGATE) {
		shouldRender = true;
		newDragGate->setGLcoords(mSnap.x, mSnap.y);
	}
	
	// If the hotspot hover is on, make it clear
	if (hotspotHighlight.size() > 0) shouldRender = true;
	hotspotHighlight = "";

	// If necessary, save the move being done.
	if (preMove.size() > 0 && currentDragState != DRAG_CONNECT && (diffSnap.x != 0 || diffSnap.y != 0)) saveMove = true;

	if (currentDragState == DRAG_SELECTION) {
		// Move all gates that are selected in the preMove vector:
		for (unsigned int i = 0; i < preMoveWire.size(); i++) wireList[preMoveWire[i].id]->move(preMoveWire[i].point, diffSnap);
		for (unsigned int i = 0; i < preMove.size(); i++) gateList[preMove[i].id]->setGLcoords(preMove[i].x+diffSnap.x, preMove[i].y+diffSnap.y);
	} else if (currentDragState == DRAG_WIRESEG) {
		wireList[wireHoverID]->updateSegDrag(snapMouse);
	}

	// Generate a new list of potential connections
	potentialConnectionHotspots.clear();
	// Reset wire hover flag
	if (drawWireHover) shouldRender = true;
	drawWireHover = false;

	CollisionGroup hitThings = mouse->getOverlaps();
	CollisionGroup::iterator hit = hitThings.begin();
	while( hit != hitThings.end()) {
		if ((*hit)->getType() == COLL_GATE) {
			guiGate* hitGate = ((guiGate*)(*hit));
			
			// Update the hotspot hover variables:
			if( hotspotHighlight.size() == 0 ) {				
				if (currentDragState != DRAG_NEWGATE || hitGate->getID() != newDragGate->getID()) hotspotHighlight = hitGate->checkHotspots( m.x, m.y, HOTSPOT_SCREEN_DELTA * getZoom() );
				if( hotspotHighlight.size() > 0 ) {
					if (currentDragState != DRAG_NEWGATE || hitGate->getID() != newDragGate->getID()) hotspotGate = hitGate->getID();
					shouldRender = true;
				}
			}
			
		}
		if ((*hit)->getType() == COLL_WIRE && !drawWireHover && currentDragState != DRAG_WIRESEG) { // Check for wire hover
			guiWire* hitWire = ((guiWire*)(*hit));
			drawWireHover = hitWire->hover( m.x, m.y, WIRE_HOVER_SCREEN_DELTA * getZoom() );
			wireHoverID = hitWire->getID();
			if (drawWireHover) shouldRender = true;
		}
		hit++;
	}

	if (currentDragState == DRAG_SELECT) { // Check for items within drag select box
		unselectAllGates();
		unselectAllWires();
		// Other items may have been selected before if we're using shift/control
		for (unsigned int i = 0; i < preMove.size(); i++) {
			gateList[preMove[i].id]->select();
		}
		for (unsigned int i = 0; i < preMoveWire.size(); i++) {
			wireList[preMoveWire[i].id]->select();
		}
		// Now check the collision box for dragselects
		CollisionGroup selThings = dragselectbox->getOverlaps();
		hit = selThings.begin();
		while( hit != selThings.end()) {
			if ((*hit)->getType() == COLL_GATE) {
				guiGate* hitGate = ((guiGate*)(*hit));
				if (dBox.contains((*hit)->getBBox())) hitGate->select();
			}
			if ((*hit)->getType() == COLL_WIRE) {
				guiWire* hitWire = ((guiWire*)(*hit));
				if (dBox.contains((*hit)->getBBox())) hitWire->select();
			}
			hit++;
		}
	}
	
	// Check potential hotspot connections (on gate/gate collisions)
	CollisionGroup ovrList = collisionChecker.overlaps[COLL_GATE];
	CollisionGroup::iterator obj = ovrList.begin();
	while( obj != ovrList.end() ) {
		CollisionGroup hitThings = (*obj)->getOverlaps();
		CollisionGroup::iterator hit = hitThings.begin();
		while( hit != hitThings.end() ) {
			// Only check gate collisions
			if ((*hit)->getType() != COLL_GATE) { hit++; continue; };
			// obj and hit are two overlapping gates
			//  get overlapping hotspots of obj in another group
			CollisionGroup hotspotOverlaps = (*obj)->checkSubsToSubs(*hit);
			CollisionGroup::iterator hotspotCollide = hotspotOverlaps.begin();
			while (hotspotCollide != hotspotOverlaps.end()) {
				// hotspotCollide is in obj; hsWalk is in hit
				CollisionGroup hshits = (*hotspotCollide)->getOverlaps();
				CollisionGroup::iterator hsWalk = hshits.begin();
				while (hsWalk != hshits.end()) {
					if ( !(((guiGate*)(*obj))->isConnected(((gateHotspot*)(*hotspotCollide))->name)) && !(((guiGate*)(*hit))->isConnected(((gateHotspot*)(*hsWalk))->name)))
						potentialConnectionHotspots.push_back( ((gateHotspot*)(*hotspotCollide))->getLocation() );
					hsWalk++;
				}
				hotspotCollide++;
			}
			hit++;
		}
		obj++;
	}

	if (currentDragState == DRAG_SELECTION || currentDragState == DRAG_SELECT || currentDragState == DRAG_CONNECT || currentDragState == DRAG_WIRESEG) shouldRender = true;
	
	// Only render if necessary
	//	REFRESH DOESN'T SEEM TO UPDATE IN TIME FOR MOUSE MOVE
	if (shouldRender) {
		klsGLCanvasRender();
		// Show the new buffer:
		glFlush();
		SwapBuffers();
	}
	
	// clean up the selected gates vector
	selectedGates.clear();
	unordered_map < unsigned long, guiGate* >::iterator thisGate = gateList.begin();
	while (thisGate != gateList.end()) {
		if ((thisGate->second)->isSelected()) selectedGates.push_back((thisGate->first));
		thisGate++;
	}
	// clean up the selected wires vector
	selectedWires.clear();
	unordered_map < unsigned long, guiWire* >::iterator thisWire = wireList.begin();
	while (thisWire != wireList.end()) {
		if (thisWire->second != nullptr) {
			if ((thisWire->second)->isSelected()) selectedWires.push_back((thisWire->first));
		}
		thisWire++;
	}
}

void GUICanvas::OnMouseUp(wxMouseEvent& event) {
	GLPoint2f m = getMouseCoords();
	SetCursor(wxCursor(wxCURSOR_ARROW));
	unordered_map < unsigned long, guiGate* >::iterator thisGate;
	cmdMoveSelection* movecommand = NULL;
	cmdCreateGate* creategatecommand = NULL;

	// Update the drag select box coordinates for wire source detection:
	klsBBox dBox;
	float delta = HOTSPOT_SCREEN_DELTA * getZoom();
	dBox.addPoint( getDragStartCoords( BUTTON_LEFT ) );
	dBox.extendTop( delta );
	dBox.extendBottom( delta );
	dBox.extendLeft( delta );
	dBox.extendRight( delta );
	dragselectbox->setBBox( dBox );

	// If moving a selection then save the move as a command
	if (saveMove && currentDragState == DRAG_SELECTION) {
		float gX, gY;
		if (preMove.size() > 0) {
			gateList[preMove[0].id]->getGLcoords(gX, gY);
			movecommand = new cmdMoveSelection( gCircuit, preMove, preMoveWire, preMove[0].x, preMove[0].y, gX, gY );
			for (unsigned int i = 0; i < preMove.size(); i++) gateList[preMove[i].id]->updateConnectionMerges();
			if (!isWithinPaste) gCircuit->GetCommandProcessor()->Submit( (wxCommand*)movecommand );
			if (!isWithinPaste) movecommand->Undo();
		}
		if (preMove.size() > 1) preMove.clear();
	}

	// Check for single selection out of group
	if (preMove.size() > 0) {
		float gX, gY;
		gateList[preMove[0].id]->getGLcoords(gX, gY);
		if (gX == preMove[0].x && gY == preMove[0].y && !((event.ShiftDown()||event.ControlDown()))) { // no move
			CollisionGroup hitThings = mouse->getOverlaps();
			CollisionGroup::iterator hit = hitThings.begin();
			while( hit != hitThings.end() ) {
				if ((*hit)->getType() == COLL_GATE) {
					guiGate* hitGate = ((guiGate*)(*hit));
					unselectAllGates();
					unselectAllWires();
					preMove.clear();
					preMove.push_back(GateState(hitGate->getID(), 0, 0, true));
					hitGate->select();
					saveMove = false;
					break;
				}
				hit++;
			}		
		}
	}

	if (currentDragState == DRAG_WIRESEG) {
		wireList[wireHoverID]->endSegDrag();
		wireList[wireHoverID]->select();
		gCircuit->GetCommandProcessor()->Submit( new cmdWireSegDrag( gCircuit, this, wireHoverID ) );
	}

	// If dragging a new gate then 
	if (currentDragState == DRAG_NEWGATE) {
		// Pedro Casanova (casanova@ujaen.es) 2021/01-03
		// Midified to create dynamics gates
		if (newDragGate->getLibraryGateName().substr(0, 3) == "%%_") {
			float nx, ny;
			newDragGate->getGLcoords(nx, ny);
			while (true) {
				newDragGate->setGUIParam("DYNAMIC_GATE", "false");		// Needed to detect "Cancel" pressed in dialog
				newDragGate->doParamsDialog(gCircuit, gCircuit->GetCommandProcessor());
				if (newDragGate->getGUIParam("DYNAMIC_GATE") != "true")
					break;
				else 
				{
					newDragGate->setGLcoords(nx, ny);
					if (addDynamicGate(newDragGate))
						break;
				}
			}
		}
		else {
			int newGID = gCircuit->getNextAvailableGateID();
			float nx, ny;
			newDragGate->getGLcoords(nx, ny);
			creategatecommand = new cmdCreateGate(this, gCircuit, newGID, newDragGate->getLibraryGateName(), nx, ny);
			gCircuit->GetCommandProcessor()->Submit((wxCommand*)creategatecommand);
			gateList[newGID]->select();
			selectedGates.push_back(newGID);
		}

		gCircuit->getGates()->erase(newDragGate->getID());

		collisionChecker.removeObject(newDragGate);
		// Only now do a collision detection on all first-level objects since the new gate is in.
		// The map collisionChecker.overlaps now contains
		// all of the objects involved in any collisions.
		collisionChecker.update();
		delete newDragGate;
	}
	else {
		// Do a collision detection on all first-level objects.
		// The map collisionChecker.overlaps now contains
		// all of the objects involved in any collisions.
		collisionChecker.update();
		if ((currentDragState == DRAG_NONE || currentDragState == DRAG_SELECTION) && preMove.size() == 1) {
			// Loop through all objects hit by the mouse
			//	Favor wires over gates
			/*CollisionGroup hitThings = mouse->getOverlaps();
			CollisionGroup::iterator hit = hitThings.begin();
			while( hit != hitThings.end() && !handled ) {
				if ((*hit)->getType() == COLL_GATE) {
					guiGate* hitGate = ((guiGate*)(*hit));*/					
					guiGate* hitGate = gateList[preMove[0].id];
					// Pedro Casanova (casanova@ujaen.es) 2020/04-12
					// To permit single click in lock mode
					if ((!(event.ShiftDown() || event.ControlDown()) && (event.LeftUp() && currentDragState == DRAG_SELECTION)) || ((event.LeftUp() && this->isLocked()) || event.LeftDClick())) {
						// Check for toggle switch
						float x, y;
						hitGate->getGLcoords(x,y);
						bool handled = false;
						if (!saveMove) {
							klsMessage::Message_SET_GATE_PARAM* clickHandleGate = hitGate->checkClick( m.x, m.y );
							if (clickHandleGate != NULL) {
								gCircuit->sendMessageToCore(klsMessage::Message(klsMessage::MT_SET_GATE_PARAM, clickHandleGate));
								handled = true;
							}
						}
						if (event.LeftDClick() && !handled) {
							if (!(event.ShiftDown() || event.ControlDown())) {
								hitGate->doParamsDialog(gCircuit, gCircuit->GetCommandProcessor());
							}
							currentDragState = DRAG_NONE;
							// setparams command will handle oscope update
							handled = true;
						}
					}
/*				}
				hit++;
			}*/
		}

		// If we are dragging something...
		if (currentDragState == DRAG_CONNECT) {
			wxCommand *command = nullptr;
			IDType wireID1, wireID2;
			bool twoWires = false;
			
			// Pedro Casanova (casanova@ujaen.es) 2020/04-12
			// We can drag from gates and from wires
			// Oh yeah, we only drag from gates... FALSE

			// Source is hotspot and target is hotspot:
			//		if source is connected and target is connected, merge
			//		if source is connected or target is connected, connect
			//		if neither is connected then create a wire
			// Source is hotspot and target is wire:
			//		if source is connected then merge
			//		if source is not connected then connect
			// Source is wire and target is hotspot:
			//		if target is connected then merge
			//		if target is not connected then connect
			// Source is wire and target is wire:
			//		merge


			if (currentConnectionSource.isGate) {
				// Drag from gate				
				if (hotspotHighlight.size() > 0) {
					// Target is a gate...
					guiGate *gate1 = gateList[currentConnectionSource.objectID];
					guiGate *gate2 = gateList[hotspotGate];
					if (!gate1->isConnected(currentConnectionSource.connection) || !gate2->isConnected(hotspotHighlight)) {
						command = createGateConnectionCommand(
							currentConnectionSource.objectID,
							currentConnectionSource.connection,
							hotspotGate,
							hotspotHighlight);
					} else {
						// Connection between two diferents wires
						twoWires = true;
						wireID1 = gate1->getConnection(currentConnectionSource.connection)->getID();
						wireID2 = gate2->getConnection(hotspotHighlight)->getID();
					}
				}
				else if (drawWireHover) {
					// Target is a wire...
					guiGate *gate = gateList[currentConnectionSource.objectID];
					if (!gate->isConnected(currentConnectionSource.connection)) {
						command = createGateWireConnectionCommand(
							currentConnectionSource.objectID,
							currentConnectionSource.connection,
							wireHoverID);
					} else {
						// Connection between two diferents wires
						twoWires = true;
						wireID1 = gate->getConnection(currentConnectionSource.connection)->getID();
						wireID2 = (IDType)wireHoverID;
					}
				} 
			} else {
				// Drag from wire								
				if (hotspotHighlight.size() > 0) {
					// Target is a gate...
					guiGate *gate = gateList[hotspotGate];
					if (!gate->isConnected(hotspotHighlight)) {
						command = createGateWireConnectionCommand(
							hotspotGate,
							hotspotHighlight,
							currentConnectionSource.objectID);
					}
					else {
						// Connection between two diferents wires
						twoWires = true;
						wireID1 = (IDType)currentConnectionSource.objectID;
						wireID2 = gate->getConnection(hotspotHighlight)->getID();						
					}

				}
				else if (drawWireHover) {
					// Target is a wire...
					if (currentConnectionSource.objectID != wireHoverID) {
						// Connection between two diferents wires
						twoWires = true;
						wireID1 = (IDType)currentConnectionSource.objectID;
						wireID2 = (IDType)wireHoverID;
					}
				}
			}
			if (twoWires) {
				// Pedro Casanova (casanova@ujaen.es) 2020/04-12
				// Connection between two diferents wires
				command = createWireConnectionCommand(wireID1, wireID2);
			}
			unselectAllWires();
			unselectAllGates();

			if (command != nullptr) {
				gCircuit->GetCommandProcessor()->Submit(command);
			}
		}
		collisionChecker.update();
	}

	if ((currentDragState == DRAG_NEWGATE || currentDragState == DRAG_SELECTION) && (potentialConnectionHotspots.size() > 0)) {
		// Check potential hotspot connections (on gate/gate collisions)
		CollisionGroup ovrList = collisionChecker.overlaps[COLL_GATE];
		CollisionGroup::iterator obj = ovrList.begin();
		while( obj != ovrList.end() ) {
			CollisionGroup hitThings = (*obj)->getOverlaps();
			CollisionGroup::iterator hit = hitThings.begin();
			while( hit != hitThings.end() ) {
				// Only check gate collisions
				if ((*hit)->getType() != COLL_GATE) { hit++; continue; };
				// obj and hit are two overlapping gates
				//  get overlapping hotspots of obj in another group
				CollisionGroup hotspotOverlaps = (*obj)->checkSubsToSubs(*hit);
				CollisionGroup::iterator hotspotCollide = hotspotOverlaps.begin();
				while (hotspotCollide != hotspotOverlaps.end()) {
					// hotspotCollide is in obj; hsWalk is in hit
					CollisionGroup hshits = (*hotspotCollide)->getOverlaps();
					CollisionGroup::iterator hsWalk = hshits.begin();
					while (hsWalk != hshits.end()) {
						if (!(((guiGate*)(*obj))->isConnected(((gateHotspot*)(*hotspotCollide))->name)) && !(((guiGate*)(*hit))->isConnected(((gateHotspot*)(*hsWalk))->name)) &&
							(((guiGate*)(*obj))->isSelected() || ((guiGate*)(*hit))->isSelected())) {
							cmdCreateWire* createwire = (cmdCreateWire *)createGateConnectionCommand(
								((guiGate*)(*obj))->getID(), ((gateHotspot*)(*hotspotCollide))->name,
								((guiGate*)(*hit))->getID(), ((gateHotspot*)(*hsWalk))->name);

							if (createwire != nullptr) {
								createwire->Do();
								//collisionChecker.update();
								if (currentDragState == DRAG_SELECTION) {
									if (movecommand == NULL) {
										movecommand = new cmdMoveSelection(gCircuit, preMove, preMoveWire, 0, 0, 0, 0);
										if (!isWithinPaste) gCircuit->GetCommandProcessor()->Submit((wxCommand*)movecommand);
									}
									movecommand->getConnections()->push_back(createwire);
								}
								else if (currentDragState == DRAG_NEWGATE) creategatecommand->getConnections()->push_back(createwire);
								else delete createwire;
							}
						}
						hsWalk++;
					}
					hotspotCollide++;
				}
				hit++;
			}
			obj++;
		}
	}

	// Drop a paste block with the proper move coords
	if (isWithinPaste) {
		pasteCommand->addCommand( movecommand );
		gCircuit->GetCommandProcessor()->Submit( pasteCommand );
		isWithinPaste = false;
		autoScrollEnable(); // Re-enable auto scrolling
	}
	
	currentDragState = DRAG_NONE;
	
	Update();
}

void GUICanvas::OnMouseEnter(wxMouseEvent& event) {
	GLPoint2f m = getMouseCoords();

	// Do a collision detection on all first-level objects.
	// The map collisionChecker.overlaps now contains
	// all of the objects involved in any collisions.
	//collisionChecker.update();

	wxGetApp().showDragImage = false;
	if (event.LeftIsDown() && wxGetApp().newGateToDrag.size() > 0 && currentDragState == DRAG_NONE && !(this->isLocked())) {
		newDragGate = gCircuit->createGate(wxGetApp().newGateToDrag, -1);
		if (newDragGate == NULL) { wxGetApp().newGateToDrag = ""; return; }
		newDragGate->setGLcoords(m.x, m.y);
		//wxGetApp().logfile << m.x << " " << (panY-(y*viewZoom)) << endl << flush;
		currentDragState = DRAG_NEWGATE;
		wxGetApp().newGateToDrag = "";
		beginDrag( BUTTON_LEFT );
		unselectAllGates();
		newDragGate->select();
		collisionChecker.addObject( newDragGate );
	} else wxGetApp().newGateToDrag = "";
}


void GUICanvas::OnKeyDown(wxKeyEvent& event) {
	switch (event.GetKeyCode()) {
	case WXK_DELETE:
		if (currentDragState == DRAG_NONE && !(this->isLocked())) deleteSelection();
		break;
	case WXK_ESCAPE:
		unselectAllGates();
		unselectAllWires();
		if (currentDragState == DRAG_NEWGATE) {
			gCircuit->getGates()->erase(newDragGate->getID());
			collisionChecker.removeObject( newDragGate );
			delete newDragGate;
			collisionChecker.update();
		} 
		// Pedro Casanova(casanova@ujaen.es) 2021/01-03
		// This causes many problems	
		/*else {		
			if (preMove.size() > 0) {
				saveMove = false;
				for (unsigned int i = 0; i < preMove.size(); i++) {
					if (gateList.find(preMove[i].id) == gateList.end()) continue;
					gateList[preMove[i].id]->setGLcoords(preMove[i].x, preMove[i].y);
					if (preMove[i].selected) gateList[preMove[i].id]->select();
				}
				preMove.clear();
			}
			if (preMoveWire.size() > 0) {
				for (unsigned int i = 0; i < preMoveWire.size(); i++) {
					if (wireList.find(preMoveWire[i].id) == wireList.end()) continue;
					wireList[preMoveWire[i].id]->setSegmentMap(preMoveWire[i].oldWireTree);
					wireList[preMoveWire[i].id]->select();
				}
			}
		}*/
		currentDragState = DRAG_NONE;
		endDrag(BUTTON_LEFT);
		Refresh();
		break;
	case WXK_LEFT:
	case WXK_NUMPAD_LEFT:
		translatePan(-PAN_STEP * getZoom(), 0.0);
		break;
	case WXK_RIGHT:
	case WXK_NUMPAD_RIGHT:
		translatePan(+PAN_STEP * getZoom(), 0.0);
		break;
	case WXK_UP:
	case WXK_NUMPAD_UP:
		translatePan(0.0, PAN_STEP * getZoom());
		break;
	case WXK_DOWN:
	case WXK_NUMPAD_DOWN:
		translatePan(0.0, -PAN_STEP * getZoom());
		break;
	case 43: // + key on top row
	case WXK_NUMPAD_ADD:
		zoomIn();
		break;
	case 45: // - key on top row
	case WXK_NUMPAD_SUBTRACT:
		zoomOut();
		break;
	case WXK_SPACE:
		setZoomAll();
		break;
#ifdef _MSG_
	// Pedro Casanova(casanova@ujaen.es) 2021/01-03
	// Print info
	case WXK_TAB:
		this->printLists();
		break;
	case WXK_BACK:
		gCircuit->printState();
		break;
#endif
	}
}

void GUICanvas::deleteSelection() {
	// whatever is in the selected vectors goes
	if (selectedWires.size() > 0 || selectedGates.size() > 0) gCircuit->GetCommandProcessor()->Submit( (wxCommand*)(new cmdDeleteSelection( gCircuit, this, selectedGates, selectedWires )) );
	selectedWires.clear();
	selectedGates.clear();
	preMove.clear();
	saveMove = false;

	// Do a collision detection on all first-level objects.
	// The map collisionChecker.overlaps now contains
	// all of the objects involved in any collisions.
	Update();
}

void GUICanvas::unselectAllGates() {
	unordered_map < unsigned long, guiGate* >::iterator thisGate = gateList.begin();
	while (thisGate != gateList.end()) {
		(thisGate->second)->unselect();
		thisGate++;
	}
}

void GUICanvas::unselectAllWires() {
	unordered_map < unsigned long, guiWire* >::iterator thisWire = wireList.begin();
	while (thisWire != wireList.end()) {
		if (thisWire->second != nullptr) {
			(thisWire->second)->unselect();
		}
		thisWire++;
	}
}	

void GUICanvas::copyBlockToClipboard () {
	klsClipboard myClipboard;
	// Ship the selected gates and wires out to the clipboard
	myClipboard.copyBlock( gCircuit, this, selectedGates, selectedWires );
}

void GUICanvas::pasteBlockFromClipboard () {
	if (this->isLocked()) return;
	
	klsClipboard myClipboard;
	pasteCommand = myClipboard.pasteBlock( gCircuit, this );
	if (pasteCommand == NULL) return;
	currentDragState = DRAG_SELECTION; // drag until dropped
	isWithinPaste = true;
	saveMove = true;
	
	// clean up the selected gates vector
	selectedGates.clear();
	preMove.clear();
	unordered_map< unsigned long, guiGate* >::iterator thisGate = gateList.begin();
	unsigned long snapToGateID = 0;
	GLPoint2f gatecoord;
	// paste only to snapped point
	GLPoint2f mc = getSnappedPoint(getMouseCoords());
	GLPoint2f minPoint;
	bool ref = false;
	// Find top-left-most point
	while (thisGate != gateList.end()) {
		GLPoint2f temp;
		if ((thisGate->second)->isSelected()) {
			(thisGate->second)->getGLcoords(temp.x, temp.y);
			if (temp.x < minPoint.x || !ref) minPoint.x = temp.x;
			if (temp.y > minPoint.y || !ref) minPoint.y = temp.y;
			ref = true;
		}
		thisGate++;
	}
	ref = false;
	// Try to drag by the top-left-most gate
	double minMagnitude = 0.0;
	thisGate = gateList.begin();
	while (thisGate != gateList.end()) {
		GLPoint2f temp;
		if ((thisGate->second)->isSelected()) {
			if (ref) {
				(thisGate->second)->getGLcoords(temp.x, temp.y);
				float diffx = gatecoord.x - minPoint.x, diffy = gatecoord.y - minPoint.y;
				double newMag = (diffx * diffx) + (diffy * diffy);
				if (newMag < minMagnitude) {
					minMagnitude = newMag;
					gatecoord = temp;
					snapToGateID = (thisGate->first);
				}
			} else {
				(thisGate->second)->getGLcoords(gatecoord.x, gatecoord.y);
				float diffx = gatecoord.x - minPoint.x, diffy = gatecoord.y - minPoint.y;
				minMagnitude = (diffx * diffx) + (diffy * diffy);
				snapToGateID = (thisGate->first);
				ref = true;
			}
		}
		thisGate++;
	}

	// What is the difference between that gate and the mouse coords
	GLPoint2f diff( mc.x-gatecoord.x, mc.y-gatecoord.y );
	// Shift all the gates and track their differences by command
	thisGate = gateList.begin();
	while (thisGate != gateList.end()) {
		if ((thisGate->second)->isSelected()) {
			preMove.push_back(GateState((thisGate->first), 0, 0, (thisGate->second)->isSelected()));
//			(thisGate->second)->translateGLcoords(diff.x, diff.y);
			(thisGate->second)->getGLcoords(preMove[preMove.size()-1].x, preMove[preMove.size()-1].y);
			preMove[preMove.size()-1].x += diff.x; preMove[preMove.size()-1].y += diff.y;
			cmdMoveGate* mgcmd = new cmdMoveGate(gCircuit, (thisGate->first), preMove[preMove.size()-1].x-diff.x, preMove[preMove.size()-1].y-diff.y, preMove[preMove.size()-1].x, preMove[preMove.size()-1].y, true);
			mgcmd->Do();
			pasteCommand->addCommand( mgcmd );
			selectedGates.push_back((thisGate->first));
		}
		thisGate++;
	}
	// clean up the selected wires vector
	selectedWires.clear();
	preMoveWire.clear();
	unordered_map< unsigned long, guiWire* >::iterator thisWire = wireList.begin();
	while (thisWire != wireList.end()) {
		if (thisWire->second != nullptr) {
			if ((thisWire->second)->isSelected()) {
				// Push back the wire's id and set up a premove state
				cmdMoveWire* movewire = new cmdMoveWire(gCircuit, (thisWire->first), (thisWire->second)->getSegmentMap(), diff);
				movewire->Do();
				pasteCommand->addCommand(movewire);
				preMoveWire.push_back(WireState((thisWire->first), (thisWire->second)->getCenter(), (thisWire->second)->getSegmentMap()));
				selectedWires.push_back((thisWire->first));
			}
		}
		thisWire++;
	}

	autoScrollDisable();
	beginDrag(BUTTON_LEFT);
	
	Update();
}


// Zoom the canvas to fit all items within it:
void GUICanvas::setZoomAll( void ) {
// TODO: BUG this function sometimes hangs the program.
	klsBBox zoomBox;

	// Add all the gates into the zoom all box:
	unordered_map< unsigned long, guiGate* >::iterator gateWalk = gateList.begin();
	while( gateWalk != gateList.end() ) {
		zoomBox.addBBox( (gateWalk->second)->getBBox() );
		gateWalk++;
	}

	// Add all the wires into the zoom all box:
	unordered_map< unsigned long, guiWire* >::iterator wireWalk = wireList.begin();
	while( wireWalk != wireList.end() ) {
		if (wireWalk->second != nullptr) {
			zoomBox.addBBox((wireWalk->second)->getBBox());
		}
		wireWalk++;
	}
	
	// Make sure to not have a dumb zoom factor on an empty canvas:
	if( gateList.empty() ) {
		zoomBox.addPoint(GLPoint2f(0, 0));
	}
	
	// Put some margin around the zoom box:
	zoomBox.extendTop( ZOOM_ALL_MARGIN );
	zoomBox.extendBottom( ZOOM_ALL_MARGIN );
	zoomBox.extendLeft( ZOOM_ALL_MARGIN );
	zoomBox.extendRight( ZOOM_ALL_MARGIN );

	// Zoom to the zoom-all box:
	setViewport( zoomBox.getTopLeft(), zoomBox.getBottomRight() );
}


// print page contents
void GUICanvas::printLists() {
	wxGetApp().logfile << "printing page lists" << endl << flush;
	unordered_map< unsigned long, guiGate* >::iterator thisGate = gateList.begin();
	while (thisGate != gateList.end()) {
		float x, y;
		(thisGate->second)->getGLcoords(x, y);
		wxGetApp().logfile << " gate " << thisGate->first << " type " << (thisGate->second)->getLibraryGateName() << " at " << x << "," << y << endl << flush;
		thisGate++;
	}
	unordered_map< unsigned long, guiWire* >::iterator thisWire = wireList.begin();
	while (thisWire != wireList.end()) {
		if (thisWire->second != nullptr) {
			wxGetApp().logfile << " wire " << thisWire->first << endl << flush;
		}
		thisWire++;
	}
#ifdef _MSG_
	{
		_MSG("printing lists");
		unordered_map< unsigned long, guiGate* >::iterator thisGate = gCircuit->getGates()->begin();
		ostringstream oss;
		while (thisGate != gCircuit->getGates()->end()) {
			float x, y;
			(thisGate->second)->getGLcoords(x, y);
			oss.str(""); oss.clear();
			oss << " gate " << thisGate->first << " type " << thisGate->second->getLibraryGateName() << " at " << x << "," << y;
			_MSG("%s\n", oss.str().c_str());
			oss.str(""); oss.clear();
			oss << "  hotspots:";
			map < string, GLPoint2f > gateHotspots = thisGate->second->getHotspotList();
			map < string, GLPoint2f >::iterator thisHotspot = gateHotspots.begin();
			while (thisHotspot != gateHotspots.end()) {
				oss << thisHotspot->first << " ";
				thisHotspot++;
			}
			_MSG("%s\n", oss.str().c_str());
			thisGate++;
		}
		unordered_map< unsigned long, guiWire* >::iterator thisWire = gCircuit->getWires()->begin();
		while (thisWire != gCircuit->getWires()->end()) {
			if (thisWire->second != nullptr) {
				oss.str(""); oss.clear();
				oss << "wire " << thisWire->first << " status: " << GetStringState(thisWire->second->getState());
				_MSG("%s\n", oss.str().c_str());
				oss.str(""); oss.clear();
				oss << "  connections: ";
				for (unsigned int i = 0; i < thisWire->second->getConnections().size(); i++) {					
					unsigned long index=0;
					map < string, GLPoint2f > gateHotspots = thisWire->second->getConnections()[i].cGate->getHotspotList();
					map < string, GLPoint2f >::iterator thisHotspot = gateHotspots.begin();
					while (thisHotspot != gateHotspots.end()) {
						if (thisHotspot->first == thisWire->second->getConnections()[i].connection)
							break;
						index++;
						thisHotspot++;
					}
					oss << thisWire->second->getConnections()[i].gid << "@" << index;
					oss << " (" << thisWire->second->getConnections()[i].cGate->getLibraryGateName(); 
					oss << "@" << thisWire->second->getConnections()[i].connection << ")";
					if (i < thisWire->second->getConnections().size() - 1) oss << " - ";
				}
				_MSG("%s\n", oss.str().c_str());
				oss.str(""); oss.clear();
				oss << "  segments: ";
				for (unsigned int i = 0; i < thisWire->second->getSegmentMap().size(); i++) {
					oss << "(" << thisWire->second->getSegmentMap()[i].begin.x << "," << thisWire->second->getSegmentMap()[i].begin.y << ")";
					oss << "-(" << thisWire->second->getSegmentMap()[i].end.x << "," << thisWire->second->getSegmentMap()[i].end.y << ")";
					if (i < thisWire->second->getSegmentMap().size() - 1) oss << " ";
				}
				_MSG("%s\n", oss.str().c_str());
			}
			thisWire++;
		}
	}
#endif
}	

// Update the collision checker and refresh
void GUICanvas::Update() {

	if (minimap == NULL){
		return;
	}

	// In case of resize, we should update every so often
	if (wxGetApp().appSystemTime.Time() > wxGetApp().appSettings.refreshRate) {
		wxGetApp().appSystemTime.Pause();
		if (gCircuit->panic) return;
		wxCriticalSectionLocker locker(wxGetApp().m_critsect);
		while (wxGetApp().mexMessages.TryLock() == wxMUTEX_BUSY) wxYield();
		while (wxGetApp().dLOGICtoGUI.size() > 0) {
			gCircuit->parseMessage(wxGetApp().dLOGICtoGUI.front());
			wxGetApp().dLOGICtoGUI.pop_front();
		}
		wxGetApp().mexMessages.Unlock();
		if (gCircuit->panic) return;
		// Do function of number of milliseconds that passed since last step
		gCircuit->lastTime = wxGetApp().appSystemTime.Time();
		gCircuit->lastTimeMod = wxGetApp().timeStepMod;
		gCircuit->lastNumSteps = wxGetApp().appSystemTime.Time() / wxGetApp().timeStepMod;
		gCircuit->sendMessageToCore(klsMessage::Message(klsMessage::MT_STEPSIM, new klsMessage::Message_STEPSIM(wxGetApp().appSystemTime.Time() / wxGetApp().timeStepMod)));
		gCircuit->setSimulate(false);
		wxGetApp().appSystemTime.Start(wxGetApp().appSystemTime.Time() % wxGetApp().timeStepMod);
	}

	minimap->setLists( &gateList, &wireList );
	minimap->setCanvas(this);
	minimap->setCanvas(this);
	updateMiniMap();
	Refresh();
	wxWindow::Update();
}

//Julian: Moved implementation of zoom fuctions out of header.

void GUICanvas::zoomIn() {
	//Only zoom when not dragging
	if (currentDragState == DRAG_NONE) {
		setZoom(getZoom() * ZOOM_STEP);
	}
}

void GUICanvas::zoomOut() {
	//Only zoom when not dragging
	if (currentDragState == DRAG_NONE) {
		setZoom(getZoom() / ZOOM_STEP);
	}
}

klsCommand * GUICanvas::createGateWireConnectionCommand(IDType gateId, const string &hotspot, IDType wireId) {

	guiGate *gate = gateList[gateId];
	guiWire *wire = wireList[wireId];

	// Make sure not already connected.
	if (gate->isConnected(hotspot) &&
		gate->getConnection(hotspot) == wire) {
		return nullptr;
	}

	cmdConnectWire *connectWire = new cmdConnectWire(gCircuit, wireId, gateId, hotspot);

	if (connectWire->validateBusLines()) {
		return connectWire;
	}
	else {
		delete connectWire;
		return nullptr;
	}
}

klsCommand * GUICanvas::createGateConnectionCommand(IDType gate1Id, const string &hotspot1, IDType gate2Id, const string &hotspot2) {

	guiGate *gate1 = gateList[gate1Id];
	guiGate *gate2 = gateList[gate2Id];

	// Don't connect a hotspot to itself.
	if (gate1 == gate2 && hotspot1 == hotspot2) {
		return nullptr;
	}

	// Make sure not already connected.
	if (gate1->isConnected(hotspot1) &&
		gate2->isConnected(hotspot2) &&
		gate1->getConnection(hotspot1) == gate2->getConnection(hotspot2)) {
		return nullptr;
	}

	// Neither connected, so create wire.
	if (!gate1->isConnected(hotspot1) &&
		!gate2->isConnected(hotspot2)) {


		vector<IDType> wireIds(gCircuit->getGates()->at(gate1Id)
			->getHotspot(hotspot1)->getBusLines());

		// Get the correct number of new, unique wire ids.
		for (int i = 0; i < (int)wireIds.size(); i++) {
			wireIds[i] = gCircuit->getNextAvailableWireID();
		}

		cmdConnectWire *connectwire =
			new cmdConnectWire(gCircuit, wireIds[0], gate1Id, hotspot1);

		cmdConnectWire *connectwire2 =
			new cmdConnectWire(gCircuit, wireIds[0], gate2Id, hotspot2);

		cmdCreateWire *createWire =
			new cmdCreateWire(this, gCircuit, wireIds, connectwire, connectwire2);

		if (createWire->validateBusLines()) {
			return createWire;
		}
		else {
			delete createWire;
			return nullptr;
		}
	}
	else {
		
		// One of the gates is connected.
		if (gate1->isConnected(hotspot1)) {
			return createGateWireConnectionCommand(gate2Id,
				hotspot2, gate1->getConnection(hotspot1)->getID());
		}
		else if (gate2->isConnected(hotspot2)) {
			return createGateWireConnectionCommand(gate1Id,
				hotspot1, gate2->getConnection(hotspot2)->getID());
		}
		return nullptr;
	}
}

// Pedro Casanova (casanova@ujaen.es) 2020/04-12
// Connect two diferents wires
// Delete the second wire and creates connections between wire1 and each hotspot of wire2
// Exists a problem: wire2 changes shape
klsCommand * GUICanvas::createWireConnectionCommand(IDType wireId1, IDType wireId2) {

	// Make sure not already connected.
	if (wireId1 == wireId2) 
		return nullptr;
	
	vector<IDType> wireIds(wireList[wireId1]->getIDs().size());

	// Get the correct number of new, unique wire ids.
	for (int i = 0; i < (int)wireIds.size(); i++) {
		wireIds[i] = gCircuit->getNextAvailableWireID();
		_MSGGUI("WireID: %lld\n", wireIds[i])	//@@@@
	}

	cmdMergeWire* mergeWire = new cmdMergeWire(this, gCircuit, wireIds, wireId1, wireId2);

	if (mergeWire->validateBusLines()) {
		return mergeWire;
	}
	else {
		delete mergeWire;
		return nullptr;
	}
}

// Pedro Casanova (casanova@ujaen.es) 2021/01-03
// Add Dynamics gates
bool GUICanvas::addDynamicGate(guiGate* gGate) {

	if (gGate->getLibraryGateName() == "%%_31_GATES" || gGate->getLibraryGateName() == "%%_34_CIRCUIT" || gGate->getLibraryGateName() == "%%_38_PLD") {
		string msg;
		if (!createGatesStruct(gGate, &msg)) {
			wxMessageBox(msg, "Error", wxOK | wxICON_ERROR, NULL);
			return false;
		}
	}
	else if (gGate->getLibraryGateName() == "%%_16_WIRES") {
		createGatesStruct(gGate);
	}
	else {
		replaceGate(gGate);
	}
	return true;
}

// Pedro Casanova (casanova@ujaen.es) 2021/01-03
// Replace Gate %%_ for his correct type (Dynamics gates)
void GUICanvas::replaceGate(guiGate* gGate) {
	// Replace gate	
	map<string, string> gParamList = *gGate->getAllGUIParams();
	map<string, string> lParamList = *gGate->getAllLogicParams();

	ostringstream type;
	gGate->unselect();
	if (gGate->getLibraryGateName() == "%%_11_WIRE") {
		type << "@@_WIRE_" << gParamList.at("LENGTH");
	}
	else if (gGate->getLibraryGateName() == "%%_14_NOWIRE") {
		type << "@@_NOWIRE_" << gParamList.at("WIDTH") << "X" << gParamList.at("HEIGHT");
	}
	else if (gGate->getLibraryGateName() == "%%_17_BUSEND") {
		type << "@@_BUSEND" << (gParamList.at("SEPARATION") == "narrow" ? "N_" : "_") << gParamList.at("INPUT_BITS");
	}
	else if (gGate->getLibraryGateName() == "%%_21_LAND") {
		type << "@@_LAND_" << gParamList.at("INPUT_BITS");
	}
	else if (gGate->getLibraryGateName() == "%%_24_LOR") {
		type << "@@_LOR_" << gParamList.at("INPUT_BITS");
	}
	else if (gGate->getLibraryGateName() == "%%_43_BLQ") {
		type << "@@_BLQ_" << gParamList.at("WIDTH") << "X" << gParamList.at("HEIGHT");
	}
	else if (gGate->getLibraryGateName() == "%%_40_GATE") {
		type << getGate(gGate);
	}
	else if (gGate->getLibraryGateName() == "%%_41_LATCH") {
		type << getLatch(gGate);
	}
	else if (gGate->getLibraryGateName() == "%%_42_FLIPFLOP") {
		type << getFlipFlop(gGate);
	}
	else if (gGate->getLibraryGateName() == "%%_44_CMB") {
		type << "@@_CMB_" << lParamList.at("INPUT_BITS") << "X" << lParamList.at("OUTPUT_BITS");
	}
	else if (gGate->getLibraryGateName() == "%%_47_FSM") {
		type << "@@_FSM_" << ((lParamList.at("ASYNCHRONOUS") == "true") ? "A" : "S") << "_"
			<< lParamList.at("INPUT_BITS") << "X" << lParamList.at("OUTPUT_BITS");
	}
	if (type.str() == "") return;
	int newGID = gCircuit->getNextAvailableGateID();
	float x, y;
	gGate->getGLcoords(x, y);
	cmdCreateGate* creategatecommand = new cmdCreateGate(gCircuit->gCanvas, gCircuit, newGID, type.str(), x, y);
	gCircuit->GetCommandProcessor()->Submit((wxCommand*)creategatecommand);
	gateList[newGID]->select();
	selectedGates.push_back(newGID);

}

string GUICanvas::getGate(guiGate* gGate) {
	map<string, string> gParamList = *gGate->getAllGUIParams();
	string gateType = gParamList.at("GATE_TYPE");
	unsigned long nInputs = atoi(gParamList.at("N_INPUTS").c_str());
	ostringstream oss;
	char index;
	if (nInputs < 11)
		index = 47 + nInputs;
	else
		index = 54 + nInputs;
	if (gateType == "AND")
		oss << "A" << index << "_SAND" << nInputs;
	else if (gateType == "NAND")
		oss << "B" << index << "_SNAND" << nInputs;
	else if (gateType == "OR")
		oss << "C" << index << "_SOR" << nInputs;
	else if (gateType == "NOR")
		oss << "D" << index << "_SNOR" << nInputs;
	else if (gateType == "XOR")
		oss << "E" << index << "_SXOR" << nInputs;
	else if (gateType == "XNOR")
		oss << "F" << index << "_SXNOR" << nInputs;
	return oss.str();
}

string GUICanvas::getLatch(guiGate* gGate) {
	map<string, string> gParamList = *gGate->getAllGUIParams();
	string latchType = gParamList.at("LATCH_TYPE");
	bool signallevel = (gParamList.at("SIGNAL_LEVEL") == "high") ? true : false;
	bool controlLevel = (gParamList.at("CONTROL_LEVEL") == "high") ? true : false;		
	if (latchType == "SR") {
		if (signallevel)
			return "A0_SRLATCH";
		else
			return "A1_SRLATCH_LOW";
	}
	else if (latchType == "SR-controlled") {
		if (controlLevel) {
			if (signallevel)
				return "A2_SRLATCH_CONTROL";
			else
				return "A3_SRLATCH_LOW_CONTROL";
		}
		else {
			if (signallevel)
				return "A4_SRLATCH_CONTROL_LOW";
			else
				return "A5_SRLATCH_LOW_CONTROL_LOW";
		}
	}
	else if (latchType == "D") {
		if (controlLevel)
			return "A6_DLATCH";
		else
			return "A6_DLATCH_LOW";
	}
	return "";
}

string GUICanvas::getFlipFlop(guiGate* gGate) {
	map<string, string> gParamList = *gGate->getAllGUIParams();
	string ffType = gParamList.at("FF_TYPE");
	bool edgeClock = (gParamList.at("EDGE_CLOCK") == "rising") ? true : false;
	bool levelCP = (gParamList.at("LEVEL_CL_PR") == "high") ? true : false;
	string presetClear = gParamList.at("PRESET_CLEAR");
	string prefix;
	ostringstream oss;
	
	if (ffType == "D") prefix = "A";
	else if (ffType == "JK") prefix = "B";
	else prefix = "C";
	
	oss << prefix;

	if (presetClear == "both") {
		if (levelCP)
			oss << "B_" << ffType << "FF";
		else
			oss << "C_" << ffType << "FF_LOW";
	}
	else if (presetClear == "clear") {
		if (levelCP)
			oss << "D_" << ffType << "FF";
		else
			oss << "G_" << ffType << "FF_LOW";
	}
	else
		oss << "H_" << ffType << "FF_NCP";

	if (!edgeClock) oss << "_NT";

	return oss.str();

}

// Pedro Casanova (casanova@ujaen.es) 2021/01-03
// Create two levels gates structs: AND-OR, OR-AND, NAND-NAND or NOR-NOR and wire sets
// Can add input wires whith links an inverters and connect to first level gate inputs
bool GUICanvas::createGatesStruct(guiGate* gGate, string *errorMsg) {

	map<string, string> gParamList = *gGate->getAllGUIParams();
	if (gGate->getLibraryGateName() != "%%_16_WIRES") {
		if (gGate->getLibraryGateName() == "%%_38_PLD") {
			string PLDType = gParamList.at("PLD_TYPE");
			unsigned long inBits = atoi(gParamList.at("INPUT_BITS").c_str());
			unsigned long outBits = atoi(gParamList.at("OUTPUT_BITS").c_str());
			unsigned long inORBits = pow(2, inBits);
			if (PLDType != "PROM")
				inORBits = atoi(gParamList.at("OR_INPUTS").c_str());
			unsigned long nAnd;
			if (PLDType == "PAL")
				nAnd = inORBits * outBits;
			else
				nAnd = inORBits;

			unsigned long linesAND = 0;
			unsigned long charsAND = 0;
			unsigned long linesOR = 0;
			unsigned long charsOR = 0;
			unsigned long totalLength;
			if (PLDType == "PROM") {
				linesOR = outBits;
				charsOR = pow(2, inBits);
			}
			else if (PLDType == "PAL") {
				linesAND = nAnd;
				charsAND = 2 * inBits;
			}
			else {
				linesAND = nAnd;
				charsAND = 2 * inBits;
				linesOR = outBits;
				charsOR = nAnd;
			}
			totalLength = linesAND * charsAND + linesOR * charsOR;

			string connections = "";
			unsigned long cntLines = 0;
			unsigned long cntChars = 0;
			string paramConnections = gParamList.at("CONNECTIONS");
			if (paramConnections != "") {
				paramConnections = paramConnections + "\n";
				for (unsigned int i = 0; i < paramConnections.length(); i++) {
					if (paramConnections[i] != '\n') {
						if (paramConnections[i] != '0' && paramConnections[i] != '1') {
							*errorMsg = "Invalid char, must be '0' or '1'";
							return false;
						}
						connections = connections + paramConnections[i];
						cntChars++;
					} else if (cntChars) {
						if (PLDType == "PROM") {
							if (cntChars != charsOR) {
								*errorMsg = "Invalid connection map line length";
								return false;
							}
						}
						else if (PLDType == "PAL") {
							if (cntChars != charsAND) {
								*errorMsg = "Invalid connection map line length";
								return false;
							}
						}
						else {
							if (cntLines < linesAND) {
								if (cntChars != charsAND) {
									*errorMsg = "Invalid connection map line length";
									return false;
								}
							}
							else {
								if (cntChars != charsOR) {
									*errorMsg = "Invalid connection map line length";
									return false;
								}
							}
						}
						cntChars = 0;
						cntLines++;
					}
				}
				if (PLDType == "PROM") {
					if (cntLines != linesOR) {
						*errorMsg = "Invalid connection map lines";
						return false;
					}
				}
				else if (PLDType == "PAL") {
					if (cntLines != linesAND) {
						*errorMsg = "Invalid connection map lines";
						return false;
					}
				}
				else {
					if (cntLines < linesAND + linesOR) {
						*errorMsg = "Invalid connection map lines";
						return false;
					}
				}
				if (totalLength != connections.length()) {
					*errorMsg = "Invalid connection map length";
					return false;
				}
				gGate->setGUIParam("CONNECTIONS", connections);
			}
		}
		else if (gGate->getLibraryGateName() == "%%_31_GATES") {
			unsigned long nOutputs = 0;
			for (unsigned int i = 1; i <= 8; i++) {
				ostringstream oss;
				oss << "G" << i;
				if (gParamList.at(oss.str()) != "0") nOutputs++;
			}
			if (!nOutputs) {
				*errorMsg = "No gate has defined inputs";
				return false;
			}
		}
		else if (gGate->getLibraryGateName() == "%%_34_CIRCUIT") {
			vector <string> inputNames;
			unsigned long nInputs = 0;
			unsigned long nOutputs = 0;			
			string outputName=gParamList.at("OUTPUT_NAME");
			removeSpaces(&outputName);
			gGate->setGUIParam("OUTPUT_NAME", outputName);
			istringstream iss(gParamList.at("INPUT_NAMES"));
			while (true) {
				string inputName;
				iss >> inputName;
				if (inputName[inputName.length() - 1] == '-') {
					inputName = inputName.substr(0, inputName.length() - 1);
					if (gParamList.at("NO_LINK_INPUT") == "true") {
						ostringstream error;
						error << "Input '" << inputName << "' can't have '-' sufix";
						*errorMsg = error.str();
						return false;
					}
				}
				if (inputName != "") {
					for (unsigned int i = 0; i < inputNames.size(); i++)
						if (inputName == inputNames[i]) {
							ostringstream error;
							error << "Duplicate input '" << inputName << "'";
							*errorMsg = error.str();
							return false;
						}
					inputNames.push_back(inputName);
					nInputs++;
				}
				if (iss.eof()) break;
			}
			if (!nInputs) {
				*errorMsg = "Incorrect number of inputs, must be at least one";
				return false;
			}
			if (nInputs > 16) {
				*errorMsg = "Incorrect number of inputs, must be less than 16";
				return false;
			}
			for (unsigned int i = 1; i <= 16; i++) {
				ostringstream oss;
				oss << "G" << i;
				if (gParamList.at(oss.str()) != "")
				{
					istringstream iss(gParamList.at(oss.str()));
					unsigned long countInputs = 0;
					while (true)
					{
						string term;
						iss >> term;
						if (term != "") {
							unsigned long termcol;
							bool found = false;
							for (unsigned int i = 0; i < nInputs; i++)
								if (term.substr(0, 1) == "/") {
									if (term.substr(1) == inputNames.at(i)) {
										termcol = 2 * i + 1;
										found = true;
										break;
									}
								}
								else {
									if (term == inputNames.at(i)) {
										termcol = 2 * i;
										found = true;
										break;
									}
								}
							if (!found) {
								ostringstream error;
								error << "Unknown input name: '" << term << "' at gate " << i;
								*errorMsg = error.str();
								return false;
							}
							countInputs++;
						}
						if (iss.eof()) break;
					}
					if (!countInputs) {
						ostringstream error;
						error << "Gate " << i << " whithout inputs";
						*errorMsg = error.str();
						return false;
					}
					nOutputs++;
				}
			}
			if (!nOutputs) {
				*errorMsg = "No gate has defined inputs";
				return false;
			}
		}
		else {
			*errorMsg = "Incorrect Gate";
			return false;
		}
	}

	cmdCreateGateStruct* creategatestruct = new cmdCreateGateStruct(gCircuit->gCanvas, gCircuit, gGate);
	gCircuit->GetCommandProcessor()->Submit((wxCommand*)creategatestruct);
	//creategatestruct->Do();

	return true;
}
