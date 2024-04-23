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

#include "ofxSuperCollider.h"
#include "ofxSCServer.h"


class ofxSCBus
{
	
public:
	ofxSCBus(int rate = RATE_AUDIO, int channels = 2, ofxSCServer *server = ofxSCServer::local());
    ~ofxSCBus();
    
    // Copy constructor
    ofxSCBus(const ofxSCBus& other);

    // Copy assignment operator
    ofxSCBus& operator=(const ofxSCBus& other);

    // Move constructor
    ofxSCBus(ofxSCBus&& other) noexcept;

    // Move assignment operator
    ofxSCBus& operator=(ofxSCBus&& other) noexcept;
	
    void set(float value);
	void free();
    void requestValues();
	
	static int id_base;
	
	ofxSCServer *server;
	int rate;
	int index;
	int channels;
    
    std::vector<float> readValues;
};
