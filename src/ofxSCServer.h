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


/// TODO: Poder afegir un missatge de /sync Que faci pausar els missatges osc fins que s'hagi rebut
/// el /sync corresponent de supercollider, per a no enviar missatges de control abans que s'hagi executat tot el graph per exmeple

#pragma once

#include <vector>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_set>

#include "ofxOsc.h"
#include "ofxOscSenderReceiver.h"
#include "ofxSCResourceAllocator.h"

class ofxSCBuffer;
class ofxSCBus;
class ofxSCNode;


class ofxSCServer
{
public:	
	ofxSCServer(std::string hostname = "localhost", unsigned int port = 57110, unsigned int receivePort = 57130, unsigned int numInputs = 32, unsigned int numOutputs = 32, unsigned int numAudioBusses = 65536, unsigned int numControlBusses = 65536, unsigned int numBuffers = 65536);
	~ofxSCServer();

	static ofxSCServer     *local();
	
	void process();
	void _process(ofEventArgs &e);
	void notify();
    void sendInitializationSyncMessage();
	void resetAllocators();
	
	void sendMsg(ofxOscMessage& message);
    void sendBundle(ofxOscBundle& bundle);

    // --- Timestamped sending ------------------------------------------------
    // scsynth executes an OSC bundle at the instant carried by its NTP
    // timetag, sample-accurately, instead of at the top of the control block
    // in which it happens to arrive. That is the only way a musical event can
    // be precise while the sender runs at frame rate.
    void sendMsgAt(ofxOscMessage& message, uint64_t timetag);
    void sendBundleAt(ofxOscBundle& bundle, uint64_t timetag);
    // Builds a timetag from a std::chrono::steady_clock instant in
    // microseconds, the domain a host's transport clock normally works in.
    static uint64_t timetagForSteadyTimeUs(uint64_t steadyTimeUs);
    // Now, optionally offset by a number of seconds. Microsecond resolution.
    static uint64_t timetagNow(double offsetSeconds = 0.0);

    // SuperCollider non-realtime score capture. Existing nodes continue to
    // use sendMsg()/sendBundle(); the server records those OSC commands.
    void beginNRTCapture(bool captureOnly = true);
    void endNRTCapture(double endTime = -1.0);
    void clearNRTScore();
    void setNRTTime(double seconds);
    void setNRTTimeProvider(std::function<double()> provider);
    void setNRTTimeProviderEnabled(bool enabled);
    bool isNRTCapturing() const { return nrtCapturing; }
    // A /sync round trip: the server answers only once every asynchronous
    // command issued before it has finished. Loading the SynthDef tree is the
    // slow one -- seconds, not milliseconds -- and anything that waits for it
    // has to wait for this, not for a guess.
    void requestNRTSync();
    bool isNRTSyncPending() const { return nrtSyncPending; }

    // Pauses recording into the score without ending the capture or stopping
    // anything being sent. Used while a capture is armed but not yet rolling:
    // the graph's state is already in the score at time zero, and every frame
    // that passes before the transport starts would otherwise pile more
    // messages onto that same timestamp.
    void setNRTCaptureSuspended(bool suspended){ nrtCaptureSuspended = suspended; }
    bool isNRTCaptureSuspended() const { return nrtCaptureSuspended; }

    // While a capture is armed but not yet rolling, only state belongs in the
    // score. Anything that happens during that window happened *before* the
    // recording started -- a note played while waiting would otherwise be
    // frozen at score time zero and sound again at the top of the render.
    void setNRTEventsSuppressed(bool suppressed){ nrtEventsSuppressed = suppressed; }
    bool areNRTEventsSuppressed() const { return nrtEventsSuppressed; }
    std::size_t getNRTEventCount() const { return nrtEvents.size(); }
    double getNRTTime() const { return nrtTime; }
    // Node id the captured score itself gives to the last synth created from
    // `defName`. The live server keeps allocating node ids after a capture, so
    // the id a node object reports now is not the id that exists inside the
    // score; editing a score has to use the score's own ids. Returns -1 when
    // the score creates no such synth.
    int findNRTNodeID(const std::string& defName) const;
    // appendAtZero is written as one extra packet at score time zero, after
    // everything else already there. Used to render a variant of the same
    // capture -- pointing the file-writing synth at a different bus yields a
    // stem without re-capturing anything.
    bool writeNRTScore(const std::string& path, double endTime = -1.0,
                       const std::vector<ofxOscMessage>& appendAtZero = {}) const;

    // Every sendMsg()/sendBundle() made on this thread while a scope is alive
    // carries this timetag. It lets existing "set this parameter on the
    // synths" code be reused verbatim for a value that must land at a precise
    // instant, instead of duplicating it into a scheduling-specific path.
    class ScopedTimetag {
    public:
        explicit ScopedTimetag(uint64_t timetag);
        ~ScopedTimetag();
        ScopedTimetag(const ScopedTimetag&) = delete;
        ScopedTimetag& operator=(const ScopedTimetag&) = delete;
    private:
        uint64_t previous;
    };
    // 0 when no scope is active.
    static uint64_t getScopedTimetag();

    
    void setWaitToSend(bool b);
    bool getWaitToSend();
    void sendStoredBundle();
    
    void setLatency(float _latency){latency = _latency;};
    void setBLatency(bool b){b_latency = b;};
    float getLatency(){return latency;};
    bool getBLatency(){return b_latency;};
	
	ofxSCResourceAllocator *allocatorBusAudio;
	ofxSCResourceAllocator *allocatorBusControl;
	ofxSCResourceAllocator *allocatorBuffer;
	ofxSCResourceAllocator *allocatorSynth;

	std::vector<ofxSCBuffer*> buffers;
    std::vector<ofxSCBus*> controlBusses;
    std::vector<ofxSCBus*> audioBusses;
    
    ofEvent<void> serverBootedEvent;
    ofEvent<void> serverInitializedEvent;
    ofEvent<ofxOscMessage> queryTreeReplyEvent;
    
    void addNodeListener(ofxSCNode* node);
    void removeNodeListener(ofxSCNode* node);
    
    ofEvent<ofxOscMessage> newFeedbackMessage;
    
protected:

	ofxOscSenderReceiver   osc;
    ofEventListener listener;
    std::map<ofxSCNode*, std::function<void(ofxOscMessage&)>> nodeFeedbackFunctions;
    
    bool waitToSend;
    
    ofxOscBundle toSendBundle;
	
	static ofxSCServer *plocal;
	std::string hostname;
	unsigned int port;
	unsigned int numInputs;
	unsigned int numOutputs;
	unsigned int numAudioBusses;
	unsigned int numControlBusses;
	unsigned int numBuffers;
    
    float latency;
    bool b_latency;
    
    bool initializing;

    struct NRTEvent {
        double time = 0.0;
        ofxOscBundle bundle;
    };

    bool nrtCapturing = false;
    bool nrtCaptureOnly = true;
    bool nrtCaptureSuspended = false;
    bool nrtSyncPending = false;
    bool nrtEventsSuppressed = false;
    bool nrtTimeProviderEnabled = false;
    double nrtTime = 0.0;
    double nrtEndTime = -1.0;
    std::function<double()> nrtTimeProvider;
    std::vector<NRTEvent> nrtEvents;
    std::unordered_set<int> nrtCreatedNodeIDs;
    
private:
    double getNRTEventTime() const;
    void captureNRTMessage(const ofxOscMessage& message);
    void captureNRTBundle(const ofxOscBundle& bundle);
    bool appendNRTMessage(ofxOscBundle& destination, const ofxOscMessage& message);
    void appendNRTBundleContents(ofxOscBundle& destination, const ofxOscBundle& source);
    bool shouldCaptureNRTAddress(const std::string& address) const;
    bool shouldCaptureNRTMessage(const ofxOscMessage& message);
    bool isTransientNRTMessage(const ofxOscMessage& message) const;
    uint64_t getNowTimetag(float latency = 0);
    // Adds the server's own latency offset to an absolute timetag when
    // latency compensation is on, so scheduled and immediate messages keep
    // the same relative timing.
    uint64_t applyLatencyToTimetag(uint64_t timetag) const;
};
