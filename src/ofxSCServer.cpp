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

#define MILISECONDS_FROM_1900_to_1970 2208988800000ULL
#define TWO_TO_THE_32_OVER_ONE_MILLION 4295

ofxSCServer *ofxSCServer::plocal = NULL;

ofxSCServer::ofxSCServer(std::string hostname, unsigned int port, unsigned int receivePort, unsigned int numInputs, unsigned int numOutputs, unsigned int numAudioBusses, unsigned int numControlBusses, unsigned int numBuffers)
{
	this->hostname = hostname;
	this->port = port;

    osc.setup(hostname, port, receivePort);
    listener = ofEvents().update.newListener(this, &ofxSCServer::_process);
	
	allocatorBusAudio = new ofxSCResourceAllocator(numAudioBusses);
	allocatorBusAudio->pos = numInputs + numOutputs;
	
	allocatorBusControl = new ofxSCResourceAllocator(numControlBusses);
	allocatorBuffer = new ofxSCResourceAllocator(numBuffers);
    
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
			std::string cmd = m.getArgAsString(0);
            if(cmd == "/d_loadDir"){
                initializing = false;
                serverInitializedEvent.notify(this);
//                ofLog() << "Server Initialized";
            }
//			int index = m.getArgAsInt32(1);
//			printf("** buffer read completed, ID %d\n", index);
//			buffers[index]->ready = true;
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
				
				if(firstIndex >= 0 && firstIndex < 4096 &&
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

void ofxSCServer::sendMsg(ofxOscMessage& m)
{
    if(toSendBundle.getMessageCount() > 1000) sendStoredBundle();
    if(waitToSend){
        toSendBundle.addMessage(m);
    }else{
        osc.sendMessage(m, true, b_latency ? getNowTimetag(latency) : 1);
    }
}

void ofxSCServer::sendBundle(ofxOscBundle& b)
{
    if(toSendBundle.getMessageCount() > (1000-b.getMessageCount())) sendStoredBundle();
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

uint64_t ofxSCServer::getNowTimetag(float latency){
    auto now = std::chrono::system_clock::now();
    auto unix_time = now.time_since_epoch();
    // Add latency (convert seconds to nanoseconds and add)
    auto latency_duration = std::chrono::duration<double>(latency);
    std::chrono::duration unix_time_mod = unix_time + std::chrono::duration_cast<std::chrono::nanoseconds>(latency_duration);
    
    //https://github.com/juce-framework/JUCE/blob/master/modules/juce_osc/osc/juce_OSCTimeTag.cpp
    const uint64_t milliseconds = (uint64_t) std::chrono::duration_cast<std::chrono::milliseconds>(unix_time_mod).count() + MILISECONDS_FROM_1900_to_1970;
    uint64_t seconds = milliseconds / 1000;
    uint32_t fractionalPart = uint32_t (4294967.296 * (milliseconds % 1000));
    
    return (seconds << 32) + fractionalPart;
}
