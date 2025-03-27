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

ofxSCNode::ofxSCNode()
{
	nodeID = 0;
	created = false;
    server = nullptr;
    setServer(ofxSCServer::local());
}

ofxSCNode::~ofxSCNode()
{
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
    server->sendMsg(m);
}

void ofxSCNode::free()
{
	ofxOscMessage m;
	m.setAddress("/n_free");
	m.addIntArg(nodeID);
	server->sendMsg(m);
	
	created = false;
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
    
    server->sendMsg(m);
}

void ofxSCNode::order(int position, std::vector<int> groupIDs)
{
    ofxOscMessage m;
    
    m.setAddress("/n_order");
    m.addIntArg(position);
    for(int groupID : groupIDs) m.addIntArg(groupID);
    m.addIntArg(nodeID);
    
    server->sendMsg(m);
}

void ofxSCNode::moveBefore(int _nodeID){
    ofxOscMessage m;
    
    m.setAddress("/n_before");
    m.addIntArg(nodeID);
    m.addIntArg(_nodeID);
    
    server->sendMsg(m);
}

void ofxSCNode::moveAfter(int _nodeID){
    ofxOscMessage m;
    
    m.setAddress("/n_after");
    m.addIntArg(nodeID);
    m.addIntArg(_nodeID);
    
    server->sendMsg(m);
}

void ofxSCNode::setServer(ofxSCServer *_server){
    if(server != nullptr)
        server->removeNodeListener(this);

    server = _server;
    server->addNodeListener(this);
}

ofxSCServer* ofxSCNode::getServer(){
    return server;
}
