/*****************************************************************************
   Project: CEDAR Logic Simulator
   Copyright 2006 Cedarville University, Benjamin Sprague,
                     Matt Lewellyn, and David Knierim
   All rights reserved.
   For license information see license.txt included with distribution.   

   guiGate: GUI representation of gate objects
*****************************************************************************/

#ifndef GUIGATE_H_
#define GUIGATE_H_


#include "gl_wrapper.h"

class guiWire;

#include <float.h>
#include <vector>
#include <set>
#include <map>
#include <sstream>
#include <string>
#include <fstream>
#include "wx/glcanvas.h"
#include "../logic/logic_values.h"
#include "XMLParser.h"
#include "guiText.h"
#include "klsCollisionChecker.h"
#include "klsMessage.h"
#include "wx/docview.h"

#include "RamPopupDialog.h"
#include "FSMParamDialog.h"
#include "CMBParamDialog.h"

#include "GUICircuit.h"
#include "../logic/logic_circuit.h"

using namespace std;

#define GATE_HOTSPOT_THICKNESS 0.05

class gateHotspot : public klsCollisionObject {
friend class guiGate;
public:
	gateHotspot() : klsCollisionObject(COLL_GATE_HOTSPOT) {
		modelLocation = worldLocation = GLPoint2f( 0, 0 );
		busLines = 1;
		calcBBox();
	};

	gateHotspot( string hsName ) : klsCollisionObject(COLL_GATE_HOTSPOT), name( hsName ) {
		gateHotspot();
		busLines = 1;
	};

	// Create the bbox for this hotspot:
	void calcBBox( void ) {
		klsBBox newBBox;
		newBBox.addPoint( worldLocation );

		newBBox.extendTop( (GLfloat) GATE_HOTSPOT_THICKNESS / 2.0 );
		newBBox.extendBottom( (GLfloat) GATE_HOTSPOT_THICKNESS / 2.0 );
		newBBox.extendLeft( (GLfloat) GATE_HOTSPOT_THICKNESS / 2.0 );
		newBBox.extendRight( (GLfloat) GATE_HOTSPOT_THICKNESS / 2.0 );

		this->setBBox( newBBox );
	};

	GLPoint2f getLocation( void ) { return worldLocation; };

	void setBusLines(int _busLines) {
		busLines = _busLines;
	}

	int getBusLines() {
		return busLines;
	}

public:
	string name;

protected:
	int busLines;
	GLPoint2f modelLocation, worldLocation;
};


class guiGate : public klsCollisionObject {
public:
	guiGate();	
	virtual ~guiGate();
	void setID(long nid) { gateID = nid; };
	unsigned long getID() { return gateID; };
	// Pedro Casanova (casanova@ujaen.es) 2020/04-12
	void setGUICircuit(GUICircuit* theGUICircuit) { ourGUICircuit = theGUICircuit; };

protected:
	string libName;
	string libGateName;
	GUICircuit* ourGUICircuit = NULL;

public:
	void setLibraryName( string nLibName, string nLibGateName ) {
		libName = nLibName;
		libGateName = nLibGateName;
	};

	string getLibraryName( void ) {
		return libName;
	};

	string getLibraryGateName( void ) {
		return libGateName;
	};

	string getLogicType();
	string getGUIType();

	//****************************************************************
	//Edit by Joshua Lansford 12/25/2006
	//I made the doParamsDialog method below virtual
	//so that the that the subclass can call its own special type
	//of pop-up that it wants.
	//Specifically, the Z-80 Processor can call up it's pop-up window
	//and the ram gate can call up its own window that shows all
	//its ram contents.
	//*****************************************************************

	// Function to show the gate's parameters dialog, takes the command
	//	processor object to assign the setparameters command to.  gc is
	//	a GUICircuit pointer
	virtual void doParamsDialog( void* gc, wxCommandProcessor* wxcmd );
	// Pedro Casanova (casanova@ujaen.es) 2021/01-03
	// Show gate propierties
	virtual void doPropsDialog();

	// Set and get param virtual functions, simply assigns a string
	virtual void setGUIParam(string paramName, string value);
	virtual string getGUIParam( string paramName ) { return gparams[paramName]; };
	map < string, string >* getAllGUIParams() { return &gparams; };
	virtual void setLogicParam(string paramName, string value) { lparams[paramName] = value; };
	virtual string getLogicParam( string paramName ) { return lparams[paramName]; };
	map < string, string >* getAllLogicParams() { return &lparams; };

	void declareInput(string name) { isInput[name] = true; };
	void declareOutput(string name) { isInput[name] = false; };
	// Pedro Casanova (casanova@ujaen.es) 2020/04-12
	// Added drawPalette to do not draw wide outlines in palette
	virtual void draw(bool color = true, bool drawPalette = false);
	void setGLcoords( float x, float y, bool noUpdateWires = false );
	void getGLcoords( float &x, float &y );
	
	// Shift the gate by x and y, relative to its current location:
	void translateGLcoords( float x, float y );
	// Make all connections end their drag so segments are merged
	void finalizeWirePlacements( void );
	
	// Draw this gate as unselected:
	void unselect( void );
	void select( void ) { selected = true; };
	// Needed for toggles and keypads, maybe others; returns a message to be passed
	//	from a click.
	virtual klsMessage::Message_SET_GATE_PARAM* checkClick( GLfloat x, GLfloat y ) { return NULL; };
	
	// Draw this gate as selected from now until unselect() is
	// called, if the coordinate passed to it is within
	// this gate's bounding box in GL coordinates.
	// Return true if this gate is selected.
	bool clickSelect( GLfloat x, GLfloat y );

	// Insert a line in the line list.
	void insertLine( float x1, float y1, float x2, float y2, int w = 1 );

	// Pedro Casanova (casanova@ujaen.es) 2020/04-12
	// Lines with offset for rotate chars
	void insertTextLine(float x0, float y0, float x1, float y1, float x2, float y2, int w = 1);

	// Recalculate the bounding box, based on the lines that are included alredy:
	virtual void calcBBox( void );


	// Insert a hotspot in the hotspot list.
	void insertHotspot( float x1, float y1, string connection, int busLines);

	// Check if any of the hotspots of this gate are within the delta
	// of the world coordinates sX and sY. delta is in gl coords.
	string checkHotspots( GLfloat x, GLfloat y, GLfloat delta );

	// Return the coordinates of the hotspot in GL world-space coords.
	void getHotspotCoords(string hsName, float &x, float &y);

	// Get the name of the hotspot that overlaps the one given.
	// Return "" for no overlap or bad hotspot name.
	std::string getHotspotPal(const std::string &hotspot);

	// Is a particular hotspot aligned to the vertical or horizontal edge?
	bool isVerticalHotspot( string hsName );

	// Update all wires connected to me
	// Update the connected wires' shapes to accomidate the new gate position:
	void updateConnectionMerges( void );
	
	klsBBox getModelBBox( void ) { return modelBBox; };

	// Get a hotspot from its name.
	gateHotspot * getHotspot(const std::string &hotspotName) {
		return hotspots[hotspotName];
	}

protected:
	// Pedro Casanova (casanova@ujaen.es) 2020/04-12
	// Added Rotate
	// Convert model->world coordinates:
	GLPoint2f modelToWorld( GLPoint2f c, bool Rotate = true );

	// Get a world-space bounding box:
	klsBBox getWorldBBox( void ) { return this->getBBox(); };

protected:
	void updateBBoxes( bool noUpdateWires = false );

	GLdouble mModel[16];

public:

	void addConnection(string, guiWire*);
	guiWire* getConnection( string hotspot ) { return connections[hotspot]; };
	void removeConnection(string, int&);
	bool isConnected(string);
	bool isSelected() { return selected; };
	bool isConnectionInput(string idx) { return isInput[idx]; };
	
	void saveGate(XMLParser*);
	
	//*********************************
	//Edit by Joshua Lansford 6/06/2007
	//I want the ram files to save their contents to file
	//with the circuit.  However, I don't want to send
	//an entire page of data up to the gui each time
	//an entry changes.
	//This way the ram gate can intelegently save what
	//it wants to into the file.
	//Also any other gate that wishes too, can also
	//save specific stuff.
	virtual void saveGateTypeSpecifics( XMLParser* xparse ){};
	//End of edit***********************


	// Return the map of hotspot names to their coordinates:
	map<string, GLPoint2f> getHotspotList( void ) { 
		map< string, GLPoint2f > remappedHS;
		map< string, gateHotspot* >::iterator hs = hotspots.begin();
		while( hs != hotspots.end() ) {
			remappedHS[hs->first] = (hs->second)->getLocation();
			hs++;
		}
		return remappedHS; 
	};
protected:
	long gateID;
	string gateType;
	string guiGateType;
	
	GLfloat myX, myY;

	bool selected; // Is this gate selected or not?

	// Model space bounding box:
	klsBBox modelBBox;
	
	// Pedro Casanova (casanova@ujaen.es) 2020/04-12
	// Now use lines instead of vertices to contain width
	//vector<GLPoint2f> vertices;
	vector<lgLine> lines;
	// Pedro Casanova (casanova@ujaen.es) 2020/04-12
	// Lines with offset for rotate chars
	vector<lgOffLine> textLines;

	// map i/o name to hotspot coord
	map< string, gateHotspot* > hotspots;
	// map i/o name to wire id
	map< string, guiWire* > connections;
	// map i/o name to status as input (true = input, false = output)
	map< string, bool > isInput;
	// map param name to value
	map< string, string > gparams;	// GUI params
	map< string, string > lparams;  // Logic params
};

// Pedro Casanova (casanova@ujaen.es) 2020/04-12
// For draw PLD gates (LAND and LOR)
class guiGatePLD : public guiGate {
public:
	guiGatePLD();
	// Pedro Casanova (casanova@ujaen.es) 2020/04-12
	// Added drawPalette to do not draw wide outlines in palette
	void draw(bool color = true, bool drawPalette = false);

	void setGUIParam(string paramName, string value);

protected:
	// Pedro Casanova (casanova@ujaen.es) 2020/04-12
	// Cross in gate for PLD
	GLPoint2f renderInfo_crossPoint;
};

// Pedro Casanova (casanova@ujaen.es) 2020/04-12
// For draw BUSEND with color 
class guiGateBUSEND : public guiGate {
public:
	// Pedro Casanova (casanova@ujaen.es) 2020/04-12
	// Added drawPalette to do not draw wide outlines in palette
	void draw(bool color = true, bool drawPalette = false);
};

class guiGateTOGGLE : public guiGate {
public:
	guiGateTOGGLE();
	// Pedro Casanova (casanova@ujaen.es) 2020/04-12
	// Added drawPalette to do not draw wide outlines in palette
	void draw(bool color = true, bool drawPalette = false);
	
	void setGUIParam( string paramName, string value );
	void setLogicParam( string paramName, string value );

	// Toggle the output button on and off:
	string getState() { return getLogicParam("TOGGLE_STATE"); };
	klsMessage::Message_SET_GATE_PARAM* checkClick( GLfloat x, GLfloat y );

protected:
	int renderInfo_outputNum;
	GLLine2f renderInfo_clickBox;
};

class guiGateKEYPAD : public guiGate {
public:
	guiGateKEYPAD();
	// Pedro Casanova (casanova@ujaen.es) 2020/04-12
	// Added drawPalette to do not draw wide outlines in palette
	void draw(bool color = true, bool drawPalette = false);
	// Pedro Casanova (casanova@ujaen.es) 2020/04-12
	// Parameter ROTATE for old style KEYPAD can only be true
	void setGUIParam(string paramName, string value);
	void setLogicParam( string paramName, string value );
	
	// Toggle the output button on and off:
	klsMessage::Message_SET_GATE_PARAM* checkClick( GLfloat x, GLfloat y );
protected:
	GLLine2f renderInfo_valueBox;
private:
	string keypadValue;
};

class guiGateREGISTER : public guiGate {
public:
	guiGateREGISTER();
	// Pedro Casanova (casanova@ujaen.es) 2020/04-12
	// Added drawPalette to do not draw wide outlines in palette
	void draw(bool color = true, bool drawPalette = false);
	void setGUIParam( string paramName, string value );
	void setLogicParam( string paramName, string value );
protected:
	GLLine2f renderInfo_valueBox;
	GLdouble renderInfo_diffx;
	GLdouble renderInfo_diffy;
	bool renderInfo_drawBlue;
	int renderInfo_numDigitsToShow;
	string renderInfo_currentValue;
	bool display_BCD;	// Pedro Casanova (casanova@ujaen.es) 2020/04-12
	bool hide_display;	// Pedro Casanova (casanova@ujaen.es) 2020/04-12
};

class guiGatePULSE : public guiGate {
public:
	guiGatePULSE();
	// Pedro Casanova (casanova@ujaen.es) 2020/04-12
	void setGUIParam(string paramName, string value);
	
	klsMessage::Message_SET_GATE_PARAM* checkClick( GLfloat x, GLfloat y );

protected:
	// Pedro Casanova (casanova@ujaen.es) 2020/04-12
	// CLICK_BOX limits
	GLLine2f renderInfo_clickBox;

};


class guiGateLED : public guiGate {
public:
	guiGateLED();
	// Pedro Casanova (casanova@ujaen.es) 2020/04-12
	// Added drawPalette to do not draw wide outlines in palette
	void draw(bool color = true, bool drawPalette = false);
	void setGUIParam( string paramName, string value );
protected:
	GLLine2f renderInfo_ledBox;
};

// Pedro Casanova (casanova@ujaen.es) 2020/04-12
// For wires
class guiGateWIRE : public guiGate {
public:
	// Pedro Casanova (casanova@ujaen.es) 2020/04-12
	// Added drawPalette to do not draw wide outlines in palette
	void draw(bool color = true, bool drawPalette = false);
};


// ************************ Labels *************************
#define SELECTED_LABEL_INTENSITY 0.50

class guiLabel : public guiGate {
public:
	guiLabel();
	// Pedro Casanova (casanova@ujaen.es) 2020/04-12
	// Added drawPalette to do not draw wide outlines in palette
	void draw(bool color = true, bool drawPalette = false);

	// Recalculate the label's bounding box:
	void calcBBox( void );
	
	// A convenience function that translates
	// TEXT_HEIGHT parameter into a GLdouble:
	GLdouble getTextHeight( void ) {
		istringstream iss(gparams["TEXT_HEIGHT"]);
		GLdouble textHeight = 1.0;
		iss >> textHeight;
		
		return textHeight;
	};

	// A custom setParam function is required because
	// the object must resize it's bounding box 
	// each time the LABEL_TEXT or TEXT_HEIGHT parameter is set.
	void setGUIParam( string paramName, string value );

private:
	guiText theText;
};


// Pedro Casanova (casanova@ujaen.es) 2020/04-12
// Modified to permit variable labes size.
// ************************ TO/FROM gate *************************
class guiTO_FROM : public guiGate {
public:
	guiTO_FROM();

	// Pedro Casanova (casanova@ujaen.es) 2020/04-12
	// Added drawPalette to do not draw wide outlines in palette
	void draw(bool color = true, bool drawPalette = false);

	// Recalculate the gate's bounding box:
	void calcBBox( void );

	// A convenience function that translates
	// TEXT_HEIGHT parameter into a GLdouble:
	GLdouble getTextHeight(void) {
		istringstream iss(gparams["TEXT_HEIGHT"]);
		GLdouble textHeight = 1.0;
		iss >> textHeight;
		
		return textHeight;
	};
	
	// A custom setParam function is required because
	// the object must resize it's bounding box 
	// each time the JUNCTION_ID parameter is set.
	void setLogicParam( string paramName, string value );

	void setGUIParam(string paramName, string value);

private:
	guiText theText;
};


// ************************ RAM gate ****************************

//*************************************************
// Pedro Casanova (casanova@ujaen.es) 2020/04-12
// Modified to limit data size values.
//Edit by Joshua Lansford 12/25/2006
//I am creating a guiGate for the RAM so that
//the ram can have its own special pop-up window
class guiGateRAM : public guiGate {
public:
	guiGateRAM();
	
	// Function to show the gate's parameters dialog, takes the command
	//	processor object to assign the setparameters command to.  gc is
	//	a GUICircuit pointer
	virtual void doParamsDialog( void* gc, wxCommandProcessor* wxcmd );
	
	//Destructor for cleaning up private vars
	virtual ~guiGateRAM();
	
	//Saves the ram contents to the circuit file
	//when the circuit saves
	virtual void saveGateTypeSpecifics( XMLParser* xparse );
	
	//Because the ram gui will be passed lots of data
	//from the ram logic, we don't want it all going
	//into the default hash of changed paramiters.
	//Thus we catch it here
	virtual void setLogicParam( string paramName, string value );

	//This method is used by the RamPopupDialog to
	//learn what values are at different addresses
	//in memory.
	unsigned long getValueAt( unsigned long address );
	
	//These is used by the pop-up to determine
	//what was the last value read and written
	long getLastWritten();
	long getLastRead();

private:
	//The pop-up dialog
	RamPopupDialog* ramPopupDialog;
	
	//This data is so we can fill the pop-up with relevent
	//information
	unsigned long dataBits;
	unsigned long addressBits;
	map< unsigned long, unsigned long > memory;


	
	//used for highlighting the last read and
	//written in the pop-up
	long lastWritten;
	long lastRead;
};
//End of edit

// ************************ FSM gate ****************************

//*************************************************
// Pedro Casanova (casanova@ujaen.es) 2021/01-03
//I am creating a guiGate for the FSM so that
//the FSM can have its own special pop-up window
class guiGateFSM : public guiGate {
public:
	guiGateFSM();

	// Function to show the gate's parameters dialog, takes the command
	//	processor object to assign the setparameters command to.  gc is
	//	a GUICircuit pointer
	virtual void doParamsDialog(void* gc, wxCommandProcessor* wxcmd);
	virtual void saveGateTypeSpecifics(XMLParser* xparse);

	//Destructor for cleaning up private vars
	virtual ~guiGateFSM();

private:
	//The pop-up dialog
	FSMParamDialog* fsmParamDialog;

};
//*************************************************

// ************************ CMB gate ****************************

//*************************************************
// Pedro Casanova (casanova@ujaen.es) 2021/01-03
//I am creating a guiGate for the CMB so that
//the CMB can have its own special pop-up window
class guiGateCMB : public guiGate {
public:
	guiGateCMB();

	// Function to show the gate's parameters dialog, takes the command
	//	processor object to assign the setparameters command to.  gc is
	//	a GUICircuit pointer
	virtual void doParamsDialog(void* gc, wxCommandProcessor* wxcmd);
	virtual void saveGateTypeSpecifics(XMLParser* xparse);

	//Destructor for cleaning up private vars
	virtual ~guiGateCMB();

private:
	//The pop-up dialog
	CMBParamDialog* cmbParamDialog;

};
//*************************************************

#endif /*GUIGATE_H_*/
