
#include "cmdSetParams.h"
#include "../GUICircuit.h"
#include "../guiGate.h"
#include "../OscopeFrame.h"
#include "../MainApp.h"

DECLARE_APP(MainApp);

paramSet::paramSet(ParameterMap *g, ParameterMap* l) {
	gParams = g;
	lParams = l;
};

// Pedro Casanova (casanova@ujaen.es) 2021/01-02
// Added canUndo in contructors to prevent FSM and CMB undo descriptions
cmdSetParams::cmdSetParams(GUICircuit* gCircuit, unsigned long gid,
		paramSet pSet, bool canUndo, bool setMode) :
			klsCommand(canUndo, "Set Parameter") {

	if ((gCircuit->getGates())->find(gid) == (gCircuit->getGates())->end()) return; // error: gate not found
	this->gCircuit = gCircuit;
	this->gid = gid;
	this->fromString = setMode;
	// Save the original set of parameters
	map < string, string >::iterator paramWalk = (*(gCircuit->getGates()))[gid]->getAllGUIParams()->begin();
	while (paramWalk != (*(gCircuit->getGates()))[gid]->getAllGUIParams()->end()) {
		oldGUIParamList[paramWalk->first] = paramWalk->second;
		paramWalk++;
	}
	paramWalk = (*(gCircuit->getGates()))[gid]->getAllLogicParams()->begin();
	while (paramWalk != (*(gCircuit->getGates()))[gid]->getAllLogicParams()->end()) {
		oldLogicParamList[paramWalk->first] = paramWalk->second;
		paramWalk++;
	}
	// Now grab the new ones...
	if (pSet.gParams != NULL) {
		paramWalk = pSet.gParams->begin();
		while (paramWalk != pSet.gParams->end()) {
			newGUIParamList[paramWalk->first] = paramWalk->second;
			paramWalk++;
		}
	}
	if (pSet.lParams != NULL) {
		paramWalk = pSet.lParams->begin();
		while (paramWalk != pSet.lParams->end()) {
			newLogicParamList[paramWalk->first] = paramWalk->second;
			paramWalk++;
		}
	}
}

cmdSetParams::cmdSetParams(string def, bool canUndo) : klsCommand(canUndo, "Set Parameter") {
	_MSGCOM("Command String: %s", def.c_str());	//####
	this->fromString = true;
	istringstream iss(def);
	string dump; char comma;
	unsigned long numgParams, numlParams;
	iss >> dump >> gid >> numgParams >> comma >> numlParams;
	for (unsigned int i = 0; i < numgParams; i++) {
		string paramName, paramVal;
		iss >> paramName;
		getline(iss, paramVal, '\t');
		newGUIParamList[paramName] = paramVal.substr(1, paramVal.size() - 1);
		oldGUIParamList[paramName] = newGUIParamList[paramName];
	}
	for (unsigned int i = 0; i < numlParams; i++) {
		string paramName, paramVal;
		iss >> paramName;
		getline(iss, paramVal, '\t');
		newLogicParamList[paramName] = paramVal.substr(1, paramVal.size() - 1);
		oldLogicParamList[paramName] = newLogicParamList[paramName];
	}
}

bool cmdSetParams::Do() {
	if ((gCircuit->getGates())->find(gid) == (gCircuit->getGates())->end()) return false; // error: gate not found
	map < string, string >::iterator paramWalk = newLogicParamList.begin();
	vector < string > dontSendMessages;
	LibraryGate lg = wxGetApp().libraries[(*(gCircuit->getGates()))[gid]->getLibraryName()][(*(gCircuit->getGates()))[gid]->getLibraryGateName()];
	for (unsigned int i = 0; i < lg.dlgParams.size(); i++) {
		if (lg.dlgParams[i].isGui) continue;
		if (lg.dlgParams[i].type == "FILE_IN" || lg.dlgParams[i].type == "FILE_OUT") dontSendMessages.push_back(lg.dlgParams[i].name);
	}
	while (paramWalk != newLogicParamList.end()) {
		(*(gCircuit->getGates()))[gid]->setLogicParam(paramWalk->first, paramWalk->second);
		bool found = false;
		for (unsigned int i = 0; i < dontSendMessages.size() && !found; i++) {
			if (dontSendMessages[i] == paramWalk->first) found = true;
		}
		if (!found) gCircuit->sendMessageToCore(klsMessage::Message(klsMessage::MT_SET_GATE_PARAM, new klsMessage::Message_SET_GATE_PARAM(gid, paramWalk->first, paramWalk->second)));
		paramWalk++;
	}

	paramWalk = newGUIParamList.begin();
	while (paramWalk != newGUIParamList.end()) {
		(*(gCircuit->getGates()))[gid]->setGUIParam(paramWalk->first, paramWalk->second);
		paramWalk++;
	}

	// Pedro Casanova (casanova@ujaen.es 2020/04-12
	// TO, FROM and LINK are valid signal to Oscope
	if (!fromString && gCircuit->getOscope() != NULL)
		if ((*(gCircuit->getGates()))[gid]->getGUIType() == "TO" || (*(gCircuit->getGates()))[gid]->getGUIType() == "FROM" || (*(gCircuit->getGates()))[gid]->getGUIType() == "LINK")
			gCircuit->getOscope()->UpdateMenu();
	
	return true;
}

bool cmdSetParams::Undo() {

	if ((gCircuit->getGates())->find(gid) == (gCircuit->getGates())->end()) return false; // error: gate not found
	map < string, string >::iterator paramWalk = oldLogicParamList.begin();
	vector < string > dontSendMessages;
	LibraryGate lg = wxGetApp().libraries[(*(gCircuit->getGates()))[gid]->getLibraryName()][(*(gCircuit->getGates()))[gid]->getLibraryGateName()];
	for (unsigned int i = 0; i < lg.dlgParams.size(); i++) {
		if (lg.dlgParams[i].isGui) continue;
		if (lg.dlgParams[i].type == "FILE_IN" || lg.dlgParams[i].type == "FILE_OUT") dontSendMessages.push_back(lg.dlgParams[i].name);
	}
	while (paramWalk != oldLogicParamList.end()) {
		(*(gCircuit->getGates()))[gid]->setLogicParam(paramWalk->first, paramWalk->second);
		bool found = false;
		for (unsigned int i = 0; i < dontSendMessages.size() && !found; i++) {
			if (dontSendMessages[i] == paramWalk->first) found = true;
		}
		if (!found) gCircuit->sendMessageToCore(klsMessage::Message(klsMessage::MT_SET_GATE_PARAM, new klsMessage::Message_SET_GATE_PARAM(gid, paramWalk->first, paramWalk->second)));
		paramWalk++;
	}
	paramWalk = oldGUIParamList.begin();
	while (paramWalk != oldGUIParamList.end()) {
		(*(gCircuit->getGates()))[gid]->setGUIParam(paramWalk->first, paramWalk->second);
		paramWalk++;
	}
	// Pedro Casanova (casanova@ujaen.es 2020/04-12
	// TO, FROM and LINK are valid signal to Oscope
	if ((!fromString && (*(gCircuit->getGates()))[gid]->getGUIType() == "TO")  || (!fromString && (*(gCircuit->getGates()))[gid]->getGUIType() == "FROM") || (!fromString && (*(gCircuit->getGates()))[gid]->getGUIType() == "LINK"))
		gCircuit->getOscope()->UpdateMenu();

	return true;
}

string cmdSetParams::toString() const {

	ostringstream oss;
	oss << "setparams " << gid << " " << newGUIParamList.size() << "," << newLogicParamList.size() << " ";
	map < string, string >::const_iterator paramWalk = newGUIParamList.cbegin();
	while (paramWalk != newGUIParamList.cend()) {
		oss << paramWalk->first << " " << paramWalk->second << "\t";
		paramWalk++;
	}
	paramWalk = newLogicParamList.begin();
	while (paramWalk != newLogicParamList.end()) {
		oss << paramWalk->first << " " << paramWalk->second << "\t";
		paramWalk++;
	}
	return oss.str();
}

void cmdSetParams::setPointers(GUICircuit* gCircuit, GUICanvas* gCanvas,
		TranslationMap &gateids, TranslationMap &wireids) {

	gid = gateids[gid];
	this->gCircuit = gCircuit;
	this->gCanvas = gCanvas;
}