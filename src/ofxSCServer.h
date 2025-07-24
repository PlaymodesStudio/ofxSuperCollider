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
#include "ofxOscSenderReceiver.h"
#include "ofxSCResourceAllocator.h"

class ofxSCBuffer;
class ofxSCBus;
class ofxSCNode;


class ofxSCServer
{
public:	
	ofxSCServer(std::string hostname = "localhost", unsigned int port = 57110);
	~ofxSCServer();

	static ofxSCServer     *local();
	
	void process();
	void _process(ofEventArgs &e);
	void notify();
	
	void sendMsg(ofxOscMessage& message);
    void sendBundle(ofxOscBundle& bundle);
    
    void setWaitToSend(bool b);
    bool getWaitToSend();
    void sendStoredBundle();
    
    void setLatency(float _latency){latency = _latency;};
    void setBLatency(bool b){b_latency = b;};
	
	ofxSCResourceAllocator *allocatorBusAudio;
	ofxSCResourceAllocator *allocatorBusControl;
	ofxSCResourceAllocator *allocatorBuffer;
	ofxSCResourceAllocator *allocatorSynth;

	ofxSCBuffer *buffers[4096];
    ofxSCBus *controlBusses[4096];
    ofxSCBus *audioBusses[65536];
    
    ofEvent<void> serverBootedEvent;
    ofEvent<void> serverInitializedEvent;
    
    void addNodeListener(ofxSCNode* node);
    void removeNodeListener(ofxSCNode* node);
    
    ofEvent<ofxOscMessage> newFeedbackMessage;
    
protected:

	ofxOscSenderReceiver   osc;
    ofEventListener listener;
    std::map<ofxSCNode*, std::function<void(ofxOscMessage&)>> nodeFeedbackFunctions;
    
    bool waitToSend;
    
    ofxOscBundle toSendBundle;
	
	static ofxSCServer *plocal;
	std::string hostname;
	unsigned int port;
    
    float latency;
    bool b_latency;
    
    bool booted;
    bool initialized;
    
    int numNotifies;
    
private:
    uint64_t getNowTimetag(float latency = 0);
};

