/*
 * Copyright (C) 2015 Dan Wilcox <danomatika@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * See https://github.com/Akira-Hayasaka/ofxGLEditor for more info.
 *
 * A rewrite of the Fluxus Repl http://www.pawfal.org/fluxus for OF
 * Copyright (C) Dave Griffiths
 */
#include "ofxRepl.h"

#include "Unicode.h"

// utils
bool isBalanced(wstring s);
bool isEmpty(wstring s);

wstring ofxRepl::s_banner = wstring(L"");
wstring ofxRepl::s_prompt = wstring(L"> ");

//--------------------------------------------------------------
ofxRepl::ofxRepl() : ofxEditor() {
	m_listener = NULL;
	m_promptPos = 0;
	m_insertPos = 0;
	m_historyNavStarted = false;
	m_linePos = 0;
}

//--------------------------------------------------------------
ofxRepl::ofxRepl(ofxEditorSettings &sharedSettings) : ofxEditor(sharedSettings) {
	m_listener = NULL;
	m_promptPos = 0;
	m_insertPos = 0;
	m_historyNavStarted = false;
	m_linePos = 0;
}

//--------------------------------------------------------------
ofxRepl::~ofxRepl() {
	ofLogToConsole();
}

//--------------------------------------------------------------
void ofxRepl::setup() {

	// setup our custom logger
	m_logger = ofPtr<Logger>(new Logger);
	m_logger->m_parent = this;
	ofSetLoggerChannel(m_logger);

	// print greeting and first prompt
	if(s_banner != L"") {
		resize();
		print(s_banner);
	}
	printPrompt();
}

//--------------------------------------------------------------
void ofxRepl::setup(ofxReplListener *listener) {
	setup();
	m_listener = listener;
}

//--------------------------------------------------------------
void ofxRepl::keyPressed(int key) {
	
	// check modifier keys
	bool modifierPressed = ofxEditor::getSuperAsModifier() ? ofGetKeyPressed(OF_KEY_SUPER) : ofGetKeyPressed(OF_KEY_CONTROL);
	if(modifierPressed) {
		switch(key) {
			case 'c': case 3:
				if(ofGetKeyPressed(OF_KEY_SHIFT)) {
					clearHistory();
				}
				else {
					clear();
				}
				return;
			case 'a': case 10:
				// swallow select all
				return;
		}
	}
	
	// swallow text buffer removal
	if((m_position <= m_promptPos && key == OF_KEY_BACKSPACE) ||
	   (m_position < m_promptPos && key == OF_KEY_DEL) ||
	   ((m_position < m_promptPos || m_highlightStart < m_promptPos || m_highlightEnd < m_promptPos) &&
	   (modifierPressed && (key == 'x' || key == 24)))) {
		return;
	}

	// key cursor on prompt line
	if(m_position < m_promptPos) {
		m_position = m_text.length();
	}
	else {
		switch(key) {
			case OF_KEY_LEFT:
				if(m_position == m_promptPos) {
					return;
				}
				break;
			case OF_KEY_UP:
				historyPrev();
				return;
			case OF_KEY_DOWN:
				historyNext();
				return;
			case OF_KEY_END:
				m_position = m_text.length();
				return;
			case OF_KEY_HOME:
				m_position = m_promptPos;
				return;
		}
	}
		
	keepCursorVisible();

	if(key == OF_KEY_RETURN) {
		m_position = m_text.length();
        eval();
		return;
    }

	ofxEditor::keyPressed(key);
}

//--------------------------------------------------------------
void ofxRepl::print(const wstring &what) {

	wstring to_print;
	for(wstring::const_iterator i = what.begin(); i != what.end(); ++i) {
		m_linePos++;
		if(*i == L'\n') {
			m_linePos = 0;
		}
		to_print += *i;
	}
	m_text.insert(m_insertPos, to_print);
		
	m_position += to_print.length();
	m_promptPos += to_print.length();
	m_insertPos += to_print.length();
	
	keepCursorVisible();
}

//--------------------------------------------------------------
void ofxRepl::print(const string &what) {
	print(string_to_wstring(what));
}

//--------------------------------------------------------------
void ofxRepl::printEvalReturn(const wstring &what) {
	if(what.size() > 0) {
		print(what);
		print(L"\n");
	}
	printPrompt();
}

//--------------------------------------------------------------
void ofxRepl::printEvalReturn(const string &what) {
	printEvalReturn(string_to_wstring(what));
}

//--------------------------------------------------------------
void ofxRepl::clear() {
	clearText();
	m_promptPos = 0;
	m_insertPos = 0;
	printPrompt();
}

//--------------------------------------------------------------
void ofxRepl::clearHistory() {
	historyClear();
}

// PROTECTED

//--------------------------------------------------------------
void ofxRepl::eval() {
	if(m_promptPos < m_text.size()) {
		wstring defun = m_text.substr(m_promptPos);
		if(!isEmpty(defun)) {
			m_insertPos = m_text.length();
			print(L"\n");
			
			m_evalText = defun;
			if(m_listener) {
				m_listener->evalReplEvent(wstring_to_string(m_evalText));
			}
			else {
				ofLogWarning("ofxRepl") << "listener not set";
			}

			if(defun[defun.length()-1] == '\n') {
				defun.resize(defun.length()-1, 0);
			}
			m_history.push_back(defun);
			m_historyNavStarted = false;
		
			// go to next line in case the listener isn't set
			if(!m_listener) {
				printPrompt();
			}
		}
	}
}

//--------------------------------------------------------------
void ofxRepl::printPrompt() {
	m_insertPos = m_text.length();
	if(m_text.length() > 0 && m_text[m_insertPos-1] != '\n') {
		m_text += '\n';
		m_insertPos++;
	}
	m_text += s_prompt;
	m_position = m_promptPos = m_text.length();
}

//--------------------------------------------------------------
void ofxRepl::historyClear() {
	m_historyNavStarted = false;
	m_history.clear();
	m_insertPos = 0;
	m_historyIter = m_history.end();
	m_historyPresent = m_text.substr(m_promptPos);
}

//--------------------------------------------------------------
void ofxRepl::historyPrev() {
	
	if(!m_historyNavStarted) {
		m_historyIter = m_history.end();
		m_historyNavStarted = true;
	}

	if(m_historyIter == m_history.end()) {
		m_historyPresent = m_text.substr(m_promptPos);
	}

	if(m_historyIter == m_history.begin()) {
		return;
	}

	m_historyIter--;
	historyShow(*m_historyIter);
}

//--------------------------------------------------------------
void ofxRepl::historyNext() {
	if(!m_historyNavStarted || (m_historyIter == m_history.end())) {
		return;
	}
	m_historyIter++;
	historyShow((m_historyIter == m_history.end()) ? m_historyPresent : *m_historyIter);
}

//--------------------------------------------------------------
void ofxRepl::historyShow(wstring what) {
	m_text.resize(m_promptPos, 0);
	m_text += what;
	m_position = m_text.length();
}

//--------------------------------------------------------------
void ofxRepl::keepCursorVisible() {
	unsigned int curVisLine = 0;
	for(int i = m_position; i > m_topTextPosition; i--) {
		if(m_text[i] == L'\n') {
			curVisLine++;
		}
	}
	while(curVisLine >= m_visibleLines) {
		if(m_text[m_topTextPosition++] == L'\n') {
			curVisLine--;
		}
	}
}

// STATIC UTILS

//--------------------------------------------------------------
void ofxRepl::setReplBanner(const wstring &text) {
	s_banner = text;
}

//--------------------------------------------------------------
void ofxRepl::setReplBanner(const string &text) {
	s_banner = string_to_wstring(text);
}

//--------------------------------------------------------------
wstring& ofxRepl::getWideReplBanner() {
	return s_banner;
}

//--------------------------------------------------------------
string ofxRepl::getReplBanner() {
	return wstring_to_string(s_banner);
}
	
//--------------------------------------------------------------
void ofxRepl::setReplPrompt(const wstring &text) {
	s_prompt = text;
}

//--------------------------------------------------------------
void ofxRepl::setReplPrompt(const string &text) {
	s_prompt = string_to_wstring(text);
}

//--------------------------------------------------------------
wstring& ofxRepl::getWideReplPrompt() {
	return s_prompt;
}

//--------------------------------------------------------------
string ofxRepl::getReplPrompt() {
	return wstring_to_string(s_prompt);
}

// PRIVATE

//--------------------------------------------------------------
void ofxRepl::Logger::log(ofLogLevel level, const string & module, const string & message){
	ofConsoleLoggerChannel::log(level, module, message);
	m_parent->print(string_to_wstring(message));
	m_parent->print(L"\n");
}

//--------------------------------------------------------------
void ofxRepl::Logger::log(ofLogLevel level, const string & module, const char* format, va_list args){
	ofConsoleLoggerChannel::log(level, module, format, args);
	m_parent->print(string_to_wstring(ofVAArgsToString(format, args)));
	m_parent->print(L"\n");
}

// OTHER UTIL

//--------------------------------------------------------------
bool isBalanced(wstring s) {
	int balance = 0;
	for(wstring::iterator i = s.begin(); i != s.end(); i++) {
		switch(*i) {
			case '(':
				balance++;
				break;
			case ')':
				balance--;
				break;
		}
		if(balance < 0) {
			return false;
		}
	}
	return balance == 0;
}

//--------------------------------------------------------------
bool isEmpty(wstring s) {
    const wstring ws = L" \t\n\r";
	for(wstring::iterator i = s.begin(); i != s.end(); i++) {
		if(ws.find(*i) == string::npos) {
			return false;
		}
	}
	return true;
}
