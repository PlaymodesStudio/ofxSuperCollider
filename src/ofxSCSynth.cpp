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

ofxSCSynth::ofxSCSynth(std::string name, ofxSCServer *_server) : ofxSCNode(_server)
{
	this->name = name;
}

ofxSCSynth::~ofxSCSynth()
{
    //If we for example created and freed fast a synth, we want the free message to arrive to the server
    resendStoredArgs();
}

void ofxSCSynth::create(int position, int groupID)
{
	ofxOscMessage m;

	if (nodeID == 0)
		nodeID = ofxSCNode::id_base++;
	
	m.setAddress("/s_new");
	m.addStringArg(name.c_str());
	m.addIntArg(nodeID);
	m.addIntArg(position);
	m.addIntArg(groupID);
	
	for (dictionary::const_iterator it = args.begin(); 
        it != args.end(); ++it)
	{
		std::string key = it->first;
		float value = it->second;

		m.addStringArg(key.c_str());
		m.addFloatArg(value);
	}
    args.clear();
    
    for (vecDictionary::const_iterator it = vecArgs.begin();
         it != vecArgs.end(); ++it)
    {
        std::string key = it->first;
        std::vector<float> value = it->second;
        
        m.addStringArg(key.c_str());
        m.addCharArg('[');
        for(auto &v : value) m.addFloatArg(v);
        m.addCharArg(']');
    }
    vecArgs.clear();
    
    for (strDictionary::const_iterator it = strArgs.begin();
         it != strArgs.end(); ++it)
    {
        std::string key = it->first;
        std::string value = it->second;
        
        m.addStringArg(key.c_str());
        m.addStringArg(value.c_str());
    }
    strArgs.clear();
    
    for (vecStrDictionary::const_iterator it = vecStrArgs.begin();
         it != vecStrArgs.end(); ++it)
    {
        std::string key = it->first;
        std::vector<std::string> value = it->second;
        
        m.addStringArg(key.c_str());
        m.addCharArg('[');
        for(auto &v : value) m.addStringArg(v);
        m.addCharArg(']');
    }
    vecStrArgs.clear();
    
    for (mapaDictionary::const_iterator it = mapaArgs.begin();
         it != mapaArgs.end(); it++)
    {
        std::string key = it->first;
        std::pair<int, int> value = it->second;
        
        m.addStringArg(key.c_str());
        m.addCharArg('[');
        for(int i = 0; i < value.second; i++) m.addStringArg("a" + ofToString(value.first + i));
        m.addCharArg(']');
    }
    mapaArgs.clear();
    
    getServer()->sendMsg(m);
}

void ofxSCSynth::createAndRun(int position, int groupID, bool run){
    ofxOscMessage m;

    //TODO: Reuse nodeIDs
    if (nodeID == 0)
        nodeID = ofxSCNode::id_base++;
    
    m.setAddress("/s_new");
    m.addStringArg(name.c_str());
    m.addIntArg(nodeID);
    m.addIntArg(position);
    m.addIntArg(groupID);
    
    for (dictionary::const_iterator it = args.begin();
        it != args.end(); ++it)
    {
        std::string key = it->first;
        float value = it->second;

        m.addStringArg(key.c_str());
        m.addFloatArg(value);
    }
    args.clear();
    
    for (vecDictionary::const_iterator it = vecArgs.begin();
         it != vecArgs.end(); ++it)
    {
        std::string key = it->first;
        std::vector<float> value = it->second;
        
        m.addStringArg(key.c_str());
        m.addCharArg('[');
        for(auto &v : value) m.addFloatArg(v);
        m.addCharArg(']');
    }
    vecArgs.clear();
    
    for (strDictionary::const_iterator it = strArgs.begin();
         it != strArgs.end(); ++it)
    {
        std::string key = it->first;
        std::string value = it->second;
        
        m.addStringArg(key.c_str());
        m.addStringArg(value.c_str());
    }
    strArgs.clear();
    
    for (vecStrDictionary::const_iterator it = vecStrArgs.begin();
         it != vecStrArgs.end(); ++it)
    {
        std::string key = it->first;
        std::vector<std::string> value = it->second;
        
        m.addStringArg(key.c_str());
        m.addCharArg('[');
        for(auto &v : value) m.addStringArg(v);
        m.addCharArg(']');
    }
    vecStrArgs.clear();
    
    for (mapaDictionary::const_iterator it = mapaArgs.begin();
         it != mapaArgs.end(); it++)
    {
        std::string key = it->first;
        std::pair<int, int> value = it->second;
        
        m.addStringArg(key.c_str());
        m.addCharArg('[');
        for(int i = 0; i < value.second; i++) m.addStringArg("a" + ofToString(value.first + i));
        m.addCharArg(']');
    }
    mapaArgs.clear();
    
    ofxOscMessage m2;
    m2.setAddress("/n_run");
    m2.addIntArg(nodeID);
    m2.addIntArg(run ? 1 : 0);
    
    ofxOscBundle bundle;
    bundle.addMessage(m);
    bundle.addMessage(m2);
    
    getServer()->sendBundle(bundle);
}

void ofxSCSynth::grain(int position, int groupID)
{
	nodeID = -1;
	create(position, groupID);
}

void ofxSCSynth::set(std::string arg, double value)
{
	if (created)
	{
		ofxOscMessage m;
		m.setAddress("/n_set");
		m.addIntArg(nodeID);
		m.addStringArg(arg);
		m.addFloatArg(value);
		
        getServer()->sendMsg(m);
	}
    else
    {
        args[arg] = value;
    }
}

void ofxSCSynth::set(std::string arg, int value)
{
	
	if (created)
	{
		ofxOscMessage m;
		m.setAddress("/n_set");
		m.addIntArg(nodeID);
		m.addStringArg(arg);
		m.addIntArg(value);
		
        getServer()->sendMsg(m);
	}
    else
    {
        args[arg] = value;
    }
}

void ofxSCSynth::set(std::string arg, std::vector<float> values)
{
   
    if (created)
    {
        ofxOscMessage m;
        m.setAddress("/n_setn");
        m.addIntArg(nodeID);
        m.addStringArg(arg);
        m.addIntArg(values.size());
        for(auto &v : values) m.addFloatArg(v);
        
        getServer()->sendMsg(m);
	}
    else
    {
        vecArgs[arg] = values;
    }
}

void ofxSCSynth::set(std::string arg, std::vector<int> values)
{
    if (created)
    {
        ofxOscMessage m;
        m.setAddress("/n_setn");
        m.addIntArg(nodeID);
        m.addStringArg(arg);
        m.addIntArg(values.size());
        for(auto &v : values) m.addIntArg(v);
        
        getServer()->sendMsg(m);
	}
    else
    {
        vecArgs[arg] = std::vector<float>(values.begin(), values.end());
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

		getServer()->sendMsg(m);
	}
    else
    {
        vecArgs[arg] = std::vector<float>(quantity, value);
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
        
		getServer()->sendMsg(m);
	}
    else
    {
        vecArgs[arg] = std::vector<float>(quantity, value);
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
        
		getServer()->sendMsg(m);
	}
    else
    {
        mapaArgs[arg] = std::make_pair(value, 1);
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
        
		getServer()->sendMsg(m);
	}
    else
    {
        mapaArgs[arg] = std::make_pair(value, quantity);
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

void ofxSCSynth::resendStoredArgs(){
    ofxOscMessage m;
    ofxOscMessage m_mapa;
    ofxOscMessage m_mapan;

    m.setAddress("/n_set");
    m.addIntArg(nodeID);

    m_mapa.setAddress("/n_mapa");
    m_mapa.addIntArg(nodeID);

    m_mapan.setAddress("/n_mapan");
    m_mapan.addIntArg(nodeID);

    // Scalar float/int args → /n_set  key value pairs
    for (dictionary::const_iterator it = args.begin();
        it != args.end(); ++it)
    {
        std::string key = it->first;
        float value = it->second;

        m.addStringArg(key.c_str());
        m.addFloatArg(value);
    }
    args.clear();

    // Vector float args → individual /n_setn messages.
    // addCharArg('['/']') produces OSC type tag 'c' (a single char value),
    // which SuperCollider's /n_set parser does NOT recognise as an array
    // delimiter — it expects literal '[' ']' type tags in the type-tag string.
    // /n_setn (nodeID, controlName, count, v0, v1, ...) is the correct,
    // unambiguous SC command for setting a run of consecutive controls.
    std::vector<ofxOscMessage> setnMessages;
    for (vecDictionary::const_iterator it = vecArgs.begin();
         it != vecArgs.end(); ++it)
    {
        std::string key = it->first;
        std::vector<float> value = it->second;

        ofxOscMessage mSetn;
        mSetn.setAddress("/n_setn");
        mSetn.addIntArg(nodeID);
        mSetn.addStringArg(key.c_str());
        mSetn.addIntArg((int)value.size());
        for(auto &v : value) mSetn.addFloatArg(v);
        setnMessages.push_back(std::move(mSetn));
    }
    vecArgs.clear();

    for (strDictionary::const_iterator it = strArgs.begin();
         it != strArgs.end(); ++it)
    {
        std::string key = it->first;
        std::string value = it->second;

        m.addStringArg(key.c_str());
        m.addStringArg(value.c_str());
    }
    strArgs.clear();

    for (vecStrDictionary::const_iterator it = vecStrArgs.begin();
         it != vecStrArgs.end(); ++it)
    {
        std::string key = it->first;
        std::vector<std::string> value = it->second;

        m.addStringArg(key.c_str());
        m.addCharArg('[');
        for(auto &v : value) m.addStringArg(v);
        m.addCharArg(']');
    }
    vecStrArgs.clear();

    for (mapaDictionary::const_iterator it = mapaArgs.begin();
         it != mapaArgs.end(); it++)
    {
        std::string key = it->first;
        std::pair<int, int> value = it->second;

        if(value.second == 1){
            m_mapa.addStringArg(key.c_str());
            m_mapa.addIntArg(value.second);
        }else{
            m_mapan.addStringArg(key.c_str());
            m_mapan.addIntArg(value.first);
            m_mapan.addIntArg(value.second);
        }
    }
    mapaArgs.clear();

    ofxOscBundle b;
    if(m.getNumArgs() > 1) b.addMessage(m);
    for(auto& sm : setnMessages) b.addMessage(sm);
    if(m_mapa.getNumArgs() > 1) b.addMessage(m_mapa);
    if(m_mapan.getNumArgs() > 1) b.addMessage(m_mapan);

    if(b.getMessageCount() > 0){
        getServer()->sendBundle(b);
    }
}
