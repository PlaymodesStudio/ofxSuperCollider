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
typedef std::unordered_map<std::string, std::vector<float>> vecDictionary;
typedef std::unordered_map<std::string, std::string> strDictionary;
typedef std::unordered_map<std::string, std::vector<std::string>> vecStrDictionary;
typedef std::unordered_map<std::string, std::pair<int, int>> mapaDictionary;

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
    void set(std::string arg, std::vector<int> values);
    
    void setMultiple(std::string arg, float value, int quantity);
    void setMultiple(std::string arg, int value, int quantity);
    
    void mapa(std::string arg, int value);
    void mapan(std::string arg, int value, int quantity);
    
    ofxOscMessage setMessage(std::string arg, double value);
    ofxOscMessage setMessage(std::string arg, int value);
    ofxOscMessage setMessage(std::string arg, std::vector<float> values);
    ofxOscMessage setMessage(std::string arg, std::vector<int> values);
    
    void resendStoredArgs();
		
protected:

	std::string name;
	dictionary args;
    vecDictionary vecArgs;
    strDictionary strArgs;
    vecStrDictionary vecStrArgs;
    mapaDictionary mapaArgs;
    std::vector<ofxOscMessage> queuedMessages;
};
