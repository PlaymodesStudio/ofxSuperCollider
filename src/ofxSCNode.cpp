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

#include "ofxSCNode.h"
#include "ofxSCGroup.h"

int ofxSCNode::id_base = 2000;

ofxSCNode::ofxSCNode(ofxSCServer *_server)
{
	nodeID = 0;
	created = false;
    server = nullptr;
    setServer(_server);
}

ofxSCNode::~ofxSCNode()
{
    setServer(nullptr);
}

void ofxSCNode::addToHead(ofxSCGroup group)
{
	this->create(0, group.nodeID);
}

void ofxSCNode::addToTail(ofxSCGroup group)
{
	this->create(1, group.nodeID);
}

void ofxSCNode::run(bool b){
    ofxOscMessage m;
    m.setAddress("/n_run");
    m.addIntArg(nodeID);
    m.addIntArg(b ? 1 : 0);
    
    if(created || server->getBLatency()){
        server->sendMsg(m);
    }else{
        storedMessages.push_back(m);
    }
}

void ofxSCNode::free()
{
	ofxOscMessage m;
	m.setAddress("/n_free");
	m.addIntArg(nodeID);
	server->sendMsg(m);
	
//	created = false;
}

void ofxSCNode::create(int position, int groupID)
{
}

void ofxSCNode::order(int position, int groupID)
{
    ofxOscMessage m;
    
    m.setAddress("/n_order");
    m.addIntArg(position);
    m.addIntArg(groupID);
    m.addIntArg(nodeID);
    
    if(created || server->getBLatency()){
        server->sendMsg(m);
    }else{
        storedMessages.push_back(m);
    }
}

void ofxSCNode::order(int position, std::vector<int> groupIDs)
{
    ofxOscMessage m;
    
    m.setAddress("/n_order");
    m.addIntArg(position);
    for(int groupID : groupIDs) m.addIntArg(groupID);
    m.addIntArg(nodeID);
    
    if(created || server->getBLatency()){
        server->sendMsg(m);
    }else{
        storedMessages.push_back(m);
    }
}

void ofxSCNode::moveBefore(int _nodeID){
    
    ofxOscMessage m;
    
    m.setAddress("/n_before");
    m.addIntArg(nodeID);
    m.addIntArg(_nodeID);
    
    if(created || server->getBLatency()){
        server->sendMsg(m);
    }else{
        storedMessages.push_back(m);
    }
}

void ofxSCNode::moveAfter(int _nodeID){
    ofxOscMessage m;
    
    m.setAddress("/n_after");
    m.addIntArg(nodeID);
    m.addIntArg(_nodeID);
    
    if(created || server->getBLatency()){
        server->sendMsg(m);
    }else{
        storedMessages.push_back(m);
    }
}

void ofxSCNode::feedbackListener(ofxOscMessage &msg){
    if(msg.getAddress() == "/n_go"){
        created = true;
        for(auto &m : storedMessages) server->sendMsg(m);
        storedMessages.clear();
        resendStoredArgs();
    }else if(msg.getAddress() == "/n_end"){
        created = false;
    }else if(msg.getAddress() == "/n_off"){
        
    }else if(msg.getAddress() == "/n_on"){
        
    }else if(msg.getAddress() == "/n_move"){
        
    }else if(msg.getAddress() == "/n_info"){
        
	}else if(msg.getAddress() == "/tr"){
		
    }else{
        newFeedbackMessage.notify(msg);
    }
}

void ofxSCNode::setServer(ofxSCServer *_server){
    if(server != nullptr)
        server->removeNodeListener(this);

    server = _server;
    if(server != nullptr)
        server->addNodeListener(this);
}

ofxSCServer* ofxSCNode::getServer(){
    return server;
}
