//
//  ofxAutomatedInput.cpp
//  ofxAutomatedInput
//
//  Created by Elie Zananiri on 2014-07-05.
//  Based on https://github.com/HeliosInteractive/ofxAutomatedInput
//

#include "ofxAutomatedInput.h"
#include "ofxAutomatedInputControlEvent.h"
#include "ofxAutomatedInputKeyEvent.h"
#include "ofxAutomatedInputMouseEvent.h"
#include "ofxAutomatedInputTouchEvent.h"

//--------------------------------------------------------------
ofxAutomatedInput::ofxAutomatedInput()
: _mode(OFX_AUTOMATED_INPUT_MODE_IDLE)
, _bTriggerOFEvents(true)
, _bLooping(false)
, _loopOffsetTime(0)
{
    
}

//--------------------------------------------------------------
ofxAutomatedInput::~ofxAutomatedInput()
{
    clear();
}

//--------------------------------------------------------------
void ofxAutomatedInput::clear()
{
    ofLogVerbose("ofxAutomatedInput::clear");
    
    stopPlayback();
    stopRecording();

    for (auto& it : _inputEvents) {
        delete it;
    }
    _inputEvents.clear();
}

//--------------------------------------------------------------
bool ofxAutomatedInput::saveToXml(const string &path)
{
    stopPlayback();
    stopRecording();

    ofxXmlSettings xml;
    xml.addTag("automated_input");
    xml.pushTag("automated_input");
    for (int i = 0 ; i < _inputEvents.size(); i++) {
        _inputEvents[i]->saveToXml(xml);
    }
    xml.popTag();
    
    return xml.saveFile(path);
}

//--------------------------------------------------------------
bool ofxAutomatedInput::loadFromXml(const string& path)
{
    clear();
    
    ofxXmlSettings xml;
    if (xml.loadFile(path)) {
        if (xml.pushTag("automated_input")) {
            for (int i = 0 ; i < xml.getNumTags("event"); i++) {
                ofxAutomatedInputType type = (ofxAutomatedInputType)xml.getAttribute("event", "event_type", (int)OFX_AUTOMATED_INPUT_TYPE_NONE, i);
                if (type == OFX_AUTOMATED_INPUT_TYPE_CONTROL) {
                    ofxAutomatedInputControlEvent *event = new ofxAutomatedInputControlEvent();
                    event->loadFromXml(xml, i);
                    _inputEvents.push_back(event);
                }
                else if (type == OFX_AUTOMATED_INPUT_TYPE_MOUSE) {
                    ofxAutomatedInputMouseEvent *event = new ofxAutomatedInputMouseEvent();
                    event->loadFromXml(xml, i);
                    _inputEvents.push_back(event);
                }
                else if (type == OFX_AUTOMATED_INPUT_TYPE_KEY) {
                    ofxAutomatedInputKeyEvent *event = new ofxAutomatedInputKeyEvent();
                    event->loadFromXml(xml, i);
                    _inputEvents.push_back(event);
                }
                else if (type == OFX_AUTOMATED_INPUT_TYPE_TOUCH) {
                    ofxAutomatedInputTouchEvent *event = new ofxAutomatedInputTouchEvent();
                    event->loadFromXml(xml, i);
                    _inputEvents.push_back(event);
                }
                else {
                    ofLogError("ofxAutomatedInput::loadFromXml") << "Unrecognized event type " << type;
                }
            }
            xml.popTag();
        }
        else {
            ofLogError("ofxAutomatedInput::loadFromXml") << "Malformed XML file at path " << path;
        }
    }
    else {
        ofLogError("ofxAutomatedInput::loadFromXml") << "Could not load file at path " << path;
        return false;
    }
    
    ofLogNotice("ofxAutomatedInput::loadFromXml") << "Successfully loaded " << _inputEvents.size() << " events with duration " << _inputEvents.back()->timeOffset();
    
    return true;
}

//--------------------------------------------------------------
void ofxAutomatedInput::debug()
{
    if (_playbackIdx >= 0 && _playbackIdx < _inputEvents.size()) {
        _inputEvents[_playbackIdx]->debug();
    }
}

//--------------------------------------------------------------
void ofxAutomatedInput::update(ofEventArgs& args)
{
    if (isPlaying()) {
        long long currTimeOffset = ofGetElapsedTimeMillis() - _playbackStartTime;
        
        int nextIdx = _playbackIdx + 1;
        if (nextIdx >= _inputEvents.size()) {
            if (_bLooping) {
                _playbackStartTime = ofGetElapsedTimeMillis() + _loopOffsetTime;
                _playbackIdx = -1;
                ofLogNotice("ofxAutomatedInput::update") << "Looping playback with start time " << _playbackStartTime << " and offset " << _loopOffsetTime;
            }
            else {
                stopPlayback();
            }
        }
        else if (_inputEvents[nextIdx]->timeOffset() <= currTimeOffset) {
            ofLogVerbose("ofxAutomatedInput::update") << "Playback ready to trigger event " << nextIdx;

            // Always play back Control events.
            if (_inputEvents[nextIdx]->type() == OFX_AUTOMATED_INPUT_TYPE_CONTROL) {
                ofxAutomatedInputControlEvent * controlEvent = static_cast<ofxAutomatedInputControlEvent *>(_inputEvents[nextIdx]);
                ofLogVerbose("ofxAutomatedInput::update") << "Triggering control event for action " << controlEvent->action();
                
                if (controlEvent->action() == ofxAutomatedInputControlEvent::Start) {
                    ofNotifyEvent(playbackStartedEvent, currTimeOffset);
                }
                else if (controlEvent->action() == ofxAutomatedInputControlEvent::Stop) {
                    ofNotifyEvent(playbackStoppedEvent, currTimeOffset);
                }
            }
            // Check that the input event type should be played back.
            else if (_inputEvents[nextIdx]->type() & _playbackFlags) {
                if (_inputEvents[nextIdx]->type() == OFX_AUTOMATED_INPUT_TYPE_MOUSE) {
                    ofxAutomatedInputMouseEvent * mouseEvent = static_cast<ofxAutomatedInputMouseEvent *>(_inputEvents[nextIdx]);
                    ofLogVerbose("ofxAutomatedInput::update") << "Triggering mouse event with type " << mouseEvent->args().type;
                    if (_bTriggerOFEvents) {
                        if (mouseEvent->args().type == ofMouseEventArgs::Moved) {
                            ofNotifyEvent(ofEvents().mouseMoved, mouseEvent->args());
                        }
                        else if (mouseEvent->args().type == ofMouseEventArgs::Pressed) {
                            ofNotifyEvent(ofEvents().mousePressed, mouseEvent->args());
                        }
                        else if (mouseEvent->args().type == ofMouseEventArgs::Dragged) {
                            ofNotifyEvent(ofEvents().mouseDragged, mouseEvent->args());
                        }
                        else if (mouseEvent->args().type == ofMouseEventArgs::Released) {
                            ofNotifyEvent(ofEvents().mouseReleased, mouseEvent->args());
                        }
                    }
                    
                    ofNotifyEvent(mouseInputEvent, mouseEvent->args());
                }
                else if (_inputEvents[nextIdx]->type() == OFX_AUTOMATED_INPUT_TYPE_KEY) {
                    ofxAutomatedInputKeyEvent * keyEvent = static_cast<ofxAutomatedInputKeyEvent *>(_inputEvents[nextIdx]);
                    ofLogVerbose("ofxAutomatedInput::update") << "Triggering key event with type " << keyEvent->args().type;
                    if (_bTriggerOFEvents) {
                        if (keyEvent->args().type == ofKeyEventArgs::Pressed) {
                            ofNotifyEvent(ofEvents().keyPressed, keyEvent->args());
                        }
                        else if (keyEvent->args().type == ofKeyEventArgs::Released) {
                            ofNotifyEvent(ofEvents().keyReleased, keyEvent->args());
                        }
                    }
                    
                    ofNotifyEvent(keyInputEvent, keyEvent->args());
                }
                else if (_inputEvents[nextIdx]->type() == OFX_AUTOMATED_INPUT_TYPE_TOUCH) {
                    ofxAutomatedInputTouchEvent * touchEvent = static_cast<ofxAutomatedInputTouchEvent *>(_inputEvents[nextIdx]);
                    ofLogVerbose("ofxAutomatedInput::update") << "Triggering touch event with type " << touchEvent->args().type;
                    if (_bTriggerOFEvents) {
                        if (touchEvent->args().type == ofTouchEventArgs::down) {
                            ofNotifyEvent(ofEvents().touchDown, touchEvent->args());
                        }
                        else if (touchEvent->args().type == ofTouchEventArgs::move) {
                            ofNotifyEvent(ofEvents().touchMoved, touchEvent->args());
                        }
                        else if (touchEvent->args().type == ofTouchEventArgs::up) {
                            ofNotifyEvent(ofEvents().touchUp, touchEvent->args());
                        }
                        else if (touchEvent->args().type == ofTouchEventArgs::doubleTap) {
                            ofNotifyEvent(ofEvents().touchDoubleTap, touchEvent->args());
                        }
                        else if (touchEvent->args().type == ofTouchEventArgs::cancel) {
                            ofNotifyEvent(ofEvents().touchCancelled, touchEvent->args());
                        }
                    }
                    
                    ofNotifyEvent(touchInputEvent, touchEvent->args());
                }
                else {
                    ofLogError("ofxAutomatedInput::update") << "Unrecognized event type " << _inputEvents[nextIdx]->type() << " at index " << nextIdx;
                }
            }
            
            ++_playbackIdx;
        }
    }
}

//--------------------------------------------------------------
void ofxAutomatedInput::mouseEventReceived(ofMouseEventArgs& args)
{
    long long timeOffset = ofGetElapsedTimeMillis() - _recordStartTime;
    ofxAutomatedInputMouseEvent * event = new ofxAutomatedInputMouseEvent(timeOffset, args);
    _inputEvents.push_back(event);
    
    ofLogVerbose("ofxAutomatedInput::mouseEventReceived") << "Adding event with type " << args.type << " at time " << timeOffset;
}

//--------------------------------------------------------------
void ofxAutomatedInput::keyEventReceived(ofKeyEventArgs& args)
{
    long long timeOffset = ofGetElapsedTimeMillis() - _recordStartTime;
    ofxAutomatedInputKeyEvent * event = new ofxAutomatedInputKeyEvent(timeOffset, args);
    _inputEvents.push_back(event);
    
    ofLogVerbose("ofxAutomatedInput::keyEventReceived") << "Adding event with type " << args.type << " at time " << timeOffset;
}

//--------------------------------------------------------------
void ofxAutomatedInput::touchEventReceived(ofTouchEventArgs& args)
{
    long long timeOffset = ofGetElapsedTimeMillis() - _recordStartTime;
    ofxAutomatedInputTouchEvent * event = new ofxAutomatedInputTouchEvent(timeOffset, args);
    _inputEvents.push_back(event);
    
    ofLogVerbose("ofxAutomatedInput::touchEventReceived") << "Adding event with type " << args.type << " at time " << timeOffset;
}

//--------------------------------------------------------------
void ofxAutomatedInput::startRecording(int recordFlags)
{
    if (isRecording()) return;
    
    if (isPlaying()) {
        stopPlayback();
    }
    
    _mode = OFX_AUTOMATED_INPUT_MODE_RECORD;
    _recordFlags = recordFlags;
    _recordStartTime = ofGetElapsedTimeMillis();
    ofLogNotice("ofxAutomatedInput::startRecording") << _recordStartTime;

    ofxAutomatedInputControlEvent * event = new ofxAutomatedInputControlEvent(0, ofxAutomatedInputControlEvent::Start);
    _inputEvents.push_back(event);
    
    ofLogVerbose("ofxAutomatedInput::startRecording") << "Adding event with type " << event->type() << " at time " << 0;
    
    if (_recordFlags & OFX_AUTOMATED_INPUT_TYPE_MOUSE) {
        ofAddListener(ofEvents().mouseMoved, this, &ofxAutomatedInput::mouseEventReceived);
        ofAddListener(ofEvents().mousePressed, this, &ofxAutomatedInput::mouseEventReceived);
        ofAddListener(ofEvents().mouseDragged, this, &ofxAutomatedInput::mouseEventReceived);
        ofAddListener(ofEvents().mouseReleased, this, &ofxAutomatedInput::mouseEventReceived);
    }
    if (_recordFlags & OFX_AUTOMATED_INPUT_TYPE_KEY) {
        ofAddListener(ofEvents().keyPressed, this, &ofxAutomatedInput::keyEventReceived);
        ofAddListener(ofEvents().keyReleased, this, &ofxAutomatedInput::keyEventReceived);
    }
    if (_recordFlags & OFX_AUTOMATED_INPUT_TYPE_TOUCH) {
        ofAddListener(ofEvents().touchDown, this, &ofxAutomatedInput::touchEventReceived);
        ofAddListener(ofEvents().touchMoved, this, &ofxAutomatedInput::touchEventReceived);
        ofAddListener(ofEvents().touchUp, this, &ofxAutomatedInput::touchEventReceived);
        ofAddListener(ofEvents().touchDoubleTap, this, &ofxAutomatedInput::touchEventReceived);
        ofAddListener(ofEvents().touchCancelled, this, &ofxAutomatedInput::touchEventReceived);
    }
}

//--------------------------------------------------------------
void ofxAutomatedInput::stopRecording()
{
    if (!isRecording()) return;
    
    long long timeOffset = ofGetElapsedTimeMillis() - _recordStartTime;
    ofLogNotice("ofxAutomatedInput::stopRecording") << ofGetElapsedTimeMillis();

    ofxAutomatedInputControlEvent * event = new ofxAutomatedInputControlEvent(timeOffset, ofxAutomatedInputControlEvent::Stop);
    _inputEvents.push_back(event);
    
    ofLogVerbose("ofxAutomatedInput::stopRecording") << "Adding event with type " << event->type() << " at time " << timeOffset;

    _mode = OFX_AUTOMATED_INPUT_MODE_IDLE;

    if (_recordFlags & OFX_AUTOMATED_INPUT_TYPE_MOUSE) {
        ofRemoveListener(ofEvents().mouseMoved, this, &ofxAutomatedInput::mouseEventReceived);
        ofRemoveListener(ofEvents().mousePressed, this, &ofxAutomatedInput::mouseEventReceived);
        ofRemoveListener(ofEvents().mouseDragged, this, &ofxAutomatedInput::mouseEventReceived);
        ofRemoveListener(ofEvents().mouseReleased, this, &ofxAutomatedInput::mouseEventReceived);
    }
    if (_recordFlags & OFX_AUTOMATED_INPUT_TYPE_KEY) {
        ofRemoveListener(ofEvents().keyPressed, this, &ofxAutomatedInput::keyEventReceived);
        ofRemoveListener(ofEvents().keyReleased, this, &ofxAutomatedInput::keyEventReceived);
    }
    if (_recordFlags & OFX_AUTOMATED_INPUT_TYPE_TOUCH) {
        ofRemoveListener(ofEvents().touchDown, this, &ofxAutomatedInput::touchEventReceived);
        ofRemoveListener(ofEvents().touchMoved, this, &ofxAutomatedInput::touchEventReceived);
        ofRemoveListener(ofEvents().touchUp, this, &ofxAutomatedInput::touchEventReceived);
        ofRemoveListener(ofEvents().touchDoubleTap, this, &ofxAutomatedInput::touchEventReceived);
        ofRemoveListener(ofEvents().touchCancelled, this, &ofxAutomatedInput::touchEventReceived);
    }
}

//--------------------------------------------------------------
void ofxAutomatedInput::toggleRecording()
{
    if (isRecording()) stopRecording();
    else startRecording();
}

//--------------------------------------------------------------
void ofxAutomatedInput::startPlayback(int playbackFlags)
{
    if (isPlaying()) return;
    
    if (isRecording()) {
        stopRecording();
    }
    
    _mode = OFX_AUTOMATED_INPUT_MODE_PLAYBACK;
    _playbackFlags = playbackFlags;
    _playbackStartTime = ofGetElapsedTimeMillis();
    _playbackIdx = -1;
    ofLogNotice("ofxAutomatedInput::startPlayback") << _playbackStartTime;

    ofAddListener(ofEvents().update, this, &ofxAutomatedInput::update);
}

//--------------------------------------------------------------
void ofxAutomatedInput::stopPlayback()
{
    if (isPlaying()) {
        _mode = OFX_AUTOMATED_INPUT_MODE_IDLE;
        long long currTimeOffset = ofGetElapsedTimeMillis() - _playbackStartTime;
        ofLogNotice("ofxAutomatedInput::stopPlayback") << currTimeOffset;

        ofRemoveListener(ofEvents().update, this, &ofxAutomatedInput::update);
    }
}

//--------------------------------------------------------------
void ofxAutomatedInput::togglePlayback()
{
    if (isPlaying()) stopPlayback();
    else startPlayback();
}
