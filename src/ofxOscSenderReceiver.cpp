/*
 *  ofxOscSenderReceiver.cpp
 *  openFrameworks
 *
 *  Created by Daniel Jones on 17/11/2009.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 *
 *  Modified by Eduard Frigola on 03/01/2023
 */

#include "ofxOscSenderReceiver.h"

//--------------------------------------------------------------
ofxOscSenderReceiver::~ofxOscSenderReceiver() {
    clear();
    stop();
}

//--------------------------------------------------------------
ofxOscSenderReceiver::ofxOscSenderReceiver(const ofxOscSenderReceiver & mom){
    copy(mom);
}

//--------------------------------------------------------------
ofxOscSenderReceiver& ofxOscSenderReceiver::operator=(const ofxOscSenderReceiver & mom){
    return copy(mom);
}

//--------------------------------------------------------------
ofxOscSenderReceiver& ofxOscSenderReceiver::copy(const ofxOscSenderReceiver& other){
    if(this == &other) return *this;
    settings = other.settings;
    if(other.sendSocket){
        setup(settings);
    }
    return *this;
}

//--------------------------------------------------------------
bool ofxOscSenderReceiver::setup(const std::string &host, int outPort, int inPort){
    if(listenSocket){
        stop();
    }
    settings.host = host;
    settings.outPort = outPort;
    settings.inPort = inPort;
    return setup(settings);
}

//--------------------------------------------------------------
bool ofxOscSenderReceiver::setup(const ofxOscSenderReceiverSettings &settings){
    // manually set larger buffer size instead of oscpack per-message size
    if(osc::UdpSocket::GetUdpBufferSize() == 0){
       osc::UdpSocket::SetUdpBufferSize(65535);
    }
    
    this->settings = settings;
    
    // check for empty host
    if(settings.host == "") {
        ofLogError("ofxOscSender") << "couldn't create sender to empty host";
        return false;
    }
    
    
    // create socket
    osc::UdpListeningReceiveSocket *socket = nullptr;
    try{
        osc::IpEndpointName name(osc::IpEndpointName::ANY_ADDRESS, settings.inPort);
        socket = new osc::UdpListeningReceiveSocket(name, this, settings.reuse);
        auto deleter = [](osc::UdpListeningReceiveSocket*socket){
            // tell the socket to shutdown
            socket->Break();
            delete socket;
        };
        auto newPtr = std::unique_ptr<osc::UdpListeningReceiveSocket, decltype(deleter)>(socket, deleter);
        listenSocket = std::move(newPtr);
    }
    catch(std::exception &e){
        std::string what = e.what();
        // strip endline as ofLogError already adds one
        if(!what.empty() && what.back() == '\n') {
            what = what.substr(0, what.size()-1);
        }
        ofLogError("ofxOscSenderReceiver") << "couldn't create receiver on port "
                                     << settings.inPort << ": " << what;
        if(socket != nullptr){
            delete socket;
            socket = nullptr;
        }
        return false;
    }

    listenThread = std::thread([this]{
        while(listenSocket){
            try{
                listenSocket->Run();
            }
            catch(std::exception &e){
                ofLogWarning("ofxOscSenderReceiver") << e.what();
            }
        }
    });

    // detach thread so we don't have to wait on it before creating a new socket
    // or on destruction, the custom deleter for the socket unique_ptr already
    // does the right thing
    listenThread.detach();
    
    // reuse socket
    osc::UdpTransmitSocket *send_socket = (osc::UdpTransmitSocket *) socket;
    try{
        osc::IpEndpointName name = osc::IpEndpointName(settings.host.c_str(), settings.outPort);
        if (!name.address){
                ofLogError("ofxOscSender") << "bad host? " << settings.host;
                return false;
        }
//        socket = new osc::UdpTransmitSocket(name, settings.broadcast);
        send_socket->Connect(name);
        sendSocket.reset(send_socket);
    }
    catch(std::exception &e){
        std::string what = e.what();
        // strip endline as ofLogError already adds one
        if(!what.empty() && what.back() == '\n') {
            what = what.substr(0, what.size()-1);
        }
        ofLogError("ofxOscSender") << "couldn't create sender to "
                                   << settings.host << " on port "
                                   << settings.outPort << ": " << what;
        if(socket != nullptr){
            delete send_socket;
            send_socket = nullptr;
        }
        sendSocket.reset();
        return false;
    }
    
    return true;
}

//--------------------------------------------------------------
void ofxOscSenderReceiver::clear(){
    sendSocket.reset();
}

//--------------------------------------------------------------
void ofxOscSenderReceiver::sendBundle(const ofxOscBundle &bundle, uint64_t timetag){
    if(!sendSocket){
        ofLogError("ofxOscSender") << "trying to send with empty socket";
        return;
    }
    
    // setting this much larger as it gets trimmed down to the size its using before being sent.
    // TODO: much better if we could make this dynamic? Maybe have ofxOscBundle return its size?
    static const int OUTPUT_BUFFER_SIZE = 327680;
    char buffer[OUTPUT_BUFFER_SIZE];
    osc::OutboundPacketStream p(buffer, OUTPUT_BUFFER_SIZE);

    // serialise the bundle and send
    p << osc::BundleInitiator(timetag);
    appendBundle(bundle, p);
    p << osc::EndBundle;
    sendSocket->Send(p.Data(), p.Size());
}

//--------------------------------------------------------------
void ofxOscSenderReceiver::sendMessage(const ofxOscMessage &message, bool wrapInBundle, uint64_t timetag){
    if(!sendSocket){
        ofLogError("ofxOscSender") << "trying to send with empty socket";
        return;
    }
    
    // setting this much larger as it gets trimmed down to the size its using before being sent.
    // TODO: much better if we could make this dynamic? Maybe have ofxOscMessage return its size?
    static const int OUTPUT_BUFFER_SIZE = 327680;
    char buffer[OUTPUT_BUFFER_SIZE];
    osc::OutboundPacketStream p(buffer, OUTPUT_BUFFER_SIZE);

    // serialise the message and send
    if(wrapInBundle) {
        p << osc::BundleInitiator(timetag);
    }
    appendMessage(message, p);
    if(wrapInBundle) {
        p << osc::EndBundle;
    }
    sendSocket->Send(p.Data(), p.Size());
}

//--------------------------------------------------------------
void ofxOscSenderReceiver::sendParameter(const ofAbstractParameter &parameter){
    if(!parameter.isSerializable()) return;
    if(parameter.type() == typeid(ofParameterGroup).name()){
        std::string address = "/";
        const std::vector<std::string> hierarchy = parameter.getGroupHierarchyNames();
        for(int i = 0; i < (int)hierarchy.size()-1; i++){
            address += hierarchy[i] + "/";
        }
        ofxOscBundle bundle;
        appendParameter(bundle, parameter, address);
        sendBundle(bundle);
    }
    else{
        std::string address = "";
        const std::vector<std::string> hierarchy = parameter.getGroupHierarchyNames();
        for(int i = 0; i < (int)hierarchy.size()-1; i++){
            address += "/" + hierarchy[i];
        }
        if(address.length()) {
            address += "/";
        }
        ofxOscMessage msg;
        appendParameter(msg, parameter, address);
        sendMessage(msg, false);
    }
}

//--------------------------------------------------------------
std::string ofxOscSenderReceiver::getHost() const{
    return settings.host;
}

//--------------------------------------------------------------
int ofxOscSenderReceiver::getInPort() const{
    return settings.inPort;
}

//--------------------------------------------------------------
int ofxOscSenderReceiver::getOutPort() const{
    return settings.outPort;
}

//--------------------------------------------------------------
const ofxOscSenderReceiverSettings &ofxOscSenderReceiver::getSettings() const {
    return settings;
}

// PRIVATE
//--------------------------------------------------------------
void ofxOscSenderReceiver::appendBundle(const ofxOscBundle &bundle, osc::OutboundPacketStream &p){
    // recursively serialise the bundle
    p << osc::BeginBundleImmediate;
    for(int i = 0; i < bundle.getBundleCount(); i++){
        appendBundle(bundle.getBundleAt(i), p);
    }
    for(int i = 0; i < bundle.getMessageCount(); i++){
        appendMessage(bundle.getMessageAt(i), p);
    }
    p << osc::EndBundle;
}

//--------------------------------------------------------------
void ofxOscSenderReceiver::appendMessage(const ofxOscMessage &message, osc::OutboundPacketStream &p){
    p << osc::BeginMessage(message.getAddress().c_str());
    for(size_t i = 0; i < message.getNumArgs(); ++i) {
        switch(message.getArgType(i)){
            case OFXOSC_TYPE_INT32:
                p << message.getArgAsInt32(i);
                break;
            case OFXOSC_TYPE_INT64:
                p << (osc::int64)message.getArgAsInt64(i);
                break;
            case OFXOSC_TYPE_FLOAT:
                p << message.getArgAsFloat(i);
                break;
            case OFXOSC_TYPE_DOUBLE:
                p << message.getArgAsDouble(i);
                break;
            case OFXOSC_TYPE_STRING:
                p << message.getArgAsString(i).c_str();
                break;
            case OFXOSC_TYPE_SYMBOL:
                p << osc::Symbol(message.getArgAsString(i).c_str());
                break;
            case OFXOSC_TYPE_CHAR:
                if(message.getArgAsChar(i) == '['){
                    p << osc::ArrayInitiator();
                }else if(message.getArgAsChar(i) == ']'){
                    p << osc::ArrayTerminator();
                }else{
                    p << message.getArgAsChar(i);
                }
                break;
            case OFXOSC_TYPE_MIDI_MESSAGE:
                p << osc::MidiMessage(message.getArgAsMidiMessage(i));
                break;
            case OFXOSC_TYPE_TRUE: case OFXOSC_TYPE_FALSE:
                p << message.getArgAsBool(i);
                break;
            case OFXOSC_TYPE_NONE:
                p << osc::NilType();
                break;
            case OFXOSC_TYPE_TRIGGER:
                p << osc::InfinitumType();
                break;
            case OFXOSC_TYPE_TIMETAG:
                p << osc::TimeTag(message.getArgAsTimetag(i));
                break;
            case OFXOSC_TYPE_RGBA_COLOR:
                p << osc::RgbaColor(message.getArgAsRgbaColor(i));
                break;
            case OFXOSC_TYPE_BLOB: {
                ofBuffer buff = message.getArgAsBlob(i);
                p << osc::Blob(buff.getData(), (unsigned long)buff.size());
                break;
            }
            default:
                ofLogError("ofxOscSender") << "appendMessage(): bad argument type "
                    << message.getArgType(i) << " '" << (char) message.getArgType(i) << "'";
                break;
        }
    }
    p << osc::EndMessage;
}

//--------------------------------------------------------------
void ofxOscSenderReceiver::appendParameter(ofxOscBundle &_bundle, const ofAbstractParameter &parameter, const std::string &address){
    if(parameter.type() == typeid(ofParameterGroup).name()){
        ofxOscBundle bundle;
        const ofParameterGroup &group = static_cast<const ofParameterGroup &>(parameter);
        for(std::size_t i = 0; i < group.size(); i++){
            const ofAbstractParameter & p = group[i];
            if(p.isSerializable()){
                appendParameter(bundle, p, address+group.getEscapedName()+"/");
            }
        }
        _bundle.addBundle(bundle);
    }
    else{
        if(parameter.isSerializable()){
            ofxOscMessage msg;
            appendParameter(msg, parameter, address);
            _bundle.addMessage(msg);
        }
    }
}

//--------------------------------------------------------------
void ofxOscSenderReceiver::appendParameter(ofxOscMessage &msg, const ofAbstractParameter &parameter, const std::string &address){
    msg.setAddress(address+parameter.getEscapedName());
    if(parameter.type() == typeid(ofParameter<int>).name()){
        msg.addIntArg(parameter.cast<int>());
    }
    else if(parameter.type() == typeid(ofParameter<float>).name()){
        msg.addFloatArg(parameter.cast<float>());
    }
    else if(parameter.type() == typeid(ofParameter<double>).name()){
        msg.addDoubleArg(parameter.cast<double>());
    }
    else if(parameter.type() == typeid(ofParameter<bool>).name()){
        msg.addBoolArg(parameter.cast<bool>());
    }
    else{
        msg.addStringArg(parameter.toString());
    }
}

//--------------------------------------------------------------
void ofxOscSenderReceiver::stop() {
    listenSocket.reset();
}

//--------------------------------------------------------------
bool ofxOscSenderReceiver::isListening() const{
    return listenSocket != nullptr;
}

//--------------------------------------------------------------
bool ofxOscSenderReceiver::hasWaitingMessages() const{
    return !messagesChannel.empty();
}

//--------------------------------------------------------------
bool ofxOscSenderReceiver::getNextMessage(ofxOscMessage *message){
    return getNextMessage(*message);
}

//--------------------------------------------------------------
bool ofxOscSenderReceiver::getNextMessage(ofxOscMessage &message){
    return messagesChannel.tryReceive(message);
}

//--------------------------------------------------------------
bool ofxOscSenderReceiver::getParameter(ofAbstractParameter &parameter){
    ofxOscMessage msg;
    while(messagesChannel.tryReceive(msg)){
        ofAbstractParameter * p = &parameter;
        std::vector<std::string> address = ofSplitString(msg.getAddress(),"/", true);
        for(unsigned int i = 0; i < address.size(); i++){
            if(p){
                if(address[i] == p->getEscapedName()){
                    if(p->type() == typeid(ofParameterGroup).name()){
                        if(address.size() >= i+1){
                            ofParameterGroup* g = static_cast<ofParameterGroup*>(p);
                            if(g->contains(address[i+1])){
                                p = &g->get(address[i+1]);
                            }
                            else{
                                p = nullptr;
                            }
                        }
                    }
                    else if(p->type() == typeid(ofParameter<int>).name() &&
                        msg.getArgType(0) == OFXOSC_TYPE_INT32){
                        p->cast<int>() = msg.getArgAsInt32(0);
                    }
                    else if(p->type() == typeid(ofParameter<float>).name() &&
                        msg.getArgType(0) == OFXOSC_TYPE_FLOAT){
                        p->cast<float>() = msg.getArgAsFloat(0);
                    }
                    else if(p->type() == typeid(ofParameter<double>).name() &&
                        msg.getArgType(0) == OFXOSC_TYPE_DOUBLE){
                        p->cast<double>() = msg.getArgAsDouble(0);
                    }
                    else if(p->type() == typeid(ofParameter<bool>).name() &&
                        (msg.getArgType(0) == OFXOSC_TYPE_TRUE ||
                         msg.getArgType(0) == OFXOSC_TYPE_FALSE ||
                         msg.getArgType(0) == OFXOSC_TYPE_INT32 ||
                         msg.getArgType(0) == OFXOSC_TYPE_INT64 ||
                         msg.getArgType(0) == OFXOSC_TYPE_FLOAT ||
                         msg.getArgType(0) == OFXOSC_TYPE_DOUBLE ||
                         msg.getArgType(0) == OFXOSC_TYPE_STRING ||
                         msg.getArgType(0) == OFXOSC_TYPE_SYMBOL)){
                        p->cast<bool>() = msg.getArgAsBool(0);
                    }
                    else if(msg.getArgType(0) == OFXOSC_TYPE_STRING){
                        p->fromString(msg.getArgAsString(0));
                    }
                }
            }
        }
    }
    return true;
}

// PROTECTED
//--------------------------------------------------------------
void ofxOscSenderReceiver::ProcessMessage(const osc::ReceivedMessage &m, const osc::IpEndpointName &remoteEndpoint){
    // convert the message to an ofxOscMessage
    ofxOscMessage msg;

    // set the address
    msg.setAddress(m.AddressPattern());
    
    // set the sender ip/host
    char endpointHost[osc::IpEndpointName::ADDRESS_STRING_LENGTH];
    remoteEndpoint.AddressAsString(endpointHost);
    msg.setRemoteEndpoint(endpointHost, remoteEndpoint.port);

    // transfer the arguments
    for(osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin(); arg != m.ArgumentsEnd(); ++arg){
        if(arg->IsInt32()){
            msg.addIntArg(arg->AsInt32Unchecked());
        }
        else if(arg->IsInt64()){
            msg.addInt64Arg(arg->AsInt64Unchecked());
        }
        else if( arg->IsFloat()){
            msg.addFloatArg(arg->AsFloatUnchecked());
        }
        else if(arg->IsDouble()){
            msg.addDoubleArg(arg->AsDoubleUnchecked());
        }
        else if(arg->IsString()){
            msg.addStringArg(arg->AsStringUnchecked());
        }
        else if(arg->IsSymbol()){
            msg.addSymbolArg(arg->AsSymbolUnchecked());
        }
        else if(arg->IsChar()){
            msg.addCharArg(arg->AsCharUnchecked());
        }
        else if(arg->IsMidiMessage()){
            msg.addMidiMessageArg(arg->AsMidiMessageUnchecked());
        }
        else if(arg->IsBool()){
            msg.addBoolArg(arg->AsBoolUnchecked());
        }
        else if(arg->IsNil()){
            msg.addNoneArg();
        }
        else if(arg->IsInfinitum()){
            msg.addTriggerArg();
        }
        else if(arg->IsTimeTag()){
            msg.addTimetagArg(arg->AsTimeTagUnchecked());
        }
        else if(arg->IsRgbaColor()){
            msg.addRgbaColorArg(arg->AsRgbaColorUnchecked());
        }
        else if(arg->IsBlob()){
            const char * dataPtr;
            osc::osc_bundle_element_size_t len = 0;
            arg->AsBlobUnchecked((const void*&)dataPtr, len);
            ofBuffer buffer(dataPtr, len);
            msg.addBlobArg(buffer);
        }
        else {
            ofLogError("ofxOscSenderReceiver") << "ProcessMessage(): argument in message "
                << m.AddressPattern() << " is an unknown type "
                << (int) arg->TypeTag() << " '" << (char) arg->TypeTag() << "'";
                break;
        }
    }

    // send msg to main thread
    messagesChannel.send(std::move(msg));
}


// friend functions
//--------------------------------------------------------------
std::ostream& operator<<(std::ostream &os, const ofxOscSenderReceiver &senderReceiver) {
    os << senderReceiver.getHost() << " " << senderReceiver.getOutPort() << " " << senderReceiver.getInPort();
    return os;
}
