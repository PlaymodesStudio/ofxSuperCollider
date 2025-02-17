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

#define MILISECONDS_FROM_1900_to_1970 2208988800000ULL
#define TWO_TO_THE_32_OVER_ONE_MILLION 4295

#define MAX_NOTIFIES_WITHOUT_REPLY 12

ofxSCServer *ofxSCServer::plocal = NULL;

ofxSCServer::ofxSCServer(std::string hostname, unsigned int port)
{
	this->hostname = hostname;
	this->port = port;

    osc.setup(hostname, port, port+20);
    listener = ofEvents().update.newListener(this, &ofxSCServer::_process);
	
	allocatorBusAudio = new ofxSCResourceAllocator(65536);
	allocatorBusAudio->pos = 64;
	
	allocatorBusControl = new ofxSCResourceAllocator(4096);
	allocatorBuffer = new ofxSCResourceAllocator(4096);
	
	if (plocal == 0)
		plocal = this;
    
    waitToSend = false;
    
    latency = 0.2;
    b_latency = false;
    
    booted = false;
    initialized = false;
    numNotifies = 0;
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
    if(booted) numNotifies++;
	
//#ifdef _ofxOscSENDERRECEIVER_H

	while(osc.hasWaitingMessages())
	{
		ofxOscMessage m;
		osc.getNextMessage(m);
//		printf("** got OSC! %s\n", m.getAddress().c_str());
//        ofLog() << m;
        
        if (m.getAddress() == "/status.reply"){
            if(!booted){
                booted = true;
                serverBootedEvent.notify(this);
                numNotifies = 0;
            }else{
                if(numNotifies > MAX_NOTIFIES_WITHOUT_REPLY){
                    booted = false;
                    initialized = false;
                    ofLog() << "Detected killed server";
                }
                numNotifies--;
            }
        }
//        else if(m.getAddress())
//            ofLog() << m.getAddress();
//        }
		
		/*-----------------------------------------------------------------------------
		 * /done
		 *  - buffer read completed
		 /*---------------------------------------------------------------------------*/
		if (m.getAddress() == "/done")
		{
			std::string cmd = m.getArgAsString(0);
            if(cmd == "/d_loadDir"){
                initialized = true;
                serverInitializedEvent.notify(this);
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
        
        else if (m.getAddress() == "/c_set"){
            int firstIndex = m.getArgAsInt32(0);
            for(int i = 0; i < m.getNumArgs(); i+=2){
                int index = m.getArgAsInt32(i);
                if(controlBusses[firstIndex] != NULL){
                    controlBusses[firstIndex]->readValues[index-firstIndex] = m.getArgAsFloat(i+1);
                }
            }
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
