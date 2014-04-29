/**
	\file room_template.h
	Template defination of rooms
*/

#ifndef DUNGEON_ROOM_TEMPLATE_H
#define DUNGEON_ROOM_TEMPLATE_H

#include <stdio.h>
#include "cJSON.h"

/**
	\def MODULE_INTERFACE_SYMB
	Room module interface symbol. Every room module should exports this symbol to describe itself.
 */
#define MODULE_INTERFACE_SYMB	mod_interface
/** String defination of MODULE_INTERFACE_SYMB */
#define MODULE_INTERFACE_SYMB_STR	"mod_interface"

/** Room module interface struct defination */
typedef struct {
	/** Points to room initializer of room module.
		\param d Should be a dungeon_t*
		\param config Should be the room configuration extracted from global conf
	 */
	int (*mod_initializer)(void /*dungeon_t*/ *d, cJSON *config);
	/** Points to room destroier of room module.
		\param d Should be a dungeon_t*
	 */
	int (*mod_destroier)(void *d);
	/** TODO: Points to room maintainer of room module.
		\param d Should be a dungeon_t*
	 */
	void (*mod_maintainer)(void*);
	/** Points to room info function of room module which reports room status in JSON.
		\param d Should be a dungeon_t*
	 */
	cJSON *(*mod_serialize)(void*);
} module_interface_t;

#endif

