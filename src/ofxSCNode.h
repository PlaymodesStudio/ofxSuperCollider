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

    bool isCreated() const { return created; }
    // Created, but its /n_go not seen yet: after a while the server asks for it
    // (/n_query). A lost /n_go (UDP) would otherwise hold every later set()
    // back for good. An /n_info for it counts as the /n_go.
    void markPending();
    void checkPending(double now);
    // True once the server reported /n_end for this node (it freed itself,
    // e.g. doneAction: 2, or was freed), until it is created again.
    bool hasEnded() const { return ended; }
		
    ofEvent<ofxOscMessage> newFeedbackMessage;
protected:
    
    void setServer(ofxSCServer *_server);
    ofxSCServer* getServer();

	bool created;
	bool ended = false;
    double pendingSince = -1.0, lastQuery = -1.0;
    int queries = 0;
    
private:
    
    ofxSCServer *server;
    std::vector<ofxOscMessage> storedMessages;
};
