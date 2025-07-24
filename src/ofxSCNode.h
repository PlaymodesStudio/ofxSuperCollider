/*-----------------------------------------------------------------------------
 *
 * ofxSuperCollider: a SuperCollider control addon for openFrameworks.
 *
 * Copyright (c) 2009 Daniel Jones.
 *
 *	 <http://www.erase.net/>
 *
 * Distributed under the MIT License.
 * For more information, see ofxSuperCollider.h.
 *
 *---------------------------------------------------------------------------*/

#pragma once

#include <vector>
#include "ofxOsc.h"
#include "ofxSCServer.h"

class ofxSCGroup;

class ofxSCNode
{
public:	
	ofxSCNode(ofxSCServer *server = ofxSCServer::local());
	~ofxSCNode();
	
	void addToHead(ofxSCGroup group);
	void addToHead(unsigned int groupID) { create(0, groupID); }
	void addToHead() { create(0, 1); }
	void addToTail(ofxSCGroup group);
	void addToTail(unsigned int groupID) { create(1, groupID); }
	void addToTail() { create(1, 1); }
	void addBefore(const ofxSCNode& node) { create(2, node.nodeID); }
	void addAfter(const ofxSCNode& node) { create(3, node.nodeID); }
	
	// pure virtual method
	virtual void create(int position = 0, int groupID = 1);
    void order(int position = 0, int groupID = 1);
    void order(int position = 0, std::vector<int> groupIDs = {1});
    
    void moveBefore(int nodeID);
    void moveAfter(int nodeID);
    
    void run(bool b);
	void free();

	static int id_base;
	
	// can't use 'id' as a keyword when mixing with objective-c!
	int nodeID;
    
    void feedbackListener(ofxOscMessage &msg);
    virtual void resendStoredArgs(){};
		
    ofEvent<ofxOscMessage> newFeedbackMessage;
protected:
    
    void setServer(ofxSCServer *_server);
    ofxSCServer* getServer();

	bool created;
    
private:
    
    ofxSCServer *server;
};
