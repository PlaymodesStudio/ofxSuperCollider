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

#include "ofxSCServer.h"
#include "ofxSCBuffer.h"
#include "ofxOsc.h"
#include "ofxSCNode.h"
#include <algorithm>
#include <set>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <cmath>
#include <utility>

#define MILISECONDS_FROM_1900_to_1970 2208988800000ULL
#define SECONDS_FROM_1900_TO_1970 2208988800ULL
#define TWO_TO_THE_32_OVER_ONE_MILLION 4295

#define INTIALIZATION_ID 1917  //Init with numbers
#define NRT_SYNC_ID 1918       //Answered once the NRT setup's async commands are done

namespace {
// One per thread: several servers may be driven from the same scheduling
// scope, and a scope must not leak into another thread's sends.
thread_local uint64_t scopedTimetagValue = 0;

// NTP timetag from a Unix epoch time in microseconds. Microsecond resolution
// matters: the previous millisecond rounding was already a third of an audio
// control block, which is most of what timestamping is meant to remove.
uint64_t timetagFromUnixMicroseconds(int64_t unixMicroseconds){
    if(unixMicroseconds < 0) return 1;
    const uint64_t seconds = (uint64_t)(unixMicroseconds / 1000000) + SECONDS_FROM_1900_TO_1970;
    const uint64_t microseconds = (uint64_t)(unixMicroseconds % 1000000);
    const uint32_t fractionalPart = (uint32_t)((microseconds * 4294967296ULL) / 1000000ULL);
    return (seconds << 32) + fractionalPart;
}
}

ofxSCServer *ofxSCServer::plocal = NULL;

ofxSCServer::ofxSCServer(std::string hostname, unsigned int port, unsigned int receivePort, unsigned int numInputs, unsigned int numOutputs, unsigned int numAudioBusses, unsigned int numControlBusses, unsigned int numBuffers)
{
	this->hostname = hostname;
	this->port = port;
	this->numInputs = numInputs;
	this->numOutputs = numOutputs;
	this->numAudioBusses = numAudioBusses;
	this->numControlBusses = numControlBusses;
	this->numBuffers = numBuffers;

    osc.setup(hostname, port, receivePort);
    listener = ofEvents().update.newListener(this, &ofxSCServer::_process);
	
	allocatorBusAudio = new ofxSCResourceAllocator(numAudioBusses);
	allocatorBusAudio->pos = numInputs + numOutputs;
	
	allocatorBusControl = new ofxSCResourceAllocator(numControlBusses);
	allocatorBuffer = new ofxSCResourceAllocator(numBuffers);
	allocatorSynth = nullptr;
    
    audioBusses.resize(numAudioBusses);
    controlBusses.resize(numControlBusses);
    buffers.resize(numBuffers);
	
	if (plocal == 0)
		plocal = this;
    
    waitToSend = false;
    
    latency = 0.2;
    b_latency = false;
    
    initializing = false;
}

ofxSCServer::~ofxSCServer()
{
}

ofxSCServer *ofxSCServer::local()
{
	if (plocal == 0)
	{
		plocal = new ofxSCServer();
	}
	
	return plocal;
}

// dummy method for oF event notification system
void ofxSCServer::_process(ofEventArgs &e)
{
	this->process();
}

void ofxSCServer::process()
{
    ofxOscMessage m;
    m.setAddress("/status");
    osc.sendMessage(m);
    
//#ifdef _ofxOscSENDERRECEIVER_H

	while(osc.hasWaitingMessages())
	{
		ofxOscMessage m;
		osc.getNextMessage(m);
//		printf("** got OSC! %s\n", m.getAddress().c_str());
//        ofLog() << m;
        
        if (m.getAddress() == "/status.reply"){
            int numSynths   = m.getArgAsInt(2);
            int numGroups   = m.getArgAsInt(3);
            int numSynthDefs = m.getArgAsInt(4);
            
            if(!initializing && numGroups == 1 && numSynthDefs == 0 && numSynths == 0){ //Server rebooted
                resetAllocators();
                serverBootedEvent.notify(this);
                initializing = true;
//                ofLog() << "Server Booted";
            }
        }
		
		/*-----------------------------------------------------------------------------
		 * /done
		 *  - buffer read completed
		 /*---------------------------------------------------------------------------*/
		else if (m.getAddress() == "/done")
		{
//			int index = m.getArgAsInt32(1);
//			printf("** buffer read completed, ID %d\n", index);
//			buffers[index]->ready = true;
		}
        
        else if (m.getAddress() == "/synced")
        {
            int id = m.getArgAsInt(0);
            if(id == INTIALIZATION_ID && initializing){
                initializing = false;
                serverInitializedEvent.notify(this);
//                ofLog() << "Server Initialized";
            }
            else if(id == NRT_SYNC_ID){
                nrtSyncPending = false;
            }
        }

		/*-----------------------------------------------------------------------------
		 * /b_info
		 *  - information on buffer size and channels
		/*---------------------------------------------------------------------------*/
		else if (m.getAddress() == "/b_info")
		{
			int index = m.getArgAsInt32(0);
			buffers[index]->frames = m.getArgAsInt32(1);
			buffers[index]->channels = m.getArgAsInt32(2);
			buffers[index]->sampleRate = m.getArgAsFloat(3);
			buffers[index]->ready = true;			
		}
		
		// buffer alloc/read failed
		else if (m.getAddress() == "/fail")
		{
		}
        
        else if (m.getAddress() == "/d_removed") //What it does? just one string argument.
        {
        }
        
		else if (m.getAddress() == "/c_set"){
			int firstIndex = m.getArgAsInt32(0);
			for(int i = 0; i < m.getNumArgs(); i+=2){
				int index = m.getArgAsInt32(i);
				int arrayIndex = index - firstIndex;
				
				if(firstIndex >= 0 && firstIndex < (int)controlBusses.size() &&
				   arrayIndex >= 0 &&
				   controlBusses[firstIndex] != NULL) {
					
					try {
						ofxSCBus* bus = controlBusses[firstIndex];
						// Add corruption check
						if(bus->channels > 0 &&
						   arrayIndex < bus->readValues.size()) {
							bus->readValues[arrayIndex] = m.getArgAsFloat(i+1);
						}
					} catch(...) {
						// Skip corrupted bus data
					}
				}
			}
		}
        else if (m.getAddress() == "/g_queryTree.reply"){
            queryTreeReplyEvent.notify(m);
        }
        
        //Node Notifications from server (n_go, n_end.., ugen notifications)
        //And Poll replies from synths
        else{
            for(auto &nff : nodeFeedbackFunctions) nff.second(m);
        }
	}
	
//#else
//
//	fprintf(stderr, "This version of ofxOsc does not have support for sender/receive objects. Please update to enable receiving responses from SuperCollider.\n");
//
//#endif
	
}

void ofxSCServer::notify()
{
	ofxOscMessage m;
	m.setAddress("/notify");
	m.addIntArg(1);
	osc.sendMessage(m, true);
}

void ofxSCServer::resetAllocators()
{
	if(allocatorBusAudio != nullptr){
		allocatorBusAudio->reset(numInputs + numOutputs);
	}
	if(allocatorBusControl != nullptr){
		allocatorBusControl->reset(0);
	}
	if(allocatorBuffer != nullptr){
		allocatorBuffer->reset(0);
	}
	if(allocatorSynth != nullptr){
		allocatorSynth->reset(0);
	}

	std::fill(audioBusses.begin(), audioBusses.end(), nullptr);
	std::fill(controlBusses.begin(), controlBusses.end(), nullptr);
	std::fill(buffers.begin(), buffers.end(), nullptr);
}

void ofxSCServer::requestNRTSync(){
    nrtSyncPending = true;
    ofxOscMessage m;
    m.setAddress("/sync");
    m.addIntArg(NRT_SYNC_ID);
    sendMsg(m);
}

void ofxSCServer::sendInitializationSyncMessage(){
    ofxOscMessage m3;
    m3.setAddress("/sync");
    m3.addIntArg(INTIALIZATION_ID);
    sendMsg(m3);
}

void ofxSCServer::sendMsg(ofxOscMessage& m)
{
    if(toSendBundle.getMessageCount() > 1000) sendStoredBundle();
    if(nrtCapturing && !waitToSend) captureNRTMessage(m);
    if(nrtCapturing && nrtCaptureOnly) return;
    const uint64_t scoped = getScopedTimetag();
    if(scoped != 0 && !waitToSend){
        // Inside a ScopedTimetag: this message belongs to a precise instant.
        osc.sendMessage(m, true, applyLatencyToTimetag(scoped));
        return;
    }
    if(waitToSend){
        toSendBundle.addMessage(m);
    }else{
        osc.sendMessage(m, true, b_latency ? getNowTimetag(latency) : 1);
    }
}

void ofxSCServer::sendBundle(ofxOscBundle& b)
{
    if(toSendBundle.getMessageCount() > (1000-b.getMessageCount())) sendStoredBundle();
    if(nrtCapturing && !waitToSend) captureNRTBundle(b);
    if(nrtCapturing && nrtCaptureOnly) return;
    const uint64_t scoped = getScopedTimetag();
    if(scoped != 0 && !waitToSend){
        osc.sendBundle(b, applyLatencyToTimetag(scoped));
        return;
    }
    if(waitToSend){
        for(int i = 0; i < b.getMessageCount(); i++){
            toSendBundle.addMessage(b.getMessageAt(i));
        }
    }else{
        osc.sendBundle(b, b_latency ? getNowTimetag(latency) : 1);
    }
}

void ofxSCServer::setWaitToSend(bool b){
    waitToSend = b;
    toSendBundle.clear();
}

bool ofxSCServer::getWaitToSend(){
    return waitToSend;
}

void ofxSCServer::sendStoredBundle(){
    if(nrtCapturing && toSendBundle.getMessageCount() > 0) captureNRTBundle(toSendBundle);
    if(nrtCapturing && nrtCaptureOnly){
        toSendBundle.clear();
        return;
    }
    osc.sendBundle(toSendBundle);
    toSendBundle.clear();
}

void ofxSCServer::beginNRTCapture(bool captureOnly){
    nrtCapturing = true;
    nrtCaptureOnly = captureOnly;
    nrtCaptureSuspended = false;
    nrtEventsSuppressed = false;
    nrtTimeProviderEnabled = false;
    nrtTime = 0.0;
    nrtEndTime = -1.0;
    nrtEvents.clear();
    nrtCreatedNodeIDs.clear();
    toSendBundle.clear();
}

void ofxSCServer::endNRTCapture(double endTime){
    if(toSendBundle.getMessageCount() > 0) sendStoredBundle();
    nrtEndTime = endTime;
    nrtCapturing = false;
    nrtTimeProviderEnabled = false;
    nrtCaptureOnly = true;
    nrtCaptureSuspended = false;
    nrtSyncPending = false;
    nrtEventsSuppressed = false;
    nrtCreatedNodeIDs.clear();
}

void ofxSCServer::clearNRTScore(){
    nrtEvents.clear();
    nrtEndTime = -1.0;
    nrtCreatedNodeIDs.clear();
}

void ofxSCServer::setNRTTime(double seconds){
    nrtTime = std::max(0.0, seconds);
}

void ofxSCServer::setNRTTimeProvider(std::function<double()> provider){
    nrtTimeProvider = std::move(provider);
}

void ofxSCServer::setNRTTimeProviderEnabled(bool enabled){
    nrtTimeProviderEnabled = enabled;
}

double ofxSCServer::getNRTEventTime() const{
    if(nrtTimeProviderEnabled && nrtTimeProvider){
        return std::max(0.0, nrtTimeProvider());
    }
    return std::max(0.0, nrtTime);
}

bool ofxSCServer::shouldCaptureNRTAddress(const std::string& address) const{
    // A query asks the server to reply. Offline there is nobody to reply to,
    // so every one of these is dead weight -- and a node polling a control bus
    // each frame can easily outnumber the actual music in the score by a
    // hundred to one.
    static const std::set<std::string> ignored = {
        "/status", "/notify", "/sync", "/dumpOSC", "/quit", "/g_queryTree",
        "/g_dumpTree", "/version",
        "/c_get", "/c_getn", "/b_get", "/b_getn", "/b_query",
        "/n_query", "/s_get", "/s_getn", "/u_query"
    };
    return ignored.count(address) == 0;
}

bool ofxSCServer::isTransientNRTMessage(const ofxOscMessage& message) const{
    const std::string address = message.getAddress();

    if(address == "/u_cmd"){
        // A unit command's selector is its first string argument. VSTPlugin's
        // /midi_msg and its relatives are events; /open, /program_read and
        // /set are state.
        for(std::size_t i = 0; i < message.getNumArgs(); i++){
            if(message.getArgType(i) != OFXOSC_TYPE_STRING) continue;
            return message.getArgAsString(i).rfind("/midi", 0) == 0;
        }
        return false;
    }

    if(address == "/n_set" || address == "/n_setn" || address == "/n_fill"){
        // SuperCollider names trigger-rate controls with a t_ prefix; setting
        // one fires it rather than storing a value.
        for(std::size_t i = 0; i < message.getNumArgs(); i++){
            if(message.getArgType(i) != OFXOSC_TYPE_STRING) continue;
            if(message.getArgAsString(i).rfind("t_", 0) == 0) return true;
        }
    }
    return false;
}

bool ofxSCServer::shouldCaptureNRTMessage(const ofxOscMessage& message){
    if(nrtEventsSuppressed && isTransientNRTMessage(message)) return false;

    const std::string address = message.getAddress();

    // Rebuilding the live graph can resend the same creation command several
    // times while NRT capture is active. scsynth rejects a second /s_new or
    // /g_new with an existing node ID, so keep only the first creation until
    // the score explicitly frees that node.
    if(address == "/s_new" && message.getNumArgs() > 1){
        const int nodeID = message.getArgAsInt(1);
        return nrtCreatedNodeIDs.insert(nodeID).second;
    }
    if(address == "/g_new" && message.getNumArgs() > 0){
        const int nodeID = message.getArgAsInt(0);
        return nrtCreatedNodeIDs.insert(nodeID).second;
    }
    if(address == "/n_free" && message.getNumArgs() > 0){
        nrtCreatedNodeIDs.erase(message.getArgAsInt(0));
    }else if(address == "/g_free" && message.getNumArgs() > 0){
        nrtCreatedNodeIDs.erase(message.getArgAsInt(0));
    }else if(address == "/g_freeAll"){
        nrtCreatedNodeIDs.clear();
    }
    return true;
}

void ofxSCServer::captureNRTMessage(const ofxOscMessage& message){
    if(nrtCaptureSuspended) return;
    NRTEvent event;
    event.time = getNRTEventTime();
    if(!appendNRTMessage(event.bundle, message)) return;
    nrtEvents.push_back(std::move(event));
}

void ofxSCServer::captureNRTBundle(const ofxOscBundle& bundle){
    if(nrtCaptureSuspended) return;
    if(bundle.getMessageCount() == 0 && bundle.getBundleCount() == 0) return;
    NRTEvent event;
    event.time = getNRTEventTime();
    appendNRTBundleContents(event.bundle, bundle);
    if(event.bundle.getMessageCount() > 0 || event.bundle.getBundleCount() > 0){
        nrtEvents.push_back(std::move(event));
    }
}

bool ofxSCServer::appendNRTMessage(ofxOscBundle& destination, const ofxOscMessage& message){
    if(!shouldCaptureNRTAddress(message.getAddress()) || !shouldCaptureNRTMessage(message)) return false;
    destination.addMessage(message);
    return true;
}

void ofxSCServer::appendNRTBundleContents(ofxOscBundle& destination, const ofxOscBundle& source){
    for(std::size_t i = 0; i < source.getMessageCount(); i++){
        appendNRTMessage(destination, source.getMessageAt(i));
    }
    for(std::size_t i = 0; i < source.getBundleCount(); i++){
        ofxOscBundle nested;
        appendNRTBundleContents(nested, source.getBundleAt(i));
        if(nested.getMessageCount() > 0 || nested.getBundleCount() > 0){
            destination.addBundle(nested);
        }
    }
}

int ofxSCServer::findNRTNodeID(const std::string& defName) const{
    int found = -1;
    for(const auto& event : nrtEvents){
        const ofxOscBundle& bundle = event.bundle;
        for(std::size_t i = 0; i < bundle.getMessageCount(); i++){
            const ofxOscMessage& message = bundle.getMessageAt(i);
            if(message.getAddress() != "/s_new") continue;
            if(message.getNumArgs() < 2) continue;
            if(message.getArgType(0) != OFXOSC_TYPE_STRING) continue;
            if(message.getArgAsString(0) != defName) continue;
            if(message.getArgType(1) != OFXOSC_TYPE_INT32) continue;
            found = message.getArgAsInt32(1);
        }
    }
    return found;
}

bool ofxSCServer::writeNRTScore(const std::string& path, double endTime,
                               const std::vector<ofxOscMessage>& appendAtZero) const{
    // Sort pointers, never NRTEvent values. ofxOscBundle::copy() appends to
    // the destination instead of replacing it, and because the class declares
    // a copy constructor it gets no move assignment -- so every "move" a sort
    // performs is really an append onto an already-populated bundle, and each
    // merge pass doubles the messages in the score.
    std::vector<const NRTEvent*> events;
    events.reserve(nrtEvents.size());
    for(const auto& event : nrtEvents) events.push_back(&event);
    std::stable_sort(events.begin(), events.end(), [](const NRTEvent* a, const NRTEvent* b){
        return a->time < b->time;
    });

    double finish = endTime >= 0.0 ? endTime : nrtEndTime;
    if(finish < 0.0){
        finish = 0.0;
        for(const auto* event : events) finish = std::max(finish, event->time);
        finish += 0.1;
    }
    finish = std::max(0.0, finish);

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if(!file.is_open()) return false;

    auto writePacket = [&file](const std::vector<char>& packet){
        const uint32_t size = static_cast<uint32_t>(packet.size());
        const char prefix[4] = {
            static_cast<char>((size >> 24) & 0xff),
            static_cast<char>((size >> 16) & 0xff),
            static_cast<char>((size >> 8) & 0xff),
            static_cast<char>(size & 0xff)
        };
        file.write(prefix, sizeof(prefix));
        file.write(packet.data(), static_cast<std::streamsize>(packet.size()));
    };

    // NRT score timetags are seconds from zero, encoded as the fractional
    // 32.32 OSC timetag representation (unlike realtime NTP wall-clock tags).
    auto scoreTimeTag = [](double seconds){
        if(seconds <= 0.0) return static_cast<uint64_t>(0);
        const uint64_t whole = static_cast<uint64_t>(std::floor(seconds));
        const double fractional = seconds - std::floor(seconds);
        const uint64_t fraction = static_cast<uint64_t>(fractional * 4294967296.0);
        return (whole << 32) | std::min<uint64_t>(fraction, 0xffffffffULL);
    };

    bool extrasWritten = appendAtZero.empty();
    auto writeExtras = [&](){
        if(extrasWritten) return;
        extrasWritten = true;
        ofxOscBundle extras;
        for(const auto& message : appendAtZero) extras.addMessage(message);
        writePacket(osc.serializeScoreBundle(extras, scoreTimeTag(0.0)));
    };

    for(const auto* event : events){
        // The extras belong after everything already at time zero, so they win
        // over the values the capture recorded there.
        if(event->time > 0.0) writeExtras();
        writePacket(osc.serializeScoreBundle(event->bundle, scoreTimeTag(event->time)));
    }
    writeExtras();

    // Keep the final audio block at the requested duration. scsynth does not
    // reliably terminate an NRT process just because the score file reached
    // EOF, so the score must explicitly quit after that block has rendered.
    ofxOscBundle endBundle;
    ofxOscMessage endMessage;
    endMessage.setAddress("/c_set");
    endMessage.addIntArg(0);
    endMessage.addFloatArg(0.0f);
    endBundle.addMessage(endMessage);
    ofxOscMessage quitMessage;
    quitMessage.setAddress("/quit");
    endBundle.addMessage(quitMessage);
    writePacket(osc.serializeScoreBundle(endBundle, scoreTimeTag(finish)));
    return file.good();
}

void ofxSCServer::addNodeListener(ofxSCNode* node){
    nodeFeedbackFunctions[node] = [node](ofxOscMessage &msg){
        if(node != nullptr && node->nodeID == msg.getArgAsInt(0))
            node->feedbackListener(msg);
    };
}

void ofxSCServer::removeNodeListener(ofxSCNode *node){
    nodeFeedbackFunctions.erase(node);
}

ofxSCServer::ScopedTimetag::ScopedTimetag(uint64_t timetag) : previous(scopedTimetagValue){
    scopedTimetagValue = timetag;
}

ofxSCServer::ScopedTimetag::~ScopedTimetag(){
    scopedTimetagValue = previous;
}

uint64_t ofxSCServer::getScopedTimetag(){
    return scopedTimetagValue;
}

uint64_t ofxSCServer::timetagNow(double offsetSeconds){
    const auto unixTime = std::chrono::system_clock::now().time_since_epoch();
    const int64_t nowUs = std::chrono::duration_cast<std::chrono::microseconds>(unixTime).count();
    return timetagFromUnixMicroseconds(nowUs + (int64_t)(offsetSeconds * 1000000.0));
}

uint64_t ofxSCServer::timetagForSteadyTimeUs(uint64_t steadyTimeUs){
    // steady_clock is monotonic but has an arbitrary epoch, so it is converted
    // through the offset between the two clocks, sampled now. Both clocks are
    // read back to back, which keeps the conversion error far below the
    // resolution that matters here.
    const int64_t steadyNowUs = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    const int64_t systemNowUs = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return timetagFromUnixMicroseconds(systemNowUs + ((int64_t)steadyTimeUs - steadyNowUs));
}

uint64_t ofxSCServer::applyLatencyToTimetag(uint64_t timetag) const {
    if(!b_latency || timetag <= 1) return timetag;
    // Fixed-point seconds: the whole part is seconds, the fraction 2^-32 s.
    const double offset = (double)latency;
    const uint64_t offsetFixed = (uint64_t)(offset * 4294967296.0);
    return timetag + offsetFixed;
}

void ofxSCServer::sendMsgAt(ofxOscMessage& m, uint64_t timetag){
    if(nrtCapturing && timetag > 1){
        captureNRTMessage(m);
        if(nrtCaptureOnly) return;
    }
    if(timetag <= 1){
        sendMsg(m);
        return;
    }
    if(waitToSend){
        // The graph is still being built; ordering matters more than timing.
        toSendBundle.addMessage(m);
        return;
    }
    osc.sendMessage(m, true, applyLatencyToTimetag(timetag));
}

void ofxSCServer::sendBundleAt(ofxOscBundle& b, uint64_t timetag){
    if(nrtCapturing && timetag > 1){
        captureNRTBundle(b);
        if(nrtCaptureOnly) return;
    }
    if(timetag <= 1){
        sendBundle(b);
        return;
    }
    if(waitToSend){
        for(int i = 0; i < b.getMessageCount(); i++) toSendBundle.addMessage(b.getMessageAt(i));
        return;
    }
    osc.sendBundle(b, applyLatencyToTimetag(timetag));
}

uint64_t ofxSCServer::getNowTimetag(float latency){
    const auto unixTime = std::chrono::system_clock::now().time_since_epoch();
    const int64_t nowUs = std::chrono::duration_cast<std::chrono::microseconds>(unixTime).count();
    return timetagFromUnixMicroseconds(nowUs + (int64_t)((double)latency * 1000000.0));
}
