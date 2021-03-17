
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <conio.h>
#include <string.h>
#include <assert.h>

#define NTILES 256
#define WIDTH 16
#define HEIGHT 16

typedef unsigned char byte;
typedef unsigned char bool;
typedef unsigned short word;

#define MASK(x) (1 << (x))

typedef enum {
  WATER=0, SHALLOW, FISH, WHALE,
  FIELD=4, FRUIT, FERTILE, CROP,
  FOREST=8, ANIMAL, RUINS, VILLAGE,
  MOUNTAIN=12, METAL, CITY,
  TERRAIN_LAST,
} TerrainTypes;

const char* TERRAIN_NAMES[TERRAIN_LAST] = {
  "Water", "Water (shallow)", "Water (fish)", "Water (whale)",
  "Field", "Field (fruit)", "Field (fertile)", "Field (crop)",
  "Forest", "Forest (animal)", "Ruins", "Village",
  "Mountain", "Mountain (ore)", "City",  
};

const byte GENFREQ[16] = {
  FIELD, FIELD, FIELD, FIELD, FIELD, FERTILE, FRUIT,
  FOREST, FOREST, FOREST, FOREST, ANIMAL,
  MOUNTAIN, MOUNTAIN, MOUNTAIN, METAL,
};

const char TERRAINCHARS[16] = ".~<>'%:#!&@V^$C ";

typedef enum {
  NOBUILD=0,
  LUMBER, SAWMILL, FARM, WINDMILL,
  MINE, FORGE, PORT, CUSTOMS, TEMPLE,
  M_PEACE, M_TOMB, M_EYE, M_POWER,
  M_BAZAAR, M_FORTUNE, M_WISDOM,
} BuildingTypes;

typedef enum {
  NOUNIT=0,
  WARRIOR, RIDER, DEFENDER, SWORDSMAN,
  ARCHER, CATAPULT, KNIGHT, MINDBENDER,
  GIANT,
  BOAT, SHIP, BATTLESHIP,
  UNIT_LAST
} UnitTypes;

const char UNITCHARS[16] = " WRDSACKMG123";

const char* UNIT_NAMES[UNIT_LAST] = {
  "N/A",
  "Warrior", "Rider", "Defender", "Swordsman",
  "Archer", "Catapult", "Knight", "Mindbender",
  "Giant",
  "Boat", "Ship", "Battleship"
};

typedef enum {
  NOTECH,
  CLIMBING,
    MEDITATION, PHILOSOPHY,
    MINING, SMITHERY,
  FISHING,
    WHALING, AQUATISM,
    SAILING, NAVIGATION,
  HUNTING,
    ARCHERY, SPIRITUALISM,
    FORESTRY, MATHEMATICS,
  RIDING,
    ROADS, TRADE,
    FREESPIRIT, CHIVALRY,
  ORGANIZATION,
    FARMING, CONSTRUCTION,
    SHIELDS,
  LAST_TECH
} TechTypes;

const char* TECH_NAMES[LAST_TECH] = {
  0,
  "Climbing", "Meditation", "Philosophy", "Mining", "Smithery",
  "Fishing", "Whaling", "Aquatism", "Sailing", "Navigation",
  "Hunting", "Archery", "Spiritualism", "Forestry", "Mathematics",
  "Riding", "Roads", "Trade", "Freespirit", "Chivalry",
  "Organization", "Farming", "Construction", "Shields",
};

typedef enum {
  DASH=1, FORTIFY=2, ESCAPE=4, PERSIST=8,
  HEAL=16, CONVERT=32,
  CARRY=64, SCOUT=128,
} SkillTypes;

typedef enum {
  MOVED=128, ATTACKED=64, ESCAPED=32, BUILT=16,
} UnitFlags;

#define ALLACTIONS 0xff

typedef enum {
  CMD_CONQUER,		// on village
  CMD_NEW_UNIT,		// on city
  CMD_BUILD,		// near city
  CMD_COLLECT,		// near city
  CMD_MOVE,		// units ...
  CMD_ATTACK,
  CMD_RECOVER,
  CMD_DISBAND,
  CMD_HEAL,
  CMD_LAST,
} Actions;

const char* COMMANDS[CMD_LAST] = {
  "Conquer", "New Unit", "Build", "Gather",
  "Move", "Attack", "Recover", "Disband", "Heal"
};

typedef enum {
  NOTRIBE,
  MOUNTAINEERS, FISHERS, HUNTERS, GATHERERS, HORSEFOLK,
  LAST_TRIBE
} Tribes;

#define NPLAYERS LAST_TRIBE

// VARIABLES

byte terrain[NTILES];	// terrain types
byte built[NTILES];	// building/road (or city level + unit count)
byte citypop[NTILES];	// city population
byte fog[NTILES];	// fog bitmask
byte territory[NTILES];	// territory control
byte tmp[NTILES];       // temp array

#define CITYLEVEL(i) (built[i] & 0x0f)
#define CITYUNITS(i) (built[i] >> 4)

// unit
byte utype[NTILES];	// unit type
byte uowner[NTILES];	// unit owner
byte uflags[NTILES];	// flags
byte uhealth[NTILES];	// health points (or population)
byte ucarry[NTILES]; 	// prev unit type, for boats

// player info
byte cur_player = 1;
byte player_cursor[NPLAYERS];
byte player_stars[NPLAYERS];
byte player_cities[NPLAYERS];
unsigned long player_tech[NPLAYERS];

static byte UNITHEALTH[UNIT_LAST] = {
  0, 10, 10, 15, 15, 10, 10, 15, 10, 40, 0, 0, 0
};
static byte UNITMOVE[UNIT_LAST] = {
  0, 1, 2, 1, 1, 1, 1, 3, 1, 1, 2, 3, 3
};
static byte UNITATTACK[UNIT_LAST] = {
  0, 2, 2, 1, 3, 2, 4, 3, 0, 5, 1, 2, 4
};
static byte UNITDEFENSE[UNIT_LAST] = {
  0, 2, 1, 3, 3, 1, 0, 1, 1, 4, 1, 2, 3
};
/*
static byte UNITRANGE[UNIT_LAST] = {
  0, 1, 1, 1, 1, 2, 3, 1, 1, 1, 2, 2, 2
};
static byte UNITSKILLS[UNIT_LAST] = {
  0,
  DASH|FORTIFY, DASH|FORTIFY|ESCAPE, FORTIFY, DASH|FORTIFY,
  DASH|FORTIFY, 0, DASH|FORTIFY|PERSIST, HEAL|CONVERT,
  0, DASH|CARRY, DASH|CARRY, DASH|CARRY|SCOUT
};
*/

byte is_left_border(byte ui)   { return (ui & 0x0f) == 0x00; }
byte is_right_border(byte ui)  { return (ui & 0x0f) == 0x0f; }
byte is_top_border(byte ui)    { return (ui & 0xf0) == 0x00; }
byte is_bottom_border(byte ui) { return (ui & 0xf0) == 0xf0; }
byte is_border(byte ui) {
  return is_left_border(ui) || is_right_border(ui)
      || is_top_border(ui) || is_bottom_border(ui);
}
byte is_border_2(byte ui) {
  return (ui & 0x0f) <= 0x01 || (ui & 0x0f) >= 0x0e
      || (ui & 0xf0) <= 0x10 || (ui & 0xf0) >= 0xe0;
}

void initworld(void) {
  register byte i=0;
  do {
    utype[i] = 0;
    uowner[i] = 0;
    uflags[i] = 0;
    uhealth[i] = 0;
    terrain[i] = 0;
    built[i] = 0;
    citypop[i] = 0;
    fog[i] = 0;
  } while (++i);
  memset(player_cities, 0, sizeof(player_cities));
  memset(player_stars, 0, sizeof(player_stars));
  memset(player_tech, 0, sizeof(player_tech));
}

byte detect_index;

// TODO: short circuit?
byte detectall(byte (*fn)(byte)) {
  byte i,v;
  i=0;
  do {
    v = fn(i);
    if (v) {
      detect_index = i;
      return v;
    }
  } while (++i);
  return 0;
}

byte detectone(byte (*fn)(byte), byte i) {
  byte v = fn(i);
  if (v) detect_index = i;
  return v;
}

byte detectadjacent(byte (*fn)(byte), byte i) {
  byte top = is_top_border(i);
  byte bottom = is_bottom_border(i);
  byte left = is_left_border(i);
  byte right = is_right_border(i);
  byte v = 0;
  if (!top) {
    if (!left) v |= detectone(fn, i-WIDTH-1);
    v |= detectone(fn, i-WIDTH);
    if (!right) v |= detectone(fn, i-WIDTH+1);
  }
  {
    if (!left) v |= detectone(fn, i-1);
    if (!right) v |= detectone(fn, i+1);
  }
  if (!bottom) {
    if (!left) v |= detectone(fn, i+WIDTH-1);
    v |= detectone(fn, i+WIDTH);
    if (!right) v |= detectone(fn, i+WIDTH+1);
  }
  return v;
}

// TODO: clip borders
byte detectsquare(byte (*fn)(byte), byte i, byte s) {
  byte x,y,v;
  for (y=0; y<s; y++) {
    for (x=0; x<s; x++) {
      v = fn(i);
      if (v) {
        detect_index = i;
        return v;
      }
      ++i;
    }
    i += WIDTH - s;
  }
  return 0;
}

byte detect5adjacent(byte (*fn)(byte), byte i) {
  return detectsquare(fn, i-2-WIDTH*2, 5);
}

byte det_city(byte i) {
  return terrain[i] == CITY;
}

byte det_cityorvillage(byte i) {
  return terrain[i] == CITY || terrain[i] == VILLAGE;
}

byte det_nonwater(byte i) {
  return terrain[i] > WATER;
}

byte det_myunit(byte i) {
  return uowner[i] == cur_player;
}

byte det_enemy(byte i) {
  return uowner[i] && uowner[i] != cur_player;
}

byte det_claimterritory(byte i) {
  territory[i] = cur_player;
  return 0;
}

static signed char DIROFS[4] = { -1, 1, -WIDTH, WIDTH };

byte randterrain() {
  return GENFREQ[rand() & 15];
}

byte canhavecity(byte i) {
  // can never be zero, b/c cities must be 2 spaces from border
  if (is_border_2(i))
    return 0;
  // detect city in 5x5 square
  if (detect5adjacent(det_cityorvillage, i))
    return 0;
  // can add a city/village here
  return i;
}

byte randwalktonewvillage(byte i) {
  byte j,dir;
  byte dist = 20;
  // pick an initial direction to walk
  while (--dist) {
    if (canhavecity(i)) {
      terrain[i] = VILLAGE;
      return i;
    }
    // try all 4 directions
    dir = rand();
    j = i + DIROFS[dir & 3];
    if (terrain[j] == WATER && !is_border_2(j)) {
      i = j;
      terrain[i] = randterrain();
    } else {
      dir++;
    }
  }
  return 0;
}

void newvillage(byte i) {
  while (!randwalktonewvillage(i)) ;
}

void copynewterrain() {
  memcpy(terrain, tmp, sizeof(terrain));
}

byte blurterrain() {
  byte nonwater=0;
  byte i=0;
  do {
    // only change water tiles
    if ((tmp[i] = terrain[i]) == 0) {
      // change to new tile if 8-adjacent to non-water tile
      if ((rand()&1) && detectadjacent(det_nonwater, i)) {
        tmp[i] = randterrain();
      }
    } else {
      ++nonwater;
    }
  } while (++i);
  copynewterrain();
  return nonwater;
}

byte fog_break(byte i) {
  fog[i] |= 1 << cur_player;
  return 1;
}

void breakfog(byte i) {
  fog_break(i);
  detectadjacent(fog_break, i);
}

byte canbuildunit(register byte ci) {
  // can build in our empty city?
  if (terrain[ci] == CITY && territory[ci] == cur_player && utype[ci] == 0) {
    // enough capacity for a new unit?
    if (CITYUNITS(ci) < CITYLEVEL(ci)) return 1;
  }
  return 0;
}

byte addunit(register byte ui, byte owner, byte unittype) {
  utype[ui] = unittype;
  uowner[ui] = owner;
  uhealth[ui] = UNITHEALTH[unittype];
  uflags[ui] = BUILT; // can't re-build this turn
  ucarry[ui] = 0;
  breakfog(ui);
  // increase city unit count
  built[ui] += 0x10;
  return 1; // TODO?
}

void removeunit(register byte ui) {
  utype[ui] = 0;
  uowner[ui] = 0;
  uhealth[ui] = 0;
  uflags[ui] = 0;
  ucarry[ui] = 0;
}

void moveunit(register byte srci, byte desti) {
  utype[desti] = utype[srci];
  uhealth[desti] = uhealth[srci];
  uowner[desti] = uowner[srci];
  uflags[desti] = uflags[srci] | MOVED;
  ucarry[desti] = ucarry[srci];
  utype[srci] = uhealth[srci] = uowner[srci] = uflags[srci] = ucarry[srci] = 0;
  breakfog(desti);
}

void claimcity(register byte ci) {
  if (terrain[ci] != CITY) {
    built[ci] = 1; // reset city level
  }
  terrain[ci] = CITY;
  uflags[ci] |= ALLACTIONS;
  citypop[ci] = 0; // TODO: reset population?
  player_cities[cur_player]++;
  // TODO: compute territory based on city radius
  territory[ci] = cur_player;
  detectadjacent(det_claimterritory, ci);
  breakfog(ci);
}

byte addcapitol(byte owner) {
  byte ci;
  // try until we get a city
  do {
    ci = rand() & 0xff;
  } while (!canhavecity(ci));
  // add city
  claimcity(ci);
  // add a unit
  addunit(ci, owner, RIDER); // TODO: depends on tribe
  // add villages with land bridge
  newvillage(ci);
  newvillage(ci);
  player_cursor[owner] = ci;
  return ci;
}

void printtile(byte i) {
  if (cur_player && !(fog[i] & (1 << cur_player))) {
    cputc(' ');
  } else if (utype[i]) {
    cputc(UNITCHARS[utype[i]]);
  } else {
    cputc(TERRAINCHARS[terrain[i]]);
  }
}

byte printxtra = 0;

void printworld(void) {
  byte x,y,i;
  i=0;
  clrscr();
  for (y=0; y<HEIGHT; y++) {
    gotoxy(0, y);
    for (x=0; x<WIDTH; x++) {
      printtile(i);
      ++i;
    }
    gotoxy(20, y);
    i -= WIDTH;
    for (x=0; x<WIDTH; x++) {
      switch (printxtra & 7) {
        case 0: cputc(tmp[i] + '0'); break;
        case 1: cputc(fog[i] + '0'); break;
        case 2: cputc(built[i] + '0'); break;
        case 3: cputc(territory[i] + '0'); break;
        case 4: cputc(uowner[i] + '0'); break;
        case 5: cputc(utype[i] + '0'); break;
        case 6: cputc(uflags[i] + '0'); break;
        case 7: cputc(citypop[i] + '0'); break;
      }
      ++i;
    }
  }
}

#define MOVE_ORIGIN -128
#define CAN_ATTACK 127

void _getunitmoves(byte ui, signed char mp) {
  byte terr;
  if (tmp[ui]) return; // already computed this node
  // is this the original location?
  if (mp == MOVE_ORIGIN) {
    // compute total move score for this unit
    mp = UNITMOVE[utype[ui]] * 2 + 1;
    // mark tile visited but not moveable
    tmp[ui] = -1;
  } else {
    mp -= 2; // TODO: compute move cost
    if (mp <= 0) return; // out of move points
    tmp[ui] = mp;
    terr = terrain[ui];
    // blocked by forest or mountain
    // TODO: forest w/ roads
    if (terr == FOREST || terr == MOUNTAIN) return;
    // blocked by enemy
    if (detectadjacent(det_enemy, ui)) return;
  }
  // TODO: 8-dir?
  if (!is_left_border(ui)) _getunitmoves(ui-1, mp);
  if (!is_right_border(ui)) _getunitmoves(ui+1, mp);
  if (!is_top_border(ui)) _getunitmoves(ui-WIDTH, mp);
  if (!is_bottom_border(ui)) _getunitmoves(ui+WIDTH, mp);
}

void getunitmoves(byte ui) {
  memset(tmp, 0, sizeof(tmp));
  _getunitmoves(ui, MOVE_ORIGIN);
}

byte det_attack(byte ui) {
  if (det_enemy(ui)) {
    tmp[ui] = CAN_ATTACK;
  }
  return 0;
}

void getunitattacks(byte ui) {
  memset(tmp, 0, sizeof(tmp));
  detectadjacent(det_attack, ui);
}

byte damageunit(byte ui, byte damage) {
  byte hp = uhealth[ui];
  if (hp > damage) {
    return (uhealth[ui] = hp - damage);
  } else {
    removeunit(ui);
    return 0;
  }
}

byte getdefensebonus(byte ui) {
  ui=ui;
  return 3; // 1.5 is standard
}

// https://polytopia.fandom.com/wiki/Combat
byte attackunit(byte srci, byte desti) {
  byte atype = utype[srci];
  byte dtype = utype[desti];
  // TODO byte defenseBonus = 1.5;
  byte attackForce = UNITATTACK[atype] * uhealth[srci] / UNITHEALTH[atype];
  byte defenseForce = UNITDEFENSE[dtype] * uhealth[desti] / UNITHEALTH[dtype];
  byte totalDamage = attackForce + defenseForce * getdefensebonus(desti) / 2; 
  byte attackResult = (attackForce * UNITATTACK[atype] * 9 / totalDamage) / 2;
  byte defenseResult = (defenseForce * UNITDEFENSE[dtype] * 9 / totalDamage) / 2;
  // attack, any hp left?
  uflags[srci] |= ATTACKED;
  if (damageunit(desti, attackResult)) {
    // defender can attack
    damageunit(srci, defenseResult);
    return 2;
  } else {
    // unit killed, move into place
    moveunit(srci, desti);
    return 1;
  }
}

byte closestcity(byte ti) {
  if (detectadjacent(det_city, ti)) return detect_index;
  if (detect5adjacent(det_city, ti)) return detect_index;
  else return 0;
}

void increasecitylevel(byte ci) {
  citypop[ci] = 0;
  ++built[ci];
  // TODO: rewards
}

void increasepopulation(byte ci) {
  if (++citypop[ci] == CITYLEVEL(ci)+1) {
    increasecitylevel(ci);
  }
}

byte gathertile(byte ti) {
  byte cityi = closestcity(ti);
  assert(cityi);
  switch (terrain[ti]) {
    case ANIMAL:
    case FRUIT:
      terrain[ti] = FIELD;
      increasepopulation(cityi);
      return 1;
    case FERTILE:
      terrain[ti] = CROP;
      increasepopulation(cityi);
      increasepopulation(cityi);
      return 1;
    case METAL:
      terrain[ti] = MINE;
      increasepopulation(cityi);
      increasepopulation(cityi);
      return 1;
    case FISH:
      terrain[ti] = WATER;
      increasepopulation(cityi);
      return 1;
    case WHALE:
      terrain[ti] = WATER;
      player_stars[cur_player] += 10;
      return 1;
    default:
      assert(0);
  }
  return 0;
}

word gettilecmds(byte i) {
  word cmds = 0;
  // is this our unit?
  if (uowner[i] == cur_player) {
    // is there a unit here?
    if (utype[i]) {
      // haven't moved this turn?
      if ((uflags[i] & MOVED) == 0) {
        cmds |= MASK(CMD_MOVE);
        // claim the village?
	if (terrain[i] == VILLAGE) {
          cmds |= MASK(CMD_CONQUER);
        }
        // claim another player's city?
	if (terrain[i] == CITY && territory[i] != cur_player) {
          cmds |= MASK(CMD_CONQUER);
        }
      }
      // enemy nearby, and haven't yet attacked?
      if ((uflags[i] & ATTACKED) == 0 && detectadjacent(det_enemy, i)) {
        cmds |= MASK(CMD_ATTACK);
      }
    }
  }
  // no unit here
  // is this our city?
  if (canbuildunit(i)) {
    // new unit if no one else is on the city
    // TODO: and if carrying capacity allows
    cmds |= MASK(CMD_NEW_UNIT);
  }
  // is this our territory?
  if (territory[i] == cur_player) {
    if (terrain[i] == ANIMAL) {
      cmds |= MASK(CMD_COLLECT);
    }
    if (terrain[i] == FRUIT) {
      cmds |= MASK(CMD_COLLECT);
    }
    if (terrain[i] == CROP) {
      cmds |= MASK(CMD_COLLECT);
    }
    if (terrain[i] == FISH) {
      cmds |= MASK(CMD_COLLECT);
    }
  }
  return cmds;
}

void printcmdlist(word cmds, const char* commands[]) {
  byte k;
  for (k=0; k<16; k++) {
    if (cmds & (1<<k)) {
      revers(1);
      cputc(commands[k][0]);
      revers(0);
      cputs(commands[k]+1);
      cputc(' ');
    }
  }
}

byte prompt(word cmds, const char* commands[]) {
  byte k,ch;
  if (cmds) {
    printcmdlist(cmds, commands);
    cputs(" ?");
    while (1) {
      ch = cgetc();
      if (ch == 27) break;
      for (k=1; k<16; k++) {
        if (ch == commands[k][0] && (cmds & (1<<k))) {
          return k;
        }
      }
    }
  }
  return 0;
}

void printcommands(word cmds) {
  if (cmds) {
    printcmdlist(cmds, COMMANDS);
  } else {
    revers(1);
    cputs("ENTER");
    revers(0);
    cputs(" - End Turn");
  }
}

void printunitinfo(byte i) {
  if (utype[i]) {
    printf("%s (p%d) %d",
      UNIT_NAMES[utype[i]], uowner[i], uhealth[i]);
  }
}

void printinfo(byte i) {
  gotoxy(20,21);
  printf("PLYR %d %3d*", cur_player, player_stars[cur_player]);
  gotoxy(1,19);
  printf("%s ", TERRAIN_NAMES[terrain[i]]);
  if (territory[i]) {
    printf("(P%d) ", territory[i]);
  }
  if (terrain[i] == CITY) {
    printf(" %d/%d/%d", citypop[i], CITYUNITS(i), CITYLEVEL(i));
  }
  printf("\n");
  gotoxy(1,20);
  printunitinfo(i);
  gotoxy(1,22);
}

void movecursor(byte delta) {
  player_cursor[cur_player] += delta;
}

void uinewunit(byte ci) {
  byte newutype = WARRIOR;
  gotoxy(1,20);
  if (newutype = prompt(0xe, UNIT_NAMES)) {
    addunit(ci, cur_player, newutype);
    uflags[ci] = ALLACTIONS;
  }
}

typedef enum { MODE_SELECT, MODE_MOVE, MODE_ATTACK } UIModes;

byte gettechcost(byte tech) {
  byte ncities = player_cities[cur_player];
  byte tier = (tech-1) % 5 + 1;
  return ncities * 3 * tier;
}

void ui_techtree() {
  byte tech;
  clrscr();
  for (tech=1; tech<LAST_TECH; tech++) {
    if ((1L<<tech) & player_tech[cur_player]) revers(1);
    printf("%15s %2d  ", TECH_NAMES[tech], gettechcost(tech));
    revers(0);
    if ((tech-1) % 5 == 0) 
      printf("\n");
  }
  printf("\n\n");
  cgetc();
}

void playturn() {
  char ch;
  byte ci;
  word cmds;
  byte srci;
  bool canmove = 0;
  bool endturn = 0;
  byte uimode = MODE_SELECT;
  do {
    printworld();
    ci = player_cursor[cur_player];
    gotoxy(ci & 15, ci >> 4);
    revers(1);
    printtile(ci);
    revers(0);
    gotoxy(1, 19);
    cmds = 0;
    switch (uimode) {
      case MODE_SELECT:
        printinfo(ci);
        cmds = gettilecmds(ci);
        printcommands(cmds);
        break;
      case MODE_MOVE:
        printinfo(ci);
        canmove = (signed char)tmp[ci] > 0;
        if (canmove) {
          printf("Press SPACE to move");
        } else {
          printf("Use arrow keys to select move target");
        }
        break;
      case MODE_ATTACK:
        printinfo(ci);
        canmove = tmp[ci] == CAN_ATTACK;
        if (canmove) {
          printf("Press SPACE to attack");
        } else {
          printf("Use arrow keys to select attack target");
        }
        break;
    }
    ch = cgetc();
    switch (ch) {
      case 0x08: movecursor(-1); break;
      case 0x15: movecursor(1); break;
      case 0x0b: movecursor(-WIDTH); break;
      case 0x0a: movecursor(WIDTH); break;
      case 'T':
        ui_techtree();
        break;
      case 'M':
        if (cmds & MASK(CMD_MOVE)) {
          srci = ci;
          getunitmoves(ci);
          uimode = (uimode == MODE_MOVE) ? MODE_SELECT : MODE_MOVE;
        }
        break;
      case 'A':
        if (cmds & MASK(CMD_ATTACK)) {
          srci = ci;
          getunitattacks(ci);
          uimode = (uimode == MODE_MOVE) ? MODE_SELECT : MODE_ATTACK;
        }
        break;
      case 'C':
        if (cmds & MASK(CMD_CONQUER)) {
          claimcity(ci);
        }
        break;
      case 'N':
        if (cmds & MASK(CMD_NEW_UNIT)) {
          uinewunit(ci);
        }
        break;
      case 'G':
        if (cmds & MASK(CMD_COLLECT)) {
          gathertile(ci);
        }
        break;
      case 27:
        uimode = MODE_SELECT;
        break;
      case 32:
        if (uimode == MODE_MOVE && canmove) {
          moveunit(srci, ci);
          uimode = MODE_SELECT;
        } else if (uimode == MODE_ATTACK && canmove) {
          attackunit(srci, ci);
          uimode = MODE_SELECT;
          gotoxy(1,22);
          cclear(39);
          gotoxy(1,21);
          printf("attacked\n");
          printunitinfo(ci);
          cgetc();
        }
        break;
      case 13:
        endturn = 1;
        break;
      case '=':
        printxtra++;
        break;
      default:
        printf("*** CHAR %x *** \n", ch);
        cgetc();
        break;
    }
  } while (!endturn);
}

void startturn(byte player) {
  byte i = 0;
  cur_player = player;
  do {
    // reset unit flags
    if (uowner[i] == player) {
      uflags[i] &= ~ALLACTIONS;
    }
    // add stars from cities, only if they aren't being occupied
    if (territory[i] == player) {
      if (terrain[i] == CITY) {
        if (uowner[i] == 0 || uowner[i] == cur_player) {
          player_stars[player] += CITYLEVEL(i);
        }
      }
    }
  } while (++i);
}

void createworld(byte plyrmask) {
  byte plyr;
  initworld();
  for (plyr=1; plyr<NPLAYERS; plyr++) {
    if (plyrmask & (1<<plyr)) {
      startturn(plyr);
      addcapitol(plyr);
    } else {
      newvillage(rand());
    }
  }
  cur_player = 0;
  printworld();
  while (blurterrain() < 160) {
    printworld();
  }
  printf("\n\nPress <ENTER> to start");
  cgetc();
}

#ifdef __MAIN__

int main (void)
{
  srand(1);
  createworld(0x2 | 0x4);
  while (1) {
    startturn(1);
    playturn();
    startturn(2);
    playturn();
  }
  /*
  // TODO: what if (0,0)?
  if (detectall(det_myunit)) {
    getunitmoves(detect_index);
    getunitattacks(detect_index);
    printworld();
    printf("\nunit moves for $%02x\n", detect_index);
  }
  */
  cgetc();
  return EXIT_SUCCESS;
}

#endif
