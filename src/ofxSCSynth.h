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
//#include <tr1/unordered_map>
#include <unordered_map>


#include "ofxSCNode.h"

typedef std::unordered_map<std::string, float> dictionary;

class ofxSCSynth : public ofxSCNode
{
public:	
	ofxSCSynth(std::string name = "sine", ofxSCServer *server = ofxSCServer::local());
	~ofxSCSynth();
    
    std::string getName() {return name;}

	ofxSCSynth (const ofxSCSynth & other) { copy (other); }
	ofxSCSynth& operator= (const ofxSCSynth & other) { return copy(other); }

	/// for operator= and copy constructor
	ofxSCSynth & copy(const ofxSCSynth & other);
	
	void create(int position = 0, int groupID = 1);
	void grain(int position = 0, int groupID = 1);
	
	void set(std::string arg, double value);
	void set(std::string arg, int value);
    void set(std::string arg, std::vector<float> values);
		
protected:

	std::string name;
	dictionary args;
};
