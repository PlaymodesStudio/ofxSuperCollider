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
        server->controlBusses[index] = this;
        readValues.resize(this->channels, 0);
	}
	else
	{
		this->index = server->allocatorBusAudio->alloc(this->channels);
        server->audioBusses[index] = this;
	}
}

ofxSCBus::~ofxSCBus(){
//    free();
}

// Copy constructor
ofxSCBus::ofxSCBus(const ofxSCBus& other) {
    rate = other.rate;
    channels = other.channels;
    server = other.server;
}

// Copy assignment operator
ofxSCBus& ofxSCBus::operator=(const ofxSCBus& other) {
    if (this != &other) {
        free();  // Free existing resources

        rate = other.rate;
        channels = other.channels;
        server = other.server;
    }
    return *this;
}

// Move constructor
ofxSCBus::ofxSCBus(ofxSCBus&& other) noexcept
    : server(other.server), rate(other.rate), index(other.index), channels(other.channels), readValues(std::move(other.readValues)) {
    other.server = nullptr;
    other.rate = 0;
    other.index = 0;
    other.channels = 0;
}

// Move assignment operator
ofxSCBus& ofxSCBus::operator=(ofxSCBus&& other) noexcept {
    if (this != &other) {
        server = other.server;
        rate = other.rate;
        index = other.index;
        channels = other.channels;
        readValues = std::move(other.readValues);

        other.server = nullptr;
        other.rate = 0;
        other.index = 0;
        other.channels = 0;
    }
    return *this;
}

void ofxSCBus::set(float value)
{
    ofxOscMessage m;
    m.setAddress("/c_set");
    m.addIntArg(index);
    m.addIntArg(value);
    server->sendMsg(m);
}

void ofxSCBus::free()
{
	// nothing is actually allocated server-side,
	// so all we need to do here is reflect the availability of this address
    if (this->rate == RATE_CONTROL){
		server->allocatorBusControl->free(this->index);
        server->controlBusses[index] = NULL;
    }else{
        server->allocatorBusAudio->free(this->index);
        server->audioBusses[index] = NULL;
    }
}

void ofxSCBus::requestValues()
{
    ofxOscMessage m;
    m.setAddress("/c_get");
    for(int i = 0; i < channels; i++){
        m.addIntArg(index + i);
    }
    server->sendMsg(m);
}
