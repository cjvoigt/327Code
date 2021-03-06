#include <stdlib.h>
#include <ncurses.h>

#include "string.h"

#include "dungeon.h"
#include "pc.h"
#include "utils.h"
#include "move.h"
#include "path.h"
#include "io.h"

const char *eq_slot_name[num_eq_slots] = {
  "weapon",
  "offhand",
  "ranged",
  "light",
  "armor",
  "helmet",
  "cloak",
  "gloves",
  "boots",
  "amulet",
  "lh ring",
  "rh ring"
};

pc::pc()
{
  uint32_t i;

  for (i = 0; i < num_eq_slots; i++) {
    eq[i] = 0;
  }

  for (i = 0; i < MAX_INVENTORY; i++) {
    in[i] = 0;
  }

  hp = 1000;
}

pc::~pc()
{
  uint32_t i;

  for (i = 0; i < MAX_INVENTORY; i++) {
    if (in[i]) {
      delete in[i];
      in[i] = NULL;
    }
  }
    
  for (i = 0; i < num_eq_slots; i++) {
    if (eq[i]) {
      delete eq[i];
      eq[i] = NULL;
    }
  }
}

void delete_pc(character *the_pc)
{
  delete static_cast<pc *>(the_pc);
}
uint32_t pc_is_alive(dungeon_t *d)
{
  return ((pc *) d->the_pc)->alive;
}

void place_pc(dungeon_t *d)
{
  ((pc *) d->the_pc)->position[dim_y] = rand_range(d->rooms->position[dim_y],
                                               (d->rooms->position[dim_y] +
                                                d->rooms->size[dim_y] - 1));
  ((pc *) d->the_pc)->position[dim_x] = rand_range(d->rooms->position[dim_x],
                                               (d->rooms->position[dim_x] +
                                                d->rooms->size[dim_x] - 1));

  pc_init_known_terrain(d->the_pc);
  pc_observe_terrain(d->the_pc, d);
}

void config_pc(dungeon_t *d)
{
  /* This should be in the PC constructor, now. */
  pc *the_pc;
  static dice pc_dice(0, 1, 4);

  the_pc = new pc;
  d->the_pc = the_pc;

  the_pc->symbol = '@';

  place_pc(d);

  the_pc->speed = PC_SPEED;
  the_pc->next_turn = 0;
  the_pc->alive = 1;
  the_pc->sequence_number = 0;
  the_pc->color.push_back(COLOR_WHITE);
  the_pc->damage = &pc_dice;
  the_pc->name = "Isabella Garcia-Shapiro";
  the_pc->defence = 1;
  the_pc->dodge = 1;

  d->charmap[the_pc->position[dim_y]]
            [the_pc->position[dim_x]] = (character *) d->the_pc;

  dijkstra(d);
  dijkstra_tunnel(d);
}

uint32_t pc_next_pos(dungeon_t *d, pair_t dir)
{
  dir[dim_y] = dir[dim_x] = 0;

  /* Tunnel to the nearest dungeon corner, then move around in hopes *
   * of killing a couple of monsters before we die ourself.          */

  if (in_corner(d, d->the_pc)) {
    /*
    dir[dim_x] = (mapxy(d->the_pc.position[dim_x] - 1,
                        d->the_pc.position[dim_y]) ==
                  ter_wall_immutable) ? 1 : -1;
    */
    dir[dim_y] = (mapxy(((pc *) d->the_pc)->position[dim_x],
                        ((pc *) d->the_pc)->position[dim_y] - 1) ==
                  ter_wall_immutable) ? 1 : -1;
  } else {
    dir_nearest_wall(d, d->the_pc, dir);
  }

  return 0;
}

void pc_learn_terrain(character *the_pc, pair_t pos, terrain_type_t ter)
{
  ((pc *) the_pc)->known_terrain[pos[dim_y]][pos[dim_x]] = ter;
  ((pc *) the_pc)->visible[pos[dim_y]][pos[dim_x]] = 1;
}

void pc_see_object(character *the_pc, object *o)
{
  if (o) {
    o->has_been_seen();
  }
}

void pc_reset_visibility(character *the_pc)
{
  uint32_t y, x;

  for (y = 0; y < DUNGEON_Y; y++) {
    for (x = 0; x < DUNGEON_X; x++) {
      ((pc *) the_pc)->visible[y][x] = 0;
    }
  }
}

terrain_type_t pc_learned_terrain(character *the_pc, int8_t y, int8_t x)
{
  return ((pc *) the_pc)->known_terrain[y][x];
}

void pc_init_known_terrain(character *the_pc)
{
  uint32_t y, x;

  for (y = 0; y < DUNGEON_Y; y++) {
    for (x = 0; x < DUNGEON_X; x++) {
      ((pc *) the_pc)->known_terrain[y][x] = ter_unknown;
      ((pc *) the_pc)->visible[y][x] = 0;
    }
  }
}

void pc_observe_terrain(character *the_pc, dungeon_t *d)
{
  pair_t where;
  pc *p;
  int8_t y_min, y_max, x_min, x_max;

  p = (pc *) the_pc;

  y_min = p->position[dim_y] - PC_VISUAL_RANGE;
  if (y_min < 0) {
    y_min = 0;
  }
  y_max = p->position[dim_y] + PC_VISUAL_RANGE;
  if (y_max > DUNGEON_Y - 1) {
    y_max = DUNGEON_Y - 1;
  }
  x_min = p->position[dim_x] - PC_VISUAL_RANGE;
  if (x_min < 0) {
    x_min = 0;
  }
  x_max = p->position[dim_x] + PC_VISUAL_RANGE;
  if (x_max > DUNGEON_X - 1) {
    x_max = DUNGEON_X - 1;
  }

  for (where[dim_y] = y_min; where[dim_y] <= y_max; where[dim_y]++) {
    where[dim_x] = x_min;
    can_see(d, p->position, where, 1);
    where[dim_x] = x_max;
    can_see(d, p->position, where, 1);
  }
  /* Take one off the x range because we alreay hit the corners above. */
  for (where[dim_x] = x_min - 1; where[dim_x] <= x_max - 1; where[dim_x]++) {
    where[dim_y] = y_min;
    can_see(d, p->position, where, 1);
    where[dim_y] = y_max;
    can_see(d, p->position, where, 1);
  }       
}

int32_t is_illuminated(character *the_pc, int8_t y, int8_t x)
{
  return ((pc *) the_pc)->visible[y][x];
}

void pc::recalculate_speed()
{
  int i;

  speed = 10;
  for (i = 0; i < num_eq_slots; i++) {
    if (eq[i]) {
      speed += eq[i]->get_speed();
    }
  }

  if (speed <= 0) {
    speed = 1;
  }
}

void pc::recalculate_defence() {
    int i;

    defence = 1;
    for(i = 0; i < num_eq_slots; i++) {
        if(eq[i]) {
            defence = eq[i]->get_dodge();
        }
    }

    if(defence <= 0) {
        defence = 1;
    }
}

void pc::recalculate_dodge() {
    int i;

    dodge = 1;
    for(i = 0; i < num_eq_slots; i++) {
        if(eq[i]) {
            dodge = eq[i]->get_dodge();
        }
    }

    if(dodge <= 0) {
        dodge = 1;
    }
}

uint32_t pc::wear_in(uint32_t slot)
{
  object *tmp;
  uint32_t i;

  if (!in[slot] || !in[slot]->is_equipable()) {
    return 1;
  }

  /* Rings are tricky since there are two slots.  We will alwas favor *
   * an empty slot, and if there is no empty slot, we'll use the      *
   * first slot.                                                      */
  i = in[slot]->get_eq_slot_index();
  if (eq[i] &&
      ((eq[i]->get_type() == objtype_RING) &&
       !eq[i + 1])) {
    i++;
  }

  tmp = in[slot];
  in[slot] = eq[i];
  eq[i] = tmp;

  io_queue_message("You wear %s.", eq[i]->get_name());

  recalculate_speed();
  recalculate_defence();
  recalculate_dodge();

  return 0;
}

uint32_t pc::has_open_inventory_slot()
{
  int i;

  for (i = 0; i < MAX_INVENTORY; i++) {
    if (!in[i]) {
      return 1;
    }
  }

  return 0;
}

int32_t pc::get_first_open_inventory_slot()
{
  int i;

  for (i = 0; i < MAX_INVENTORY; i++) {
    if (!in[i]) {
      return i;
    }
  }

  return -1;
}

uint32_t pc::remove_eq(uint32_t slot)
{
  if (!eq[slot]                      ||
      !in[slot]->is_removable() ||
      !has_open_inventory_slot()) {
    io_queue_message("You can't remove %s, because you have nowhere to put it.",
                     eq[slot]->get_name());

    return 1;
  }

  io_queue_message("You remove %s.", eq[slot]->get_name());

  in[get_first_open_inventory_slot()] = eq[slot];
  eq[slot] = NULL;


  recalculate_speed();

  return 0;
}

uint32_t pc::drop_in(dungeon_t *d, uint32_t slot)
{
  if (!in[slot] || !in[slot]->is_dropable()) {
    return 1;
  }

  io_queue_message("You drop %s.", in[slot]->get_name());

  in[slot]->to_pile(d, position);
  in[slot] = NULL;

  return 0;
}

uint32_t pc::destroy_in(uint32_t slot)
{
  if (!in[slot] || !in[slot]->is_destructable()) {
    return 1;
  }

  io_queue_message("You destroy %s.", in[slot]->get_name());

  delete in[slot];
  in[slot] = NULL;

  return 0;
}

uint32_t pc::pick_up(dungeon_t *d)
{
  object *o;

  while (has_open_inventory_slot() &&
         d->objmap[position[dim_y]][position[dim_x]]) {
    io_queue_message("You pick up %s.",
                     d->objmap[position[dim_y]][position[dim_x]]->get_name());
    in[get_first_open_inventory_slot()] =
      from_pile(d, position);
  }

  for (o = d->objmap[position[dim_y]][position[dim_x]];
       o;
       o = o->get_next()) {
    io_queue_message("You have no room for %s.", o->get_name());
  }

  return 0;
}

object *pc::from_pile(dungeon_t *d, pair_t pos)
{
  object *o;

  if ((o = (object *) d->objmap[pos[dim_y]][pos[dim_x]])) {
    d->objmap[pos[dim_y]][pos[dim_x]] = o->get_next();
    o->set_next(0);
  }

  return o;
}

void pc::do_ranged_attack(dungeon_t* d, character* c) {
    uint32_t sum = 0;

    for(int i = eq_slot_ranged; i < eq_slot_rring; i++) {
        if(eq[i]) {
            sum += eq[i]->roll_dice();
        }
    }

    c->take_damage(d, this, sum);
}

void pc::do_poison_spell(dungeon_t* d, character* c) {
    for(int i = c->position[dim_y] - 1; i <= c->position[dim_y] + 1; i++) {
        for(int j = c->position[dim_x] - 1; j <= c->position[dim_x] + 1; j++) {
            character* chr = d->charmap[i][j];

            if(!chr) { continue; };
            chr->take_damage(d, this, 100);
        }
    }
}
