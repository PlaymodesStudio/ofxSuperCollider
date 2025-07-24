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

// VST Parameter Event Structure
struct VSTParameterEvent {
	int nodeID;
	int synthIndex;
	int parameterIndex;
	float value;
	std::string name;
};

// VST Plugin Opened Event Structure
struct VSTPluginOpenedEvent {
	int nodeID;
	int synthIndex;
	bool success;
	bool hasEditor;
	float latency;
};

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
	
	// VST Events
	ofEvent<VSTParameterEvent> vstParameterChanged;
	ofEvent<VSTParameterEvent> vstParameterAutomated;
	ofEvent<VSTPluginOpenedEvent> vstPluginOpened;
	
protected:

	ofxOscSenderReceiver   osc;
	ofEventListener listener;
	
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
	std::string decodeStringFromFloats(const ofxOscMessage& message, int startIndex);
};
