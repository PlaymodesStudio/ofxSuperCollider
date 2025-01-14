/*
 *  ofxOscSenderReceiver.h
 *  openFrameworks
 *
 *  Created by Daniel Jones on 17/11/2009.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 *
 *  Modified by Eduard Frigola on 03/01/2023
 */

#ifndef _ofxOscSENDERRECEIVER_H
#define _ofxOscSENDERRECEIVER_H

#include "OscTypes.h"
#include "OscOutboundPacketStream.h"
#include "OscPacketListener.h"
#include "UdpSocket.h"

#include "ofxOscBundle.h"
#include "ofParameter.h"

#include "ofThreadChannel.h"

/// \struct ofxOscSenderSettings
/// \brief OSC message sender settings
struct ofxOscSenderReceiverSettings {
    std::string host = "localhost"; ///< destination host name/ip
    int outPort = 0;                ///< destination port
    bool broadcast = true;          ///< broadcast (aka multicast) ip range support?
    int inPort = 0;                 ///< receiving port
    bool reuse = true;              ///< should the port be reused by other receivers?
    bool start = true;              ///< start listening after setup?
};

/// \class ofxOscSenderReceiver
/// \brief OSC message sender which sends to a specific host & port and receives from the same socket
class ofxOscSenderReceiver : public osc::OscPacketListener
{
public:
    ofxOscSenderReceiver() {}
    ~ofxOscSenderReceiver();
    ofxOscSenderReceiver(const ofxOscSenderReceiver &mom);
    ofxOscSenderReceiver& operator=(const ofxOscSenderReceiver &mom);
    /// for operator= and copy constructor
    ofxOscSenderReceiver& copy(const ofxOscSenderReceiver &other);

    /// set up the sender with the destination host name/ip and port
    /// \return true on success
    bool setup(const std::string &host, int outPort, int inPort);

    /// set up the sender with the given settings
    /// \returns true on success
    bool setup(const ofxOscSenderReceiverSettings &settings);

    /// clear the sender, does not clear host or port values
    void clear();

    /// send the given message
    /// if wrapInBundle is true (default), message sent in a timetagged bundle
    void sendMessage(const ofxOscMessage &message, bool wrapInBundle=false, uint64_t timetag = 1);

    /// send the given bundle
    void sendBundle(const ofxOscBundle &bundle, uint64_t timetag = 1);

    /// create & send a message with data from an ofParameter
    void sendParameter(const ofAbstractParameter &parameter);

    /// \return current host name/ip
    std::string getHost() const;

    /// \return current port
    int getOutPort() const;
    
    /// \return current port
    int getInPort() const;
    
    /// start listening manually using the current settings
    ///
    /// this is not required if you called setup(port)
    /// or setup(settings) with start set to true
    ///
    /// \return true if listening started or was already running
    bool start();
    
    /// stop listening, does not clear port value
    void stop();
    
    /// \return true if the receiver is listening
    bool isListening() const;

    /// \return true if there are any messages waiting for collection
    bool hasWaitingMessages() const;

    /// remove a message from the queue and copy it's data into msg
    /// \return false if there are no waiting messages, otherwise return true
    bool getNextMessage(ofxOscMessage& msg);
    OF_DEPRECATED_MSG("Pass a reference instead of a pointer", bool getNextMessage(ofxOscMessage *msg));
    
    /// try to get waiting message an ofParameter
    /// \return true if message was handled by the given parameter
    bool getParameter(ofAbstractParameter &parameter);

    /// \return the current sender settings
    const ofxOscSenderReceiverSettings &getSettings() const;

    /// output stream operator for string conversion and printing
    /// \return host name/ip and port separated by a space
    friend std::ostream& operator<<(std::ostream &os, const ofxOscSenderReceiver &senderReceiver);
    
protected:

    /// process an incoming osc message and add it to the queue
    virtual void ProcessMessage(const osc::ReceivedMessage &m, const osc::IpEndpointName &remoteEndpoint);

private:

    // helper methods for constructing messages
    void appendBundle(const ofxOscBundle &bundle, osc::OutboundPacketStream &p);
    void appendMessage(const ofxOscMessage &message, osc::OutboundPacketStream &p);
    void appendParameter(ofxOscBundle &bundle, const ofAbstractParameter &parameter, const std::string &address);
    void appendParameter(ofxOscMessage &msg, const ofAbstractParameter &parameter, const std::string &address);

    ofxOscSenderReceiverSettings settings; ///< current settings
    std::unique_ptr<osc::UdpTransmitSocket> sendSocket; ///< sender socket

    /// socket to listen on, unique for each port
    /// shared between objects if allowReuse is true
    std::unique_ptr<osc::UdpListeningReceiveSocket, std::function<void(osc::UdpListeningReceiveSocket*)>> listenSocket;

    std::thread listenThread; ///< listener thread
    ofThreadChannel<ofxOscMessage> messagesChannel; ///< message passing thread channel
};

#endif
