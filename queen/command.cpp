/* ScummVM - Scumm Interpreter
 * Copyright (C) 2003-2004 The ScummVM project
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * $Header$
 *
 */

#include "stdafx.h"
#include "queen/command.h"

#include "queen/display.h"
#include "queen/input.h"
#include "queen/graphics.h"
#include "queen/grid.h"
#include "queen/logic.h"
#include "queen/queen.h"
#include "queen/sound.h"
#include "queen/state.h"
#include "queen/talk.h"
#include "queen/walk.h"

namespace Queen {

void CmdText::clear() {
	memset(_command, 0, sizeof(_command));
}

void CmdText::display(uint8 color) {
	_vm->display()->textCurrentColor(color);
	_vm->display()->setTextCentered(COMMAND_Y_POS, _command, false);
}

void CmdText::displayTemp(uint8 color, bool locked, Verb v, const char *name) {
	char temp[MAX_COMMAND_LEN];
	if (locked) {
		sprintf(temp, "%s%s", _vm->logic()->joeResponse(39), _vm->logic()->verbName(v));
	} else {
		strcpy(temp, _vm->logic()->verbName(v));
	}
	if (name != NULL) {
		strcat(temp, " ");
		strcat(temp, name);
	}
	_vm->display()->textCurrentColor(color);
	_vm->display()->setTextCentered(COMMAND_Y_POS, temp, false);
}

void CmdText::displayTemp(uint8 color, const char *name) {
	char temp[MAX_COMMAND_LEN];
	sprintf(temp, "%s %s", _command, name);
	_vm->display()->textCurrentColor(color);
	_vm->display()->setTextCentered(COMMAND_Y_POS, temp, false);
}

void CmdText::setVerb(Verb v) {
	strcpy(_command, _vm->logic()->verbName(v));
}

void CmdText::addLinkWord(Verb v) {
	strcat(_command, " ");
	strcat(_command, _vm->logic()->verbName(v));
}

void CmdText::addObject(const char *objName) {
	strcat(_command, " ");
	strcat(_command, objName);
}

bool CmdText::isEmpty() const {
	return _command[0] == 0;
}

void CmdState::init() {
	commandLevel = 1;
	oldVerb = verb = action = VERB_NONE;
	oldNoun = noun = subject[0] = subject[1] = 0;
	
	selAction = defaultVerb = VERB_NONE;
	selNoun = 0;
}

Command::Command(QueenEngine *vm)
	: _vm(vm) {

	_cmdText._vm = vm;
}

void Command::clear(bool clearTexts) {
	_cmdText.clear();
	if (clearTexts) {
		_vm->display()->clearTexts(CmdText::COMMAND_Y_POS, CmdText::COMMAND_Y_POS);
	}
	_parse = false;
	_state.init();
}

void Command::executeCurrentAction() {
	_vm->logic()->entryObj(0);

	if (_mouseKey == Input::MOUSE_RBUTTON && _state.subject[0] > 0) {

		ObjectData *od = _vm->logic()->objectData(_state.subject[0]);
		if (od == NULL || od->name <= 0) {
			cleanupCurrentAction();
			return;
		}

		_state.verb = State::findDefaultVerb(od->state);
		_state.selAction = (_state.verb == VERB_NONE) ? VERB_WALK_TO : _state.verb;
		_cmdText.setVerb(_state.selAction);
		_cmdText.addObject(_vm->logic()->objectName(od->name));
	}

	// make sure that command is always highlighted when actioned!
	_cmdText.display(INK_CMD_SELECT);

	_state.selNoun = _state.noun;
	_state.commandLevel = 1;

	if (handleWrongAction()) {
		cleanupCurrentAction();
		return;
	}

	// get the commands associated with object/item
	uint16 comMax = 0;
	uint16 matchingCmds[MAX_MATCHING_CMDS];
	CmdListData *cmdList = &_cmdList[1];
	uint16 i;
	for (i = 1; i <= _numCmdList; ++i, ++cmdList) {
		if (cmdList->match(_state.selAction, _state.subject[0], _state.subject[1])) {
			matchingCmds[comMax] = i;
			++comMax;
		}
	}

	debug(6, "Command::executeCurrentAction() - comMax=%d subj1=%X subj2=%X", comMax, _state.subject[0], _state.subject[1]);

	if (comMax == 0) {
		sayInvalidAction(_state.selAction, _state.subject[0], _state.subject[1]);
		cleanupCurrentAction();
		return;
	}

	// process each associated command for the Object, until all done
	// or one of the Gamestate tests fails...
	int16 cond = 0;
	CmdListData *com = &_cmdList[0];
	uint16 comId = 0;
	for (i = 1; i <= comMax; ++i) {

		comId = matchingCmds[i - 1];
		com = &_cmdList[comId];

		// check the Gamestates and set them if necessary
		cond = 0;
		if (com->setConditions) {
			cond = setConditions(comId, (i == comMax));
		}

		if (cond == -1 && i == comMax) {
			// only exit on a condition fail if at last command
			// Joe hasnt spoken, so do normal LOOK command
			break;
		} else if (cond == -2 && i == comMax) {
			// only exit on a condition fail if at last command
			// Joe has spoken, so skip LOOK command
			cleanupCurrentAction();
			return;
		} else if (cond >= 0) {
			// we've had a successful Gamestate check, so we must now exit
			cond = executeCommand(comId, cond);
			break;
		}
	}

	if (cond <= 0 && _state.selAction == VERB_LOOK_AT) {
		lookAtSelectedObject();
	} else {
		// only play song if it's a PLAY AFTER type
		if (com->song < 0) {
			_vm->sound()->playSong(-com->song);
		}
		clear(true);
	}
	cleanupCurrentAction();
}

void Command::updatePlayer() {	
	if (_vm->logic()->joeWalk() != JWM_MOVE) {
		int16 cx = _vm->input()->mousePosX();
		int16 cy = _vm->input()->mousePosY();
		lookForCurrentObject(cx, cy);
		lookForCurrentIcon(cx, cy);
	}

	if (_vm->input()->keyVerb() != VERB_NONE) {

		if (_vm->input()->keyVerb() == VERB_USE_JOURNAL) {
			_vm->input()->clearKeyVerb();
			_vm->logic()->useJournal();
		} else if (_vm->input()->keyVerb() != VERB_SKIP_TEXT) {
			_state.verb = _vm->input()->keyVerb();
			if (isVerbInv(_state.verb)) {
				_state.noun = _state.selNoun = 0;
				// Clear old noun and old verb in case we're pointing at an
				// object (noun) or item (verb) and we want to use an item
				// on it. This was the command will be redisplayed with the
				// object/item that the cursor is currently on.
				_state.oldNoun = 0;
				_state.oldVerb = VERB_NONE;
				grabSelectedItem();
			} else {
				grabSelectedVerb();
			}
			_vm->input()->clearKeyVerb();
		}
	}

	_mouseKey = _vm->input()->mouseButton();
	_vm->input()->clearMouseButton();
	if (_mouseKey > 0) {
		grabCurrentSelection();
	}
}

void Command::readCommandsFrom(byte *&ptr) {
	uint16 i;

	_numCmdList = READ_BE_UINT16(ptr); ptr += 2;
	_cmdList = new CmdListData[_numCmdList + 1];
	if (_numCmdList == 0) {
		_cmdList[0].readFromBE(ptr);
	} else {
		memset(&_cmdList[0], 0, sizeof(CmdListData));
		for (i = 1; i <= _numCmdList; i++) {
			_cmdList[i].readFromBE(ptr);
		}
	}
	
	_numCmdArea = READ_BE_UINT16(ptr); ptr += 2;
	_cmdArea = new CmdArea[_numCmdArea + 1];
	if (_numCmdArea == 0) {
		_cmdArea[0].readFromBE(ptr);
	} else {
		memset(&_cmdArea[0], 0, sizeof(CmdArea));
		for (i = 1; i <= _numCmdArea; i++) {
			_cmdArea[i].readFromBE(ptr);
		}
	}

	_numCmdObject = READ_BE_UINT16(ptr); ptr += 2;
	_cmdObject = new CmdObject[_numCmdObject + 1];
	if (_numCmdObject == 0) {
		_cmdObject[0].readFromBE(ptr);
	} else {
		memset(&_cmdObject[0], 0, sizeof(CmdObject));
		for (i = 1; i <= _numCmdObject; i++) {
			_cmdObject[i].readFromBE(ptr);
		}
	}

	_numCmdInventory = READ_BE_UINT16(ptr);	ptr += 2;
	_cmdInventory = new CmdInventory[_numCmdInventory + 1];
	if (_numCmdInventory == 0) {
		_cmdInventory[0].readFromBE(ptr);
	} else {
		memset(&_cmdInventory[0], 0, sizeof(CmdInventory));
		for (i = 1; i <= _numCmdInventory; i++) {
			_cmdInventory[i].readFromBE(ptr);
		}
	}

	_numCmdGameState = READ_BE_UINT16(ptr);	ptr += 2;
	_cmdGameState = new CmdGameState[_numCmdGameState + 1];
	if (_numCmdGameState == 0) {
		_cmdGameState[0].readFromBE(ptr);
	} else {
		memset(&_cmdGameState[0], 0, sizeof(CmdGameState));
		for (i = 1; i <= _numCmdGameState; i++) {
			_cmdGameState[i].readFromBE(ptr);
		}
	}
}

ObjectData *Command::findObjectData(uint16 objRoomNum) const {
	ObjectData *od = NULL;
	if (objRoomNum != 0) {
		objRoomNum += _vm->logic()->currentRoomData();
		od = _vm->logic()->objectData(objRoomNum);
	}
	return od;
}

ItemData *Command::findItemData(Verb invNum) const {
	ItemData *id = NULL;
	uint16 itNum = _vm->logic()->findInventoryItem(invNum - VERB_INV_FIRST);
	if (itNum != 0) {
		id = _vm->logic()->itemData(itNum);
	}
	return id;
}

int16 Command::executeCommand(uint16 comId, int16 condResult) {
	// execute.c l.313-452
	debug(6, "Command::executeCommand() - cond = %X, com = %X", condResult, comId);

	CmdListData *com = &_cmdList[comId];

	if (com->setAreas) {
		setAreas(comId);
	}

	// Don't grab if action is TALK or WALK
	if (_state.selAction != VERB_TALK_TO && _state.selAction != VERB_WALK_TO) {
		int i;
		for  (i = 0; i < 2; ++i) {
			int16 obj = _state.subject[i];
			if (obj > 0) {
				_vm->logic()->joeGrab(State::findGrab(_vm->logic()->objectData(obj)->state));
			}
		}
	}

	bool cutDone = false;
	if (condResult > 0) {
		// check for cutaway/dialogs before updating Objects
		const char *desc = _vm->logic()->objectTextualDescription(condResult);
		if (executeIfCutaway(desc)) {
			condResult = 0;
			cutDone = true;
		} else if (executeIfDialog(desc)) {
			condResult = 0;
		}
	}

	int16 oldImage = 0;
	if (_state.subject[0] > 0) {
		// an object (not an item)
		oldImage = _vm->logic()->objectData(_state.subject[0])->image;
	}

	if (com->setObjects) {
		setObjects(comId);
	}

	if (com->setItems) {
		setItems(comId);
	}

	if (com->imageOrder != 0 && _state.subject[0] > 0) {
		ObjectData *od = _vm->logic()->objectData(_state.subject[0]);
		// we must update the graphic image of the object
		if (com->imageOrder < 0) {
			// instead of setting to -1 or -2, flag as negative
			if (od->image > 0) {
				// make sure that object is not already updated
				od->image = -(od->image + 10);
			}
		} else {
			od->image = com->imageOrder;
		}
		_vm->graphics()->refreshObject(_state.subject[0]);
	} else {
		// this object is not being updated by command list, see if
		// it has another image copied to it
		if (_state.subject[0] > 0) {
			// an object (not an item)
			if (_vm->logic()->objectData(_state.subject[0])->image != oldImage) {
				_vm->graphics()->refreshObject(_state.subject[0]);
			}
		}
	}

	// don't play music on an OPEN/CLOSE command - in case the command fails
	if (_state.selAction != VERB_NONE && 
		_state.selAction != VERB_OPEN && 
		_state.selAction != VERB_CLOSE) {
		// only play song if it's a PLAY BEFORE type
		if (com->song > 0) {
			_vm->sound()->playSong(com->song);
		}
	}

	// do a special hardcoded section
	// l.419-452 execute.c
	switch (com->specialSection) {
	case 1:
		_vm->logic()->useJournal();
		return condResult;
	case 2:
		_vm->logic()->joeUseDress(true);
		break;
	case 3:
		_vm->logic()->joeUseClothes(true);
		break;
	case 4:
		_vm->logic()->joeUseUnderwear();
		break;
	}

	if (_state.subject[0] > 0)
		changeObjectState(_state.selAction, _state.subject[0], com->song, cutDone);

	// execute.c l.533-548
	// FIXME: useless test, as .dog has already been played
	// if (_state.selAction == VERB_TALK_TO && cond > 0) {
	//	if (executeIfDialog(_vm->logic()->objectTextualDescription(cond))) {
	//		cleanupCurrentAction();
	//		return;
	//	}
	// }

	// execute.c l.550-589
	// FIXME: the EXECUTE_EXIT1 stuff can be omitted as it is 
	//        more or less redundant code
	if (condResult > 0) {
		_vm->logic()->makeJoeSpeak(condResult, true);
	}
	return condResult;
}

int16 Command::makeJoeWalkTo(int16 x, int16 y, int16 objNum, Verb v, bool mustWalk) {
	// Check to see if object is actually an exit to another
	// room. If so, then set up new room
	ObjectData *objData = _vm->logic()->objectData(objNum);
	if (objData->x != 0 || objData->y != 0) {
		x = objData->x;
		y = objData->y;
	}

	if (v == VERB_WALK_TO) {
		_vm->logic()->entryObj(objData->entryObj);
		if (objData->entryObj != 0) {
			_vm->logic()->newRoom(_vm->logic()->objectData(objData->entryObj)->room);
			// because this is an exit object, see if there is
			// a walk off point and set (x,y) accordingly
			WalkOffData *wod = _vm->logic()->walkOffPointForObject(objNum);
			if (wod != NULL) {
				x = wod->x;
				y = wod->y;
			}
		}
	} else {
		_vm->logic()->entryObj(0);
		_vm->logic()->newRoom(0);
	}

	debug(6, "Command::makeJoeWalkTo() - x=%d y=%d newRoom=%d", x, y, _vm->logic()->newRoom());

	int16 p = 0;
	if (mustWalk) {
		// determine which way for Joe to face Object
		uint16 facing = State::findDirection(objData->state);

		BobSlot *bobJoe  = _vm->graphics()->bob(0);
		if (x == bobJoe->x && y == bobJoe->y) {
			_vm->logic()->joeFacing(facing);
			_vm->logic()->joeFace();
		} else {
			p = _vm->walk()->moveJoe(facing, x, y, false);
			if (p != 0) {
				_vm->logic()->newRoom(0); // cancel makeJoeWalkTo, that should be equivalent to cr10 fix
			}			
		}
	}
	return p;
}

void Command::grabCurrentSelection() {
	_selPosX = _vm->input()->mousePosX();
	_selPosY = _vm->input()->mousePosY();

	uint16 zone = _vm->grid()->findObjectUnderCursor(_selPosX, _selPosY);
	_state.noun = _vm->grid()->findObjectNumber(zone);
	_state.verb = _vm->grid()->findVerbUnderCursor(_selPosX, _selPosY);

	_selPosX += _vm->display()->horizontalScroll();

	if (_state.verb == VERB_SCROLL_UP || _state.verb == VERB_SCROLL_DOWN) {
		// move through inventory (by four if right mouse button)
		uint16 scroll = (_mouseKey == Input::MOUSE_RBUTTON) ? 4 : 1;
		_vm->logic()->inventoryScroll(scroll, _state.verb == VERB_SCROLL_UP);
	} else if (isVerbAction(_state.verb)) {
		grabSelectedVerb();
	} else if (isVerbInv(_state.verb)) {
		grabSelectedItem();
	} else if (_state.noun != 0) { // 0 && _state.noun <= _vm->logic()->currentRoomObjMax()) {
		grabSelectedNoun();
	} else if (_selPosY < ROOM_ZONE_HEIGHT && _state.verb == VERB_NONE) {
		// select without a command, do a WALK
		clear(true);
		_vm->logic()->joeWalk(JWM_EXECUTE);
	}
}

void Command::grabSelectedObject(int16 objNum, uint16 objState, uint16 objName) {
	if (_state.action != VERB_NONE) {
		_cmdText.addObject(_vm->logic()->objectName(objName));
	}

	_state.subject[_state.commandLevel - 1] = objNum;

	// if first noun and it's a 2 level command then set up action word
	if (_state.action == VERB_USE && _state.commandLevel == 1) {
		if (State::findUse(objState) == STATE_USE_ON) {
			// object supports 2 levels, command not fully constructed
			_state.commandLevel = 2;
			_cmdText.addLinkWord(VERB_PREP_WITH);
			_cmdText.display(INK_CMD_NORMAL);
			_parse = false;
		} else {
//			_cmdText.display(INK_CMD_SELECT);
			_parse = true;
		}
	} else if (_state.action == VERB_GIVE && _state.commandLevel == 1) {
		// command not fully constructed
		_state.commandLevel = 2;
		_cmdText.addLinkWord(VERB_PREP_TO);		 
		_cmdText.display(INK_CMD_NORMAL);
		_parse = false;
	} else {
//		_cmdText.display(INK_CMD_SELECT);
		_parse = true;
	}

	if (_parse) {
		_state.verb = VERB_NONE;
//		_vm->logic()->newRoom(0);
		_vm->logic()->joeWalk(JWM_EXECUTE);
		_state.selAction = _state.action;
		_state.action = VERB_NONE;
	}
}

void Command::grabSelectedItem() {
//	_parse = true;

	ItemData *id = findItemData(_state.verb);
	if (id == NULL || id->name <= 0) {
		return;
	}

	int16 item = _vm->logic()->findInventoryItem(_state.verb - VERB_INV_FIRST);

	// If we've selected via keyboard, and there is no VERB then do
	// the ITEMs default, otherwise keep constructing!

	if (_mouseKey == Input::MOUSE_LBUTTON ||
		(_vm->input()->keyVerb() != VERB_NONE && _state.verb != VERB_NONE)) {
		if (_state.action == VERB_NONE) {
			if (_vm->input()->keyVerb() != VERB_NONE) {
				// We've selected via the keyboard, no command is being 
				// constructed, so we shall find the item's default
				_state.verb = State::findDefaultVerb(id->state);
				if (_state.verb == VERB_NONE) {
					// set to Look At
					_state.verb = VERB_LOOK_AT;
					_cmdText.setVerb(VERB_LOOK_AT);
				}
				_state.action = _state.verb;
			} else {
				// Action>0 ONLY if command has been constructed 
				// Left Mouse Button pressed just do Look At     
				_state.action = VERB_LOOK_AT;
				_cmdText.setVerb(VERB_LOOK_AT);
			}
		}
		_state.verb = VERB_NONE;
	} else {
//		if (_vm->logic()->joeWalk() == JWM_MOVE) {
//			_cmdText.clear();
//			_state.commandLevel = 1;
//			_vm->logic()->joeWalk(JWM_NORMAL);
//			_state.action = VERB_NONE;
//			lookForCurrentIcon();
//		}

		if (_state.defaultVerb != VERB_NONE) {
			State::alterDefaultVerb(&id->state, _state.defaultVerb);
			_state.defaultVerb = VERB_NONE;
			clear(true);
			return;
		}

		if (_cmdText.isEmpty()) {
			_state.verb = VERB_LOOK_AT;
			_state.action = VERB_LOOK_AT;
			_cmdText.setVerb(VERB_LOOK_AT);
		} else {
			if (_state.commandLevel == 2 && _parse)
				_state.verb = _state.action;
			else
				_state.verb = State::findDefaultVerb(id->state);
			if (_state.verb == VERB_NONE) {
				// No match made, so command not yet completed. Redefine as LOOK AT
				_state.action = VERB_LOOK_AT;
				_cmdText.setVerb(VERB_LOOK_AT);
			} else {
				_state.action = _state.verb;
			}
			_state.verb = VERB_NONE;
		}
	}

	grabSelectedObject(-item, id->state, id->name);
}

void Command::grabSelectedNoun() {
	// if the NOUN has been selected from screen then it is positive
	// otherwise it has been selected from inventory and is negative
	// set PARSE to TRUE, default FALSE if command half complete
	// click object without a command, if DEFAULT then
	// do that, otherwise do a WALK!

	ObjectData *od = findObjectData(_state.noun);
	if (od == NULL || od->name <= 0) {
		// selected a turned off object, so just walk
		clear(true);
		_state.noun = 0;
//		_vm->logic()->newRoom(0);
		_vm->logic()->joeWalk(JWM_EXECUTE);
		return;
	}

	if (_state.verb == VERB_NONE) {
		if (_mouseKey == Input::MOUSE_LBUTTON) {
			if ((_state.commandLevel != 2 && _state.action == VERB_NONE) || 
				(_state.commandLevel == 2 && _parse)) {
					// action2 > 0 only if command has been constructed
					// lmb pressed, just walk
					_state.verb = VERB_WALK_TO;
					_state.action = VERB_WALK_TO;
					_cmdText.setVerb(VERB_WALK_TO);
			}
		} else if (_mouseKey == Input::MOUSE_RBUTTON) {

			// rmb pressed, do default if one exists
			if (_state.defaultVerb != VERB_NONE) {
				// change default of command				
				State::alterDefaultVerb(&od->state, _state.defaultVerb);
				_state.defaultVerb = VERB_NONE;
				clear(true);
				return;
			}

			if (_cmdText.isEmpty()) {
				// Ensures that Right Mkey will select correct default
				_state.verb = State::findDefaultVerb(od->state);
				_state.selAction = (_state.verb == VERB_NONE) ? VERB_WALK_TO : _state.verb;
				_cmdText.setVerb(_state.selAction);
				_cmdText.addObject(_vm->logic()->objectName(od->name));
			} else {
				_state.verb = VERB_NONE;
				if ((_state.commandLevel == 2 && !_parse) || _state.action != VERB_NONE) {
					_state.verb = _state.action;
				} else {
					_state.verb = State::findDefaultVerb(od->state);
				}
				_state.action = (_state.verb == VERB_NONE) ? VERB_WALK_TO : _state.verb;
				_state.verb = VERB_NONE;
			}
		}
	}

	_state.selNoun = 0;
	int16 objNum = _vm->logic()->currentRoomData() + _state.noun;
	grabSelectedObject(objNum, od->state, od->name);
}

void Command::grabSelectedVerb() {
	_state.action = _state.verb;
	_state.subject[0] = 0;
	_state.subject[1] = 0;

	// if right mouse key selected, then store command VERB
	if (_mouseKey == Input::MOUSE_RBUTTON) {
		_state.defaultVerb = _state.verb;
		_cmdText.displayTemp(11, true, _state.verb);
	}
	else {
		_state.defaultVerb = VERB_NONE;
		if (_vm->logic()->joeWalk() == JWM_MOVE && _state.verb != VERB_NONE) {
			_vm->logic()->joeWalk(JWM_NORMAL);
		}
		_state.commandLevel = 1;
		_state.oldVerb = VERB_NONE;
		_state.oldNoun = 0;
		_cmdText.setVerb(_state.verb);
		_cmdText.display(INK_CMD_NORMAL);
	}
}

bool Command::executeIfCutaway(const char *description) {
	if (strlen(description) > 4 && 
		scumm_stricmp(description + strlen(description) - 4, ".cut") == 0) {

		_vm->display()->clearTexts(CmdText::COMMAND_Y_POS, CmdText::COMMAND_Y_POS);

		char nextCutaway[20];
		memset(nextCutaway, 0, sizeof(nextCutaway));
		_vm->logic()->playCutaway(description, nextCutaway);
		while (nextCutaway[0] != '\0') {
			_vm->logic()->playCutaway(nextCutaway, nextCutaway);
		}
		return true;
	}
	return false;
}

bool Command::executeIfDialog(const char *description) {
	if (strlen(description) > 4 && 
		scumm_stricmp(description + strlen(description) - 4, ".dog") == 0) {

		_vm->display()->clearTexts(CmdText::COMMAND_Y_POS, CmdText::COMMAND_Y_POS);

		char cutaway[20];
		memset(cutaway, 0, sizeof(cutaway));
		_vm->logic()->startDialogue(description, _state.selNoun, cutaway);

		while (cutaway[0] != '\0') {
			char currentCutaway[20];
			strcpy(currentCutaway, cutaway);
			_vm->logic()->playCutaway(currentCutaway, cutaway);
		}
		return true;
	}
	return false;
}

bool Command::handleWrongAction() {
	// l.96-141 execute.c
	uint16 objMax = _vm->grid()->objMax(_vm->logic()->currentRoom());
	uint16 roomData = _vm->logic()->currentRoomData();

	// select without a command or WALK TO ; do a WALK
	if ((_state.selAction == VERB_WALK_TO || _state.selAction == VERB_NONE) && 
		(_state.selNoun > objMax || _state.selNoun == 0)) {
		if (_state.selAction == VERB_NONE) {
			_vm->display()->clearTexts(CmdText::COMMAND_Y_POS, CmdText::COMMAND_Y_POS);
		}
		_vm->walk()->moveJoe(0, _selPosX, _selPosY, false);
		return true;
	}

	// check to see if one of the objects is hidden
	int i;
	for (i = 0; i < 2; ++i) {
		int16 obj = _state.subject[i];
		if (obj > 0 && _vm->logic()->objectData(obj)->name <= 0) {
			return true;
		}
	}

	// check for USE command on exists
	if (_state.selAction == VERB_USE &&
		_state.subject[0] > 0 && _vm->logic()->objectData(_state.subject[0])->entryObj > 0) {
		_state.selAction = VERB_WALK_TO;
	}

	if (_state.selNoun > 0 && _state.selNoun <= objMax) {
		uint16 objNum = roomData + _state.selNoun;
		if (makeJoeWalkTo(_selPosX, _selPosY, objNum, _state.selAction, true) != 0) {
			return true;
		}
		if (_state.selAction == VERB_WALK_TO && _vm->logic()->objectData(objNum)->entryObj < 0) {
			return true;
		}
	}
	return false;
}

void Command::sayInvalidAction(Verb action, int16 subj1, int16 subj2) {
	// l.158-272 execute.c
	switch (action) {

	case VERB_LOOK_AT:
		lookAtSelectedObject();
		break;

	case VERB_OPEN:
		// 'it doesn't seem to open'
		_vm->logic()->makeJoeSpeak(1);
		break;

	case VERB_USE:
		if (subj1 < 0) {
			uint16 k = _vm->logic()->itemData(-subj1)->sfxDescription;
			if (k > 0) {
				_vm->logic()->makeJoeSpeak(k, true);
			} else {
				_vm->logic()->makeJoeSpeak(2);
			}
		} else {
			_vm->logic()->makeJoeSpeak(2);
		}
		break;

	case VERB_TALK_TO:
		_vm->logic()->makeJoeSpeak(24 + _vm->randomizer.getRandomNumber(2));
		break;

	case VERB_CLOSE:
		_vm->logic()->makeJoeSpeak(2);
		break;

	case VERB_MOVE:
		// 'I can't move it'
		if (subj1 > 0) {
			int16 img = _vm->logic()->objectData(subj1)->image;
			if (img == -4 || img == -3) {
				_vm->logic()->makeJoeSpeak(18);
			} else {
				_vm->logic()->makeJoeSpeak(3);
			}
		} else {
			_vm->logic()->makeJoeSpeak(3);
		}
		break;

	case VERB_GIVE:
		// 'I can't give the subj1 to subj2'
		if (subj1 < 0) {
			if (subj2 > 0) {
				int16 img = _vm->logic()->objectData(subj2)->image;
				if (img == -4 || img == -3) {
					_vm->logic()->makeJoeSpeak(27 + _vm->randomizer.getRandomNumber(2));
				}
			} else {
				_vm->logic()->makeJoeSpeak(11);
			}
		} else {
			_vm->logic()->makeJoeSpeak(12);
		}
		break;

	case VERB_PICK_UP:
		if (subj1 < 0) {
			_vm->logic()->makeJoeSpeak(14);
		} else {
			int16 img = _vm->logic()->objectData(subj2)->image;
			if (img == -4 || img == -3) {
				// Trying to get a person
				_vm->logic()->makeJoeSpeak(20);
			} else {
				// 5 : 'I can't pick that up'
				// 6 : 'I don't think I need that'
				// 7 : 'I'd rather leave it here'
				// 8 : 'I don't think I'd have any use for that'
				_vm->logic()->makeJoeSpeak(5 + _vm->randomizer.getRandomNumber(3));
			}
		}
		break;
		
	default:
		break;
	}
}

void Command::changeObjectState(Verb action, int16 obj, int16 song, bool cutDone) {
	// l.456-533 execute.c
	ObjectData *objData = _vm->logic()->objectData(obj);

	if (action == VERB_OPEN && !cutDone) {
		if (State::findOn(objData->state) == STATE_ON_ON) {
			State::alterOn(&objData->state, STATE_ON_OFF);
			State::alterDefaultVerb(&objData->state, VERB_NONE);

			// play music if it exists... (or SFX for open/close door)
			if (song != 0) {
				_vm->sound()->playSong(ABS(song));
			}

			if (objData->entryObj != 0) {
				// if it's a door, then update door that it links to
				openOrCloseAssociatedObject(action, ABS(objData->entryObj));
				objData->entryObj = ABS(objData->entryObj);
			}
		} else {
			// 'it's already open !'
			_vm->logic()->makeJoeSpeak(9);
		}
	} else if (action == VERB_CLOSE && !cutDone) {
		if (State::findOn(objData->state) == STATE_ON_OFF) {
			State::alterOn(&objData->state, STATE_ON_ON);
			State::alterDefaultVerb(&objData->state, VERB_OPEN);

			// play music if it exists... (or SFX for open/close door)
			if (song != 0) {
				_vm->sound()->playSong(ABS(song));
			}

			if (objData->entryObj != 0) {
				// if it's a door, then update door that it links to
				openOrCloseAssociatedObject(action, ABS(objData->entryObj));
				objData->entryObj = -ABS(objData->entryObj);
			}
		} else {
			// 'it's already closed !'
			_vm->logic()->makeJoeSpeak(10);
		}
	} else if (action == VERB_MOVE) {
		State::alterOn(&objData->state, STATE_ON_OFF);
	}
}

void Command::cleanupCurrentAction() {
	// l.595-597 execute.c
	_vm->logic()->joeFace();
	_state.oldNoun = 0;
	_state.oldVerb = VERB_NONE;
}

void Command::openOrCloseAssociatedObject(Verb action, int16 otherObj) {
	CmdListData *cmdList = &_cmdList[1];
	uint16 com = 0;
	uint16 i;
	for (i = 1; i <= _numCmdList; ++i, ++cmdList) {
		if (cmdList->match(action, otherObj, 0)) {
			if (cmdList->setConditions) {
				warning("using Command::openOrCloseAssociatedObject() with setConditions");
				CmdGameState *cmdGs = _cmdGameState;
				uint16 j;
				for (j = 1; j <= _numCmdGameState; ++j) {
					// FIXME: weird, why using cmdGs[i] instead of cmdGs[j] ?
					if (cmdGs[j].id == i && cmdGs[i].gameStateSlot > 0) {
						if (_vm->logic()->gameState(cmdGs[i].gameStateSlot) == cmdGs[i].gameStateValue) {
							com = i;
							break;
						}
					}
				}
			} else {
				com = i;
				break;
			}
		}
	}

	debug(6, "Command::openOrCloseAssociatedObject() - com=%X", com);

	if (com != 0) {

		cmdList = &_cmdList[com];
		ObjectData *objData = _vm->logic()->objectData(otherObj);

		if (cmdList->imageOrder != 0) {
			objData->image = cmdList->imageOrder;
		}

		if (action == VERB_OPEN) {
			if (State::findOn(objData->state) == STATE_ON_ON) {
				State::alterOn(&objData->state, STATE_ON_OFF);
				State::alterDefaultVerb(&objData->state, VERB_NONE);
				objData->entryObj = ABS(objData->entryObj);
			}
		} else if (action == VERB_CLOSE) {
			if (State::findOn(objData->state) == STATE_ON_OFF) {
				State::alterOn(&objData->state, STATE_ON_ON);
				State::alterDefaultVerb(&objData->state, VERB_OPEN);
				objData->entryObj = -ABS(objData->entryObj);
			}
		}
	}
}

int16 Command::setConditions(uint16 command, bool lastCmd) {
	debug(9, "Command::setConditions(%d, %d)", command, lastCmd);
	// Test conditions, if FAIL write &&  exit, Return -1
	// if(Joe speaks before he returns, -2 is returned
	// This way a -1 return will allow Joe to speak normal description

	uint16 temp[21];
	memset(temp, 0, sizeof(temp));
	uint16 tempInd = 0;

	int16 ret = 0;
	uint16 i;
	CmdGameState *cmdGs = &_cmdGameState[1];
	for (i = 1; i <= _numCmdGameState; ++i, ++cmdGs) {
		if (cmdGs->id == command) {
			if (cmdGs->gameStateSlot > 0) {
				if (_vm->logic()->gameState(cmdGs->gameStateSlot) != cmdGs->gameStateValue) {
					debug(6, "Command::setConditions() - GS[%d] == %d (should be %d)", cmdGs->gameStateSlot, _vm->logic()->gameState(cmdGs->gameStateSlot), cmdGs->gameStateValue);
					// failed test
					ret = i;
					break;
				}
			} else {
				temp[tempInd] = i;
				++tempInd;
			}
		}
	}

	if (ret > 0) {
		// we've failed, so see if we need to make Joe speak
		cmdGs = &_cmdGameState[ret];
		if (cmdGs->speakValue > 0 && lastCmd) {
			// check to see if fail state is in fact a cutaway
			const char *objDesc = _vm->logic()->objectTextualDescription(cmdGs->speakValue);
			if (!executeIfCutaway(objDesc) && !executeIfDialog(objDesc)) {
				_vm->logic()->makeJoeSpeak(cmdGs->speakValue, true);
			}
			ret = -2;
		} else {
			ret = -1;
		}
	} else {
		ret = 0;
		// all tests were okay, now set gamestates
		for (i = 0; i < tempInd; ++i) {
			cmdGs = &_cmdGameState[temp[i]];
			_vm->logic()->gameState(ABS(cmdGs->gameStateSlot), cmdGs->gameStateValue);
			// set return value for Joe to say something
			ret = cmdGs->speakValue;
		}
	}
	return ret;
}

void Command::setAreas(uint16 command) {
	debug(9, "Command::setAreas(%d)", command);

	CmdArea *cmdArea = &_cmdArea[1];
	uint16 i;
	for (i = 1; i <= _numCmdArea; ++i, ++cmdArea) {
		if (cmdArea->id == command) {
			uint16 areaNum = ABS(cmdArea->area);
			Area *area = _vm->grid()->area(cmdArea->room, areaNum);
			if (cmdArea->area > 0) {
				// turn on area
				area->mapNeighbours = ABS(area->mapNeighbours);
			} else {
				// turn off area
				area->mapNeighbours = -ABS(area->mapNeighbours);
			}
		}
	}
}

void Command::setObjects(uint16 command) {
	debug(9, "Command::setObjects(%d)", command);

	CmdObject *cmdObj = &_cmdObject[1];
	uint16 i;
	for (i = 1; i <= _numCmdObject; ++i, ++cmdObj) {
		if (cmdObj->id == command) {

			// found an object
			uint16 dstObj = ABS(cmdObj->dstObj);
			ObjectData *objData = _vm->logic()->objectData(dstObj);

			debug(6, "Command::setObjects() - dstObj=%X srcObj=%X _state.subject[0]=%X", cmdObj->dstObj, cmdObj->srcObj, _state.subject[0]);

			if (cmdObj->dstObj > 0) {
				// show the object
				objData->name = ABS(objData->name);
				// test that the object has not already been deleted
				// by checking if it is not equal to zero
				if (cmdObj->srcObj == -1 && objData->name != 0) {
					// delete object by setting its name to 0 and
					// turning off graphic image
					objData->name = 0;
					if (objData->room == _vm->logic()->currentRoom()) {
						if (dstObj != _state.subject[0]) {
							// if the new object we have updated is on screen and is not the 
							// current object, then we can update. This is because we turn
							// current object off ourselves by COM_LIST(com, 8)
							if (objData->image != -3 && objData->image != -4) {
								// it is a normal object (not a person)
								// turn the graphic image off for the object
								objData->image = -(objData->image + 10);
							}
						}
						// invalidate object area
						uint16 objZone = dstObj - _vm->logic()->currentRoomData();
						_vm->grid()->setZone(GS_ROOM, objZone, 0, 0, 1, 1);
					}
				}

				if (cmdObj->srcObj > 0) {
					// copy data from dummy object to object
					int16 image1 = objData->image;
					int16 image2 = _vm->logic()->objectData(cmdObj->srcObj)->image;
					_vm->logic()->objectCopy(cmdObj->srcObj, dstObj);
					if (image1 != 0 && image2 == 0 && objData->room == _vm->logic()->currentRoom()) {
						uint16 bobNum = _vm->logic()->findBob(dstObj);
						if (bobNum != 0) {
							_vm->graphics()->clearBob(bobNum);
						}
					}
				}

				if (dstObj != _state.subject[0]) {
					// if the new object we have updated is on screen and
					// is not current object then update it
					_vm->graphics()->refreshObject(dstObj);
				}
			} else {
				// hide the object
				if (objData->name > 0) {
					objData->name = -objData->name;
					// may need to turn BOBs off for objects to be hidden on current 
					// screen ! if the new object we have updated is on screen and
					// is not current object then update it
					_vm->graphics()->refreshObject(dstObj);
				}
			}
		}
	}
}

void Command::setItems(uint16 command) {
	debug(9, "Command::setItems(%d)", command);

	CmdInventory *cmdInv = &_cmdInventory[1];
	ItemData *items = _vm->logic()->itemData(0);
	uint16 i;
	for (i = 1; i <= _numCmdInventory; ++i, ++cmdInv) {
		if (cmdInv->id == command) {
			uint16 dstItem = ABS(cmdInv->dstItem);
			// found an item
			if (cmdInv->dstItem > 0) {
				// add item to inventory
				if (cmdInv->srcItem > 0) {
					// copy data from source item to item, then enable it
					items[dstItem] = items[cmdInv->srcItem];
					items[dstItem].name = ABS(items[dstItem].name);
				}
				_vm->logic()->inventoryInsertItem(cmdInv->dstItem);
			} else {
				// delete item
				if (items[dstItem].name > 0) {
					_vm->logic()->inventoryDeleteItem(dstItem);
				}
				if (cmdInv->srcItem > 0) {
					// copy data from source item to item, then disable it
					items[dstItem] = items[cmdInv->srcItem];
					items[dstItem].name = -ABS(items[dstItem].name);
				}
			}
		}
	}
}

uint16 Command::nextObjectDescription(ObjectDescription* objDesc, uint16 firstDesc) {
	// l.69-103 select.c
	uint16 i;
	uint16 diff = objDesc->lastDescription - firstDesc;
	debug(6, "Command::nextObjectDescription() - diff = %d, type = %d", diff, objDesc->type);
	switch (objDesc->type) {
	case 0:
		// random type, start with first description
		if (objDesc->lastSeenNumber == 0) {
			// first time look at called
			objDesc->lastSeenNumber = firstDesc;
			break;
		}
		// already displayed first, do a random
	case 1:
		i = objDesc->lastSeenNumber;
		while (i == objDesc->lastSeenNumber) {
			i = firstDesc + _vm->randomizer.getRandomNumber(diff);
		}
		objDesc->lastSeenNumber = i;
		break;
	case 2:
		// sequential, but loop
		++objDesc->lastSeenNumber;
		if (objDesc->lastSeenNumber > objDesc->lastDescription) {
			objDesc->lastSeenNumber = firstDesc;
		}
		break;
	case 3:
		// sequential without looping
		if (objDesc->lastSeenNumber != objDesc->lastDescription) {
			++objDesc->lastSeenNumber;
		}
		break;
	}
	return objDesc->lastSeenNumber;
}

void Command::lookAtSelectedObject() {
	uint16 desc;
	if (_state.subject[0] < 0) {
		desc = _vm->logic()->itemData(-_state.subject[0])->description;
	} else {
		ObjectData *objData = _vm->logic()->objectData(_state.subject[0]);
		if (objData->name <= 0) {
			return;
		}
		desc = objData->description;
	}

	debug(6, "Command::lookAtSelectedObject() - desc = %X, _state.subject[0] = %X", desc, _state.subject[0]);

	// check to see if the object/item has a series of description
	ObjectDescription *objDesc = _vm->logic()->objectDescription(1);
	uint16 i;
	for (i = 1; i <= _vm->logic()->objectDescriptionCount(); ++i, ++objDesc) {
		if (objDesc->object == _state.subject[0]) {
			desc = nextObjectDescription(objDesc, desc);
			break;
		}
	}

	_vm->logic()->makeJoeSpeak(desc, true);
	_vm->logic()->joeFace();
}

void Command::lookForCurrentObject(int16 cx, int16 cy) {
	uint16 obj = _vm->grid()->findObjectUnderCursor(cx, cy);
	_state.noun = _vm->grid()->findObjectNumber(obj);

	if (_state.oldNoun == _state.noun) {
		return;
	}

	ObjectData *od = findObjectData(_state.noun);
	if (od == NULL || od->name <= 0) {
		_state.oldNoun = _state.noun;
		_vm->display()->clearTexts(CmdText::COMMAND_Y_POS, CmdText::COMMAND_Y_POS);
		if (_state.defaultVerb != VERB_NONE) {
			_cmdText.displayTemp(INK_CMD_LOCK, true, _state.defaultVerb);
		} else if (_state.action != VERB_NONE) {
			_cmdText.display(INK_CMD_NORMAL);
		}
		return;
	}

	// if no command yet selected, then use DEFAULT command, if any
	if (_state.action == VERB_NONE) {
		Verb v = State::findDefaultVerb(od->state);
		_cmdText.setVerb((v == VERB_NONE) ? VERB_WALK_TO : v);
		if (_state.noun == 0) {
			_cmdText.clear();
		}
	}
	const char *name = _vm->logic()->objectName(od->name);
	if (_state.defaultVerb != VERB_NONE) {
		_cmdText.displayTemp(INK_CMD_LOCK, true, _state.defaultVerb, name);
	} else {
		_cmdText.displayTemp(INK_CMD_NORMAL, name);
	}
	_state.oldNoun = _state.noun;
}

void Command::lookForCurrentIcon(int16 cx, int16 cy) {
	_state.verb = _vm->grid()->findVerbUnderCursor(cx, cy);
	if (_state.oldVerb != _state.verb) {

		if (_state.action == VERB_NONE) {
			_cmdText.clear();
		}
		_vm->display()->clearTexts(CmdText::COMMAND_Y_POS, CmdText::COMMAND_Y_POS);

		if (isVerbInv(_state.verb)) {
			ItemData *id = findItemData(_state.verb);
			if (id != NULL && id->name > 0) {
				if (_state.action == VERB_NONE) {
					Verb v = State::findDefaultVerb(id->state);
					_cmdText.setVerb((v == VERB_NONE) ? VERB_LOOK_AT : v);
				}
				const char *name = _vm->logic()->objectName(id->name);
				if (_state.defaultVerb != VERB_NONE) {
					_cmdText.displayTemp(INK_CMD_LOCK, true, _state.defaultVerb, name);
				} else {
					_cmdText.displayTemp(INK_CMD_NORMAL, name);
				}
			}
		} else if (isVerbAction(_state.verb)) {
			_cmdText.displayTemp(INK_CMD_NORMAL, false, _state.verb);
		} else if (_state.verb == VERB_NONE) {
			_cmdText.display(INK_CMD_NORMAL);
		}
		_state.oldVerb = _state.verb;
	}
}

} // End of namespace Queen
