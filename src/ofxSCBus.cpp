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

#include "ofxSCBus.h"


int ofxSCBus::id_base = 64;


ofxSCBus::ofxSCBus(int rate, int channels, ofxSCServer *server)
{
	this->rate = rate;
	this->channels = channels;
	this->server = server;	
	
	if (this->rate == RATE_CONTROL)
	{
		this->index = server->allocatorBusControl->alloc(this->channels);
        readValues.resize(this->channels, 0);
        // -1: the server's control busses are used up. Writing the tables at
        // [-1] corrupted the heap; the bus is simply unusable instead.
        if(index >= 0 && index < (int)server->controlBusses.size()){
            server->controlBusses[index] = this;
            server->setControlBusOwner(index, this->channels, this);
        }else{
            ofLogError("ofxSCBus") << "No room for " << this->channels << " control bus channels"
                                   << " (raise the server's control bus count)";
        }
	}
	else
	{
		this->index = server->allocatorBusAudio->alloc(this->channels);
        if(index >= 0 && index < (int)server->audioBusses.size()){
            server->audioBusses[index] = this;
        }else{
            ofLogError("ofxSCBus") << "No room for " << this->channels << " audio bus channels"
                                   << " (raise the server's audio bus count)";
        }
	}
}

ofxSCBus::~ofxSCBus(){
//    free();
}

// Copy constructor
// A copy describes the same kind of bus but owns no range (index -1): two
// owners would free it twice
ofxSCBus::ofxSCBus(const ofxSCBus& other) {
    rate = other.rate;
    channels = other.channels;
    server = other.server;
    index = -1;
}

// Copy assignment operator
ofxSCBus& ofxSCBus::operator=(const ofxSCBus& other) {
    if (this != &other) {
        free();  // Free existing resources

        rate = other.rate;
        channels = other.channels;
        server = other.server;
        index = -1;
    }
    return *this;
}

// Move constructor
ofxSCBus::ofxSCBus(ofxSCBus&& other) noexcept
    : server(other.server), rate(other.rate), index(other.index), channels(other.channels), readValues(std::move(other.readValues)) {
    retarget(&other);
    other.server = nullptr;
    other.rate = 0;
    other.index = -1;
    other.channels = 0;
}

// The server's tables point at the bus object: after a move, at this one
void ofxSCBus::retarget(ofxSCBus* from){
    if(server == nullptr || index < 0) return;
    if(rate == RATE_CONTROL){
        server->reassignControlBusOwner(index, channels, from, this);
        if(index < (int)server->controlBusses.size() && server->controlBusses[index] == from) server->controlBusses[index] = this;
    }else if(index < (int)server->audioBusses.size() && server->audioBusses[index] == from){
        server->audioBusses[index] = this;
    }
}

// Move assignment operator
ofxSCBus& ofxSCBus::operator=(ofxSCBus&& other) noexcept {
    if (this != &other) {
        server = other.server;
        rate = other.rate;
        index = other.index;
        channels = other.channels;
        readValues = std::move(other.readValues);
        retarget(&other);

        other.server = nullptr;
        other.rate = 0;
        other.index = -1;
        other.channels = 0;
    }
    return *this;
}

void ofxSCBus::set(float value)
{
    ofxOscMessage m;
    m.setAddress("/c_set");
    m.addIntArg(index);
    m.addFloatArg(value);   // was addIntArg: 0.7 arrived as 0
    server->sendMsg(m);
}

void ofxSCBus::free()
{
    // Never allocated (the server ran out), or already freed: nothing to give
    // back. Freeing twice put the range on the free list twice, and two later
    // busses then shared it.
    if(index < 0 || server == nullptr) return;
	// nothing is actually allocated server-side,
	// so all we need to do here is reflect the availability of this address
    if (this->rate == RATE_CONTROL){
        server->reassignControlBusOwner(index, channels, this, nullptr);
		server->allocatorBusControl->free(this->index);
        server->controlBusses[index] = NULL;
    }else{
        server->allocatorBusAudio->free(this->index);
        server->audioBusses[index] = NULL;
    }
    index = -1;
}

void ofxSCBus::requestValues()
{
    // Queued: the server sends every bus read of the frame in one /c_getn.
    if (this->rate == RATE_CONTROL){
        server->requestControlValues(index, channels);
        return;
    }
    ofxOscMessage m;
    m.setAddress("/c_get");
    for(int i = 0; i < channels; i++){
        m.addIntArg(index + i);
    }
    server->sendMsg(m);
}
