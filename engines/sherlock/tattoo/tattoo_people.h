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
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef SHERLOCK_TATTOO_PEOPLE_H
#define SHERLOCK_TATTOO_PEOPLE_H

#include "common/scummsys.h"
#include "common/stack.h"
#include "sherlock/people.h"

namespace Sherlock {

class SherlockEngine;

namespace Tattoo {

// Animation sequence identifiers for characters
enum TattooSequences {
	// Walk Sequences Numbers for NPCs
	WALK_UP			= 0,
	WALK_UPRIGHT		= 1,
	WALK_RIGHT		= 2,
	WALK_DOWNRIGHT	= 3,
	WALK_DOWN		= 4,
	WALK_DOWNLEFT	= 5,
	WALK_LEFT		= 6,
	WALK_UPLEFT		= 7,

	// Stop Sequences Numbers for NPCs
	STOP_UP			= 8,
	STOP_UPRIGHT		= 9,
	STOP_RIGHT		= 10,
	STOP_DOWNRIGHT	= 11,
	STOP_DOWN		= 12,
	STOP_DOWNLEFT	= 13,
	STOP_LEFT		= 14,
	STOP_UPLEFT		= 15,

	// NPC Talk Sequence Numbers
	TALK_UPRIGHT		= 16,
	TALK_RIGHT		= 17,
	TALK_DOWNRIGHT	= 18,
	TALK_DOWNLEFT	= 19,
	TALK_LEFT		= 20,
	TALK_UPLEFT		= 21,

	// NPC Listen Sequence Numbers
	LISTEN_UPRIGHT	= 22,      
	LISTEN_RIGHT		= 23,
	LISTEN_DOWNRIGHT	= 24,
	LISTEN_DOWNLEFT	= 25,
	LISTEN_LEFT		= 26,
	LISTEN_UPLEFT	= 27
};

enum NpcPath {
	NPCPATH_SET_DEST		= 1,
	NPCPATH_PAUSE			= 2,
	NPCPATH_SET_TALK_FILE	= 3,
	NPCPATH_CALL_TALK_FILE	= 4,
	NPCPATH_TAKE_NOTES		= 5,
	NPCPATH_FACE_HOLMES		= 6,
	NPCPATH_PATH_LABEL		= 7,
	NPCPATH_GOTO_LABEL		= 8,
	NPCPATH_IFFLAG_GOTO_LABEL = 9
};

struct SavedNPCPath {
	byte _path[MAX_NPC_PATH];
	int _npcIndex;
	int _npcPause;
	Common::Point _walkDest;
	int _npcFacing;
	bool _lookHolmes;

	SavedNPCPath();
	SavedNPCPath(byte path[MAX_NPC_PATH], int npcIndex, int npcPause, const Common::Point &walkDest,
		int npcFacing, bool lookHolmes);
};

class TattooPerson: public Person {
private:
	Point32 _nextDest;
private:
	bool checkCollision() const;

	/**
	 * Free the alternate graphics used by NPCs
	 */
	void freeAltGraphics();
protected:
	/**
	 * Get the source position for a character potentially affected by scaling
	 */
	virtual Common::Point getSourcePoint() const;
public:
	Common::Stack<SavedNPCPath> _pathStack;
	int _npcIndex;
	int _npcPause;
	byte _npcPath[MAX_NPC_PATH];
	Common::String _npcName;
	bool _npcMoved;
	int _npcFacing;
	bool _resetNPCPath;
	int _savedNpcSequence;
	int _savedNpcFrame;
	int _tempX;
	int _tempScaleVal;
	bool _updateNPCPath;
	bool _lookHolmes;
public:
	TattooPerson();
	virtual ~TattooPerson() {}

	/**
	 * Clear the NPC related data
	 */
	void clearNPC();

	/**
	 * Called from doBgAnim to move NPCs along any set paths. If an NPC is paused in his path,
	 * he will remain paused until his pause timer runs out. If he is walking somewhere,
	 * he will continue walking there until he reaches the dest position. When an NPC stops moving,
	 * the next element of his path is processed.
	 *
	 * The path is an array of bytes with control codes followed by their parameters as needed.
	 */
	void updateNPC();

	/**
	 * Push the NPC's path data onto the path stack for when a talk file moves the NPC that
	 * has some control codes.
	 */
	void pushNPCPath();

	/**
	 * Pull an NPC's path data that has been previously saved on the path stack for that character.
	 * There are two possibilities for when the NPC was interrupted, and both are handled differently:
	 * 1) The NPC was paused at a position
	 * If the NPC didn't move, we can just restore his pause counter and exit. But if he did move,
	 * he must return to that position, and the path index must be reset to the pause he was executing.
	 * This means that the index must be decremented by 3
	 * 2) The NPC was in route to a position
	 * He must be set to walk to that position again. This is done by moving the path index
	 * so that it points to the code that set the NPC walking there in the first place. 
	 * The regular calls to updateNPC will handle the rest
	 */
	void pullNPCPath();

	/**
	 * Checks a sprite associated with an NPC to see if the frame sequence specified
	 * in the sequence number uses alternate graphics, and if so if they need to be loaded
	 */
	void checkWalkGraphics();

	/**
	 * Synchronize the data for a savegame
	 */
	void synchronize(Serializer &s);

	/**
	 * This adjusts the sprites position, as well as it's animation sequence:
	 */
	virtual void adjustSprite();

	/**
	 * Bring a moving character to a standing position
	 */
	virtual void gotoStand();

	/**
	 * Set the variables for moving a character from one poisition to another
	 * in a straight line
	 */
	virtual void setWalking();

	/**
	 * Walk to the co-ordinates passed, and then face the given direction
	 */
	virtual void walkToCoords(const Point32 &destPos, int destDir);

	/**
	 * Adjusts the frame and sequence variables of a sprite that corresponds to the current speaker
	 * so that it points to the beginning of the sequence number's talk sequence in the object's
	 * sequence buffer
	 * @param seq	Which sequence to use (if there's more than 1)
	 * @remarks		1: First talk seq, 2: second talk seq, etc.
	 */
	virtual void setObjTalkSequence(int seq);
};

class TattooPeople : public People {
public:
	TattooPeople(SherlockEngine *vm);
	virtual ~TattooPeople() {}

	TattooPerson &operator[](PeopleId id) { return *(TattooPerson *)_data[id]; }
	TattooPerson &operator[](int idx) { return *(TattooPerson *)_data[idx]; }

	/**
	 * If the specified speaker is a background object, it will set it so that it uses 
	 * the Listen Sequence (specified by the sequence number). If the current sequence 
	 * has an Allow Talk Code in it, the _gotoSeq field will be set so that the object 
	 * begins listening as soon as it hits the Allow Talk Code. If there is no Abort Code, 
	 * the Listen Sequence will begin immediately.
	 * @param speaker		Who is speaking
	 * @param sequenceNum	Which listen sequence to use
	 */
	void setListenSequence(int speaker, int sequenceNum = 1);

	/**
	 * Restore any saved NPC walk path data from any of the NPCs
	 */
	void pullNPCPaths();

	/**
	 * Finds the scene background object corresponding to a specified speaker
	 */
	virtual int findSpeaker(int speaker);

	/**
	 * Synchronize the data for a savegame
	 */
	virtual void synchronize(Serializer &s);

	/**
	 * Change the sequence of the scene background object associated with the specified speaker.
	 */
	virtual void setTalkSequence(int speaker, int sequenceNum = 1);

	/**
	 * Load the walking images for Sherlock
	 */
	virtual bool loadWalk();

	/**
	 * Restrict passed point to zone using Sherlock's positioning rules
	 */
	virtual const Common::Point restrictToZone(int zoneId, const Common::Point &destPos);
};

} // End of namespace Scalpel

} // End of namespace Sherlock


#endif
