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

#include "ofxSCGroup.h"

void ofxSCGroup::create(int position, int groupID, bool parallel)
{
	nodeID = ofxSCNode::id_base++;
	getServer()->invalidateNodeIndex();
	
	ofxOscMessage m;
	
    if(parallel) m.setAddress("/p_new");
	else m.setAddress("/g_new");
	m.addIntArg(nodeID);
	m.addIntArg(position);
	m.addIntArg(groupID);
	
    // Sent as a bundle like ofxSCSynth::createAndRun: a bare message carries
    // the server's latency timetag while synth bundles run on arrival, so a
    // group sent with sendMsg would be placed after synths created after it.
    ofxOscBundle bundle;
    bundle.addMessage(m);
    getServer()->sendBundle(bundle);
	
	created = true;
}
