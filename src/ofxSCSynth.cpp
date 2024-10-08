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

#include "ofxSCSynth.h"

ofxSCSynth::ofxSCSynth(std::string name, ofxSCServer *server)
{
	ofxSCNode();
	
	this->name = name;
	this->server = server;
}

ofxSCSynth::~ofxSCSynth()
{
}

void ofxSCSynth::create(int position, int groupID)
{
	ofxOscBundle b;
	ofxOscMessage m;

	if (nodeID == 0)
		nodeID = ofxSCNode::id_base++;
	
	m.setAddress("/s_new");
	m.addStringArg(name.c_str());
	m.addIntArg(nodeID);
	m.addIntArg(position);
	m.addIntArg(groupID);
	b.addMessage(m);
	
	for (dictionary::const_iterator it = args.begin(); 
        it != args.end(); ++it)
	{
		std::string key = it->first;
		float value = it->second;

		m.clear();
		m.setAddress("/n_set");
		m.addIntArg(nodeID);
		m.addStringArg(key.c_str());
		m.addFloatArg(value);
		b.addMessage(m);
	}

	server->sendBundle(b);
	
	created = true;
}

void ofxSCSynth::grain(int position, int groupID)
{
	nodeID = -1;
	create(position, groupID);
}

void ofxSCSynth::set(std::string arg, double value)
{
	args.insert(dictionary::value_type(arg, value));
	
	if (created)
	{
		ofxOscMessage m;
		m.setAddress("/n_set");
		m.addIntArg(nodeID);
		m.addStringArg(arg);
		m.addFloatArg(value);
		
		server->sendMsg(m);
	}
}

void ofxSCSynth::set(std::string arg, int value)
{
	args.insert(dictionary::value_type(arg, value));
	
	if (created)
	{
		ofxOscMessage m;
		m.setAddress("/n_set");
		m.addIntArg(nodeID);
		m.addStringArg(arg);
		m.addIntArg(value);
		
		server->sendMsg(m);
	}
}

void ofxSCSynth::set(std::string arg, std::vector<float> values)
{
    //args.insert(dictionary::value_type(arg, values));
    
    if (created)
    {
        ofxOscMessage m;
        m.setAddress("/n_setn");
        m.addIntArg(nodeID);
        m.addStringArg(arg);
        m.addIntArg(values.size());
        for(auto &v : values) m.addFloatArg(v);
        
        server->sendMsg(m);
    }
}

void ofxSCSynth::set(std::string arg, std::vector<int> values)
{
    //args.insert(dictionary::value_type(arg, values));
    
    if (created)
    {
        ofxOscMessage m;
        m.setAddress("/n_setn");
        m.addIntArg(nodeID);
        m.addStringArg(arg);
        m.addIntArg(values.size());
        for(auto &v : values) m.addIntArg(v);
        
        server->sendMsg(m);
    }
}

void ofxSCSynth::setMultiple(std::string arg, float value, int quantity){
    if (created)
    {
        ofxOscMessage m;
        m.setAddress("/n_fill");
        m.addIntArg(nodeID);
        m.addStringArg(arg);
        m.addIntArg(quantity);
        m.addFloatArg(value);
        m.addFloatArg(value);

        server->sendMsg(m);
    }
}

void ofxSCSynth::setMultiple(std::string arg, int value, int quantity){
    if (created)
    {
        ofxOscMessage m;
        m.setAddress("/n_fill");
        m.addIntArg(nodeID);
        m.addStringArg(arg);
        m.addIntArg(quantity);
        m.addIntArg(value);
        m.addIntArg(value);
        
        server->sendMsg(m);
    }
}

void ofxSCSynth::mapa(std::string arg, int value){
    if (created)
    {
        ofxOscMessage m;
        m.setAddress("/n_mapa");
        m.addIntArg(nodeID);
        m.addStringArg(arg);
        m.addIntArg(value);
        
        server->sendMsg(m);
    }
}

void ofxSCSynth::mapan(std::string arg, int value, int quantity){
    if (created)
    {
        ofxOscMessage m;
        m.setAddress("/n_mapan");
        m.addIntArg(nodeID);
        m.addStringArg(arg);
        m.addIntArg(value);
        m.addIntArg(quantity);
        
        server->sendMsg(m);
    }
}

ofxOscMessage ofxSCSynth::setMessage(std::string arg, double value)
{
    ofxOscMessage m;
    m.setAddress("/n_set");
    m.addIntArg(nodeID);
    m.addStringArg(arg);
    m.addFloatArg(value);
    
    return m;
}

ofxOscMessage ofxSCSynth::setMessage(std::string arg, int value)
{
    ofxOscMessage m;
    m.setAddress("/n_set");
    m.addIntArg(nodeID);
    m.addStringArg(arg);
    m.addIntArg(value);
    
    return m;
}

ofxOscMessage ofxSCSynth::setMessage(std::string arg, std::vector<float> values)
{
    ofxOscMessage m;
    m.setAddress("/n_setn");
    m.addIntArg(nodeID);
    m.addStringArg(arg);
    m.addIntArg(values.size());
    for(auto &v : values) m.addFloatArg(v);
    
    return m;
}

ofxOscMessage ofxSCSynth::setMessage(std::string arg, std::vector<int> values)
{
    ofxOscMessage m;
    m.setAddress("/n_setn");
    m.addIntArg(nodeID);
    m.addStringArg(arg);
    m.addIntArg(values.size());
    for(auto &v : values) m.addIntArg(v);
    
    return m;
}
