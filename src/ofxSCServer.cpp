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

#include "ofxSCServer.h"
#include "ofxSCBuffer.h"
#include "ofxOsc.h"
#include "ofxSCNode.h"
#include <algorithm>
#include <chrono>
#include <cstdint>

#define MILISECONDS_FROM_1900_to_1970 2208988800000ULL
#define SECONDS_FROM_1900_TO_1970 2208988800ULL
#define TWO_TO_THE_32_OVER_ONE_MILLION 4295

#define INTIALIZATION_ID 1917  //Init with numbers

namespace {
// One per thread: several servers may be driven from the same scheduling
// scope, and a scope must not leak into another thread's sends.
thread_local uint64_t scopedTimetagValue = 0;

// NTP timetag from a Unix epoch time in microseconds. Microsecond resolution
// matters: the previous millisecond rounding was already a third of an audio
// control block, which is most of what timestamping is meant to remove.
uint64_t timetagFromUnixMicroseconds(int64_t unixMicroseconds){
    if(unixMicroseconds < 0) return 1;
    const uint64_t seconds = (uint64_t)(unixMicroseconds / 1000000) + SECONDS_FROM_1900_TO_1970;
    const uint64_t microseconds = (uint64_t)(unixMicroseconds % 1000000);
    const uint32_t fractionalPart = (uint32_t)((microseconds * 4294967296ULL) / 1000000ULL);
    return (seconds << 32) + fractionalPart;
}
}

ofxSCServer *ofxSCServer::plocal = NULL;

ofxSCServer::ofxSCServer(std::string hostname, unsigned int port, unsigned int receivePort, unsigned int numInputs, unsigned int numOutputs, unsigned int numAudioBusses, unsigned int numControlBusses, unsigned int numBuffers)
{
	this->hostname = hostname;
	this->port = port;
	this->numInputs = numInputs;
	this->numOutputs = numOutputs;
	this->numAudioBusses = numAudioBusses;
	this->numControlBusses = numControlBusses;
	this->numBuffers = numBuffers;

    osc.setup(hostname, port, receivePort);
    listener = ofEvents().update.newListener(this, &ofxSCServer::_process);
	
	allocatorBusAudio = new ofxSCResourceAllocator(numAudioBusses);
	allocatorBusAudio->pos = numInputs + numOutputs;
	
	allocatorBusControl = new ofxSCResourceAllocator(numControlBusses);
	allocatorBuffer = new ofxSCResourceAllocator(numBuffers);
	allocatorSynth = nullptr;
    
    audioBusses.resize(numAudioBusses);
    controlBusses.resize(numControlBusses);
    buffers.resize(numBuffers);
	
	if (plocal == 0)
		plocal = this;
    
    waitToSend = false;
    
    latency = 0.2;
    b_latency = false;
    
    initializing = false;
}

ofxSCServer::~ofxSCServer()
{
}

ofxSCServer *ofxSCServer::local()
{
	if (plocal == 0)
	{
		plocal = new ofxSCServer();
	}
	
	return plocal;
}

// dummy method for oF event notification system
void ofxSCServer::_process(ofEventArgs &e)
{
	this->process();
}

void ofxSCServer::process()
{
    ofxOscMessage m;
    m.setAddress("/status");
    osc.sendMessage(m);
    
//#ifdef _ofxOscSENDERRECEIVER_H

	while(osc.hasWaitingMessages())
	{
		ofxOscMessage m;
		osc.getNextMessage(m);
//		printf("** got OSC! %s\n", m.getAddress().c_str());
//        ofLog() << m;
        
        if (m.getAddress() == "/status.reply"){
            int numSynths   = m.getArgAsInt(2);
            int numGroups   = m.getArgAsInt(3);
            int numSynthDefs = m.getArgAsInt(4);
            
            if(!initializing && numGroups == 1 && numSynthDefs == 0 && numSynths == 0){ //Server rebooted
                resetAllocators();
                serverBootedEvent.notify(this);
                initializing = true;
//                ofLog() << "Server Booted";
            }
        }
		
		/*-----------------------------------------------------------------------------
		 * /done
		 *  - buffer read completed
		 /*---------------------------------------------------------------------------*/
		else if (m.getAddress() == "/done")
		{
//			int index = m.getArgAsInt32(1);
//			printf("** buffer read completed, ID %d\n", index);
//			buffers[index]->ready = true;
		}
        
        else if (m.getAddress() == "/synced")
        {
            int id = m.getArgAsInt(0);
            if(id == INTIALIZATION_ID && initializing){
                initializing = false;
                serverInitializedEvent.notify(this);
//                ofLog() << "Server Initialized";
            }
        }

		/*-----------------------------------------------------------------------------
		 * /b_info
		 *  - information on buffer size and channels
		/*---------------------------------------------------------------------------*/
		else if (m.getAddress() == "/b_info")
		{
			int index = m.getArgAsInt32(0);
			buffers[index]->frames = m.getArgAsInt32(1);
			buffers[index]->channels = m.getArgAsInt32(2);
			buffers[index]->sampleRate = m.getArgAsFloat(3);
			buffers[index]->ready = true;			
		}
		
		// buffer alloc/read failed
		else if (m.getAddress() == "/fail")
		{
		}
        
        else if (m.getAddress() == "/d_removed") //What it does? just one string argument.
        {
        }
        
		else if (m.getAddress() == "/c_set"){
			int firstIndex = m.getArgAsInt32(0);
			for(int i = 0; i < m.getNumArgs(); i+=2){
				int index = m.getArgAsInt32(i);
				int arrayIndex = index - firstIndex;
				
				if(firstIndex >= 0 && firstIndex < (int)controlBusses.size() &&
				   arrayIndex >= 0 &&
				   controlBusses[firstIndex] != NULL) {
					
					try {
						ofxSCBus* bus = controlBusses[firstIndex];
						// Add corruption check
						if(bus->channels > 0 &&
						   arrayIndex < bus->readValues.size()) {
							bus->readValues[arrayIndex] = m.getArgAsFloat(i+1);
						}
					} catch(...) {
						// Skip corrupted bus data
					}
				}
			}
		}
        else if (m.getAddress() == "/g_queryTree.reply"){
            queryTreeReplyEvent.notify(m);
        }
        
        //Node Notifications from server (n_go, n_end.., ugen notifications)
        //And Poll replies from synths
        else{
            for(auto &nff : nodeFeedbackFunctions) nff.second(m);
        }
	}
	
//#else
//
//	fprintf(stderr, "This version of ofxOsc does not have support for sender/receive objects. Please update to enable receiving responses from SuperCollider.\n");
//
//#endif
	
}

void ofxSCServer::notify()
{
	ofxOscMessage m;
	m.setAddress("/notify");
	m.addIntArg(1);
	osc.sendMessage(m, true);
}

void ofxSCServer::resetAllocators()
{
	if(allocatorBusAudio != nullptr){
		allocatorBusAudio->reset(numInputs + numOutputs);
	}
	if(allocatorBusControl != nullptr){
		allocatorBusControl->reset(0);
	}
	if(allocatorBuffer != nullptr){
		allocatorBuffer->reset(0);
	}
	if(allocatorSynth != nullptr){
		allocatorSynth->reset(0);
	}

	std::fill(audioBusses.begin(), audioBusses.end(), nullptr);
	std::fill(controlBusses.begin(), controlBusses.end(), nullptr);
	std::fill(buffers.begin(), buffers.end(), nullptr);
}

void ofxSCServer::sendInitializationSyncMessage(){
    ofxOscMessage m3;
    m3.setAddress("/sync");
    m3.addIntArg(INTIALIZATION_ID);
    sendMsg(m3);
}

void ofxSCServer::sendMsg(ofxOscMessage& m)
{
    if(toSendBundle.getMessageCount() > 1000) sendStoredBundle();
    const uint64_t scoped = getScopedTimetag();
    if(scoped != 0 && !waitToSend){
        // Inside a ScopedTimetag: this message belongs to a precise instant.
        osc.sendMessage(m, true, applyLatencyToTimetag(scoped));
        return;
    }
    if(waitToSend){
        toSendBundle.addMessage(m);
    }else{
        osc.sendMessage(m, true, b_latency ? getNowTimetag(latency) : 1);
    }
}

void ofxSCServer::sendBundle(ofxOscBundle& b)
{
    if(toSendBundle.getMessageCount() > (1000-b.getMessageCount())) sendStoredBundle();
    const uint64_t scoped = getScopedTimetag();
    if(scoped != 0 && !waitToSend){
        osc.sendBundle(b, applyLatencyToTimetag(scoped));
        return;
    }
    if(waitToSend){
        for(int i = 0; i < b.getMessageCount(); i++){
            toSendBundle.addMessage(b.getMessageAt(i));
        }
    }else{
        osc.sendBundle(b, b_latency ? getNowTimetag(latency) : 1);
    }
}

void ofxSCServer::setWaitToSend(bool b){
    waitToSend = b;
    toSendBundle.clear();
}

bool ofxSCServer::getWaitToSend(){
    return waitToSend;
}

void ofxSCServer::sendStoredBundle(){
    osc.sendBundle(toSendBundle);
    toSendBundle.clear();
}

void ofxSCServer::addNodeListener(ofxSCNode* node){
    nodeFeedbackFunctions[node] = [node](ofxOscMessage &msg){
        if(node != nullptr && node->nodeID == msg.getArgAsInt(0))
            node->feedbackListener(msg);
    };
}

void ofxSCServer::removeNodeListener(ofxSCNode *node){
    nodeFeedbackFunctions.erase(node);
}

ofxSCServer::ScopedTimetag::ScopedTimetag(uint64_t timetag) : previous(scopedTimetagValue){
    scopedTimetagValue = timetag;
}

ofxSCServer::ScopedTimetag::~ScopedTimetag(){
    scopedTimetagValue = previous;
}

uint64_t ofxSCServer::getScopedTimetag(){
    return scopedTimetagValue;
}

uint64_t ofxSCServer::timetagNow(double offsetSeconds){
    const auto unixTime = std::chrono::system_clock::now().time_since_epoch();
    const int64_t nowUs = std::chrono::duration_cast<std::chrono::microseconds>(unixTime).count();
    return timetagFromUnixMicroseconds(nowUs + (int64_t)(offsetSeconds * 1000000.0));
}

uint64_t ofxSCServer::timetagForSteadyTimeUs(uint64_t steadyTimeUs){
    // steady_clock is monotonic but has an arbitrary epoch, so it is converted
    // through the offset between the two clocks, sampled now. Both clocks are
    // read back to back, which keeps the conversion error far below the
    // resolution that matters here.
    const int64_t steadyNowUs = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    const int64_t systemNowUs = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return timetagFromUnixMicroseconds(systemNowUs + ((int64_t)steadyTimeUs - steadyNowUs));
}

uint64_t ofxSCServer::applyLatencyToTimetag(uint64_t timetag) const {
    if(!b_latency || timetag <= 1) return timetag;
    // Fixed-point seconds: the whole part is seconds, the fraction 2^-32 s.
    const double offset = (double)latency;
    const uint64_t offsetFixed = (uint64_t)(offset * 4294967296.0);
    return timetag + offsetFixed;
}

void ofxSCServer::sendMsgAt(ofxOscMessage& m, uint64_t timetag){
    if(timetag <= 1){
        sendMsg(m);
        return;
    }
    if(waitToSend){
        // The graph is still being built; ordering matters more than timing.
        toSendBundle.addMessage(m);
        return;
    }
    osc.sendMessage(m, true, applyLatencyToTimetag(timetag));
}

void ofxSCServer::sendBundleAt(ofxOscBundle& b, uint64_t timetag){
    if(timetag <= 1){
        sendBundle(b);
        return;
    }
    if(waitToSend){
        for(int i = 0; i < b.getMessageCount(); i++) toSendBundle.addMessage(b.getMessageAt(i));
        return;
    }
    osc.sendBundle(b, applyLatencyToTimetag(timetag));
}

uint64_t ofxSCServer::getNowTimetag(float latency){
    const auto unixTime = std::chrono::system_clock::now().time_since_epoch();
    const int64_t nowUs = std::chrono::duration_cast<std::chrono::microseconds>(unixTime).count();
    return timetagFromUnixMicroseconds(nowUs + (int64_t)((double)latency * 1000000.0));
}
