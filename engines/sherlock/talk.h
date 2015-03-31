/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef SHERLOCK_TALK_H
#define SHERLOCK_TALK_H

#include "common/scummsys.h"
#include "common/array.h"
#include "common/stream.h"
#include "common/stack.h"

namespace Sherlock {

struct SavedSequence {
	int _objNum;
	Common::Array<byte> _sequences;
};

struct Statement {
	Common::String _statement;
	Common::String _reply;
	Common::String _linkFile;
	Common::String _voiceFile;
	Common::Array<int> _required;
	Common::Array<int> _modified;
	int _portraitSide;
	int _quotient;
	int _talkMap;

	void synchronize(Common::SeekableReadStream &s, bool voices);
};

class SherlockEngine;

class Talk {

private:
	SherlockEngine *_vm;
	int _saveSeqNum;
	Common::Array<SavedSequence> _savedSequences;
	Common::Stack<int> _sequenceStack;
	Common::Array<Statement> _statements;
	int _speaker;
	int _talkIndex;
	int _talkTo;

	void pullSequence();

	void loadTalkFile(const Common::String &filename);

	void clearTalking();

	void setTalkMap();
public:
	bool _talkToAbort;
	int _talkCounter;
public:
	Talk(SherlockEngine *vm);

	void talkTo(const Common::String &filename);

	void talk(int objNum);

	void freeTalkVars();
};

} // End of namespace Sherlock

#endif
