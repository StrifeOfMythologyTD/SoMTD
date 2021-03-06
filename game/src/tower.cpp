// Copyright (C) 2016 Dylan Guedes
//
// This file is part of SoMTD.
//
// SoMTD is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// SoMTD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with SoMTD. If not, see <http://www.gnu.org/licenses/>.

#include <ijengine/canvas.h>
#include <ijengine/engine.h>
#include <ijengine/texture.h>
#include <ijengine/rectangle.h>

#include <cmath>
#include <iostream>

#include "./game.h"
#include "./tower.h"
#include "./animation.hpp"
#include "./player.hpp"

namespace SoMTD {
Tower::Tower(std::string texture_name,
    unsigned id,
    int _x,
    int _y,
    std::string image_selected,
    Animation::StateStyle statestyle,
    int frame_per_state,
    int total_states,
    float newattackspeed,
    int newdamage) :
    m_image_path(texture_name),
    m_id(id),
    m_start(-1),
    m_priority(0),
    m_imageselected_path(image_selected) {
  m_attack_speed = newattackspeed;
  m_damage = newdamage;
  m_level = 1;
  m_range = 85.0;
  m_texture = ijengine::resources::get_texture(texture_name);
  m_animation = new Animation(
      _x, _y, texture_name, statestyle, frame_per_state, total_states);
  m_x = _x;
  m_y = _y;
  ijengine::event::register_listener(this);
  mytimer = 0;
  m_cooldown = 0;
  m_actual_state = IDLE;
  m_projectiles = new std::list<Projectile*>();
}

Tower::~Tower() {
  delete m_projectiles;
  delete m_animation;
  ijengine::event::unregister_listener(this);
}

bool
Tower::on_event(const ijengine::GameEvent& event) {
  if (event.id() == UPGRADE_TOWER) {
    if (m_selected && player::get().gold() > 250) {
      player::get().discount_gold(250);
      level_up();
    }
  }

  if (event.id() == SoMTD::MOUSEOVER) {
    double x_pos = event.get_property<double>("x");
    double y_pos = event.get_property<double>("y");
    std::pair<int, int> click_as_tile = SoMTD::tools::isometric_to_grid(
        x_pos, y_pos, 100, 81, 1024/2, 11);
    if (click_as_tile.first == x() && click_as_tile.second == y()) {
      m_mouseover = true;
    } else {
      m_mouseover = false;
    }
  }

  if (event.id() == SoMTD::CLICK) {
    double x_pos = event.get_property<double>("x");
    double y_pos = event.get_property<double>("y");
    std::pair<int, int> click_as_tile = SoMTD::tools::isometric_to_grid(
        x_pos, y_pos, 100, 81, 1024/2, 11);
    if (click_as_tile.first == x() && click_as_tile.second == y()) {
      m_selected = true;
      m_animation->update_texture(m_imageselected_path);
      player::get().state = SoMTD::player::PlayerState::SELECTED_TOWER;
      player::get().selected_object = this;
    } else {
      m_selected = false;
      m_animation->update_texture(m_image_path);
    }
  }

  return false;
}

void
Tower::draw_self(ijengine::Canvas *canvas, unsigned a1, unsigned a2) {
  m_animation->draw(canvas, a1, a2);

  for (auto it=m_projectiles->begin(); it != m_projectiles->end(); ++it) {
    (*it)->draw_self(canvas, a1, a2);
  }

  if (m_mouseover) {
    Point pos = m_animation->screen_position();
    int half_h = m_animation->height()/2;
    for (double theta=0.0; theta < 360; ++theta) {
      double myx = ((m_range * cos(theta)) + pos.x+m_animation->width()-15);
      double myy = (m_range * sin(theta) + pos.y+ half_h/2);
      ijengine::Point myp(myx, myy);
      canvas->draw(myp);
    }
  }
}

void
Tower::update_self(unsigned now, unsigned last) {
  if (m_next_frame_time == 0) {
    m_next_frame_time = now+(1000/m_animation->frame_per_state());
  }

  if (now > m_next_frame_time) {
    m_animation->next_frame();
    m_next_frame_time += 1000/m_animation->frame_per_state();
  }

  for (auto it=m_projectiles->begin(); it != m_projectiles->end(); ++it) {
    if ((*it)->done())
      it = m_projectiles->erase(it);
    else
      (*it)->update_self(now, last);
  }

  switch (actual_state()) {
    case IDLE:
      handle_idle_state(now, last);
      break;

    case ATTACKING:
      handle_attacking_state(now, last);
      break;

    default:
      break;
  }
}

void
Tower::level_up() {
  m_level+=1;
  m_damage *=1.15;
  m_range+=30.0;
}

int
Tower::level() const {
  return m_level;
}

int
Tower::damage() const {
  return m_damage;
}

double
Tower::range() const {
  return m_range;
}

void
Tower::draw_self_after(ijengine::Canvas* c, unsigned a1, unsigned a2) {
  for (auto it=m_projectiles->begin(); it != m_projectiles->end(); ++it) {
    (*it)->draw_self_after(c, a1, a2);
  }

  if (m_selected) {
    Point pos = m_animation->screen_position();
    auto font = ijengine::resources::get_font("Forelle.ttf", 40);
    c->set_font(font);
    c->draw("Press U", pos.x, pos.y-80);
  }
}

Animation*
Tower::animation() const {
  return m_animation;
}

Tower::State
Tower::actual_state() const {
  return m_actual_state;
}

void
Tower::handle_idle_state(unsigned, unsigned) { }

void
Tower::handle_attacking_state(unsigned now, unsigned last) {
  if (now > m_cooldown) {
    if (m_target) {
      if (m_target->active()) {
        double dx = x() - target()->animation()->screen_position().x;
        double dy = y() - target()->animation()->screen_position().y;
        double distance = sqrt(dx*dx + dy*dy);
        if (distance < range()+target()->animation()->width()/2) {
          attack(m_target, now, last);

          if (m_id == 0x001)
            player::get().increase_gold(damage());
        } else {
          m_actual_state = SoMTD::Tower::IDLE;
          m_target = nullptr;
        }
      } else {
        m_actual_state = SoMTD::Tower::IDLE;
        m_target = nullptr;
      }
    } else {
      m_actual_state = SoMTD::Tower::IDLE;
    }
  }
}

void
SoMTD::Tower::attack(SoMTD::MovableUnit* newtarget, unsigned now, unsigned last)
{
    // If it is a poseidon tower, it can have multiple targets
    switch (id()) {
        // poseidon towers
        case 0x10:

          if (m_cooldown < now) {
            m_cooldown = now+attack_speed()*1000;
            m_target = newtarget;
            m_actual_state = State::ATTACKING;
            Projectile* p = new Projectile(target(), std::make_pair(target()->animation()->screen_position().x, target()->animation()->screen_position().y), "projectiles/projetil_poseidon.png", std::make_pair(animation()->screen_position().x, animation()->screen_position().y), 1, 1, damage());
            m_projectiles->push_back(p);
            newtarget->suffer_slow(400, 3000, now, last);
          }
          break;

        case 0x11:
          if (m_cooldown < now) {
            m_cooldown = now+attack_speed()*1000;
            m_target = newtarget;
            m_actual_state = State::ATTACKING;
            Projectile* p = new Projectile(target(), std::make_pair(target()->animation()->screen_position().x, target()->animation()->screen_position().y), "projectiles/projetil_poseidon.png", std::make_pair(animation()->screen_position().x, animation()->screen_position().y), 1, 1, damage());
            m_projectiles->push_back(p);
            newtarget->suffer_slow(400, 2000, now, last);
          }
          break;

        case 0x12:

        if (m_cooldown < now) {
            m_cooldown = now+attack_speed()*1000;
            m_actual_state = IDLE;

            /* pushing an slow event to the queue of events. the first
             * argument is the x_position of the unit, the second argument
             * is the y_position of the unit, the third argument is the
             * range of the slow, 4th argument is the dmg of the slow,
             * 5th argument is the slow coefficient, and 6th the time penalization */
            player::get().units_events()->push_back(0x000);
            player::get().event_args()->push_back((int)newtarget->x());
            player::get().event_args()->push_back((int)newtarget->y());
            player::get().event_args()->push_back(50);
            player::get().event_args()->push_back(damage());
            player::get().event_args()->push_back(600);
            player::get().event_args()->push_back(2000);
        }
        break;

        case 0x13:
            if (m_cooldown < now) {
                m_cooldown = now+attack_speed()*1000;
                m_actual_state = IDLE;

                /* pushing an slow event to the queue of events. the first
                 * argument is the x_position of the unit, the second argument
                 * is the y_position of the unit, the third argument is the
                 * range of the slow, 4th argument is the dmg of the slow,
                 * 5th argument is the slow coefficient, and 6th the time penalization */
                player::get().units_events()->push_back(0x000);
                player::get().event_args()->push_back((int)newtarget->x());
                player::get().event_args()->push_back((int)newtarget->y());
                player::get().event_args()->push_back(50);
                player::get().event_args()->push_back(damage());
                player::get().event_args()->push_back(500);
                player::get().event_args()->push_back(3000);
            }
            break;

        //zeus towers
        case 0x0:
        case 0x1:
        case 0x2:
        case 0x3:
            if (m_cooldown < now) {
                m_cooldown = now+attack_speed()*1000;
                m_target = newtarget;
                Projectile* p = new Projectile(target(), std::make_pair(target()->animation()->screen_position().x, target()->animation()->screen_position().y), "projectiles/projetil_zeus2.png", std::make_pair(animation()->screen_position().x, animation()->screen_position().y), 1, 1, damage());
                m_projectiles->push_back(p);
                m_actual_state = State::ATTACKING;
                player::get().increase_gold(damage());
            }
            break;

         //hades towers
         case 0x100:
         case 0x101:
           if (m_cooldown < now) {
               m_cooldown = now+attack_speed()*1000;
               m_target = newtarget;
               m_actual_state = State::ATTACKING;
               Projectile* p = new Projectile(target(), std::make_pair(target()->animation()->screen_position().x, target()->animation()->screen_position().y), "projectiles/projetil_caveira.png", std::make_pair(animation()->screen_position().x, animation()->screen_position().y), 1, 1, damage());
               m_projectiles->push_back(p);
           }
           break;

         case 0x102:
           if (m_cooldown < now) {
               m_cooldown = now+attack_speed()*1000;
               m_target = newtarget;
               Projectile* p = new Projectile(target(), std::make_pair(target()->animation()->screen_position().x, target()->animation()->screen_position().y), "projectiles/projetil_caveira.png", std::make_pair(animation()->screen_position().x, animation()->screen_position().y), 1, 1, damage());
               m_projectiles->push_back(p);
               newtarget->suffer_poison(damage(), 8000, now, last);
               m_actual_state = State::ATTACKING;

           }
           break;

         case 0x103:
            if (m_cooldown < now) {
                m_cooldown = now+attack_speed()*1000;
                m_target = newtarget;
                m_actual_state = State::ATTACKING;
                Projectile* p = new Projectile(
                    target(),
                    std::make_pair(target()->animation()->screen_position().x,
                      target()->animation()->screen_position().y),
                    "projectiles/projetil_caveira.png",
                    std::make_pair(animation()->screen_position().x,
                      animation()->screen_position().y), 1, 1, damage());
                m_projectiles->push_back(p);
                newtarget->suffer_bleed(damage(), 10000, now, last);
            }
            break;

        default:
            m_cooldown = now+attack_speed()*1000;
            m_target = newtarget;
            Projectile* p = new Projectile(
                target(),
                std::make_pair(target()->animation()->screen_position().x,
                  target()->animation()->screen_position().y),
                "projectiles/projetil_poseidon.png",
                std::make_pair(animation()->screen_position().x,
                  animation()->screen_position().y), 1, 1, damage());
            m_projectiles->push_back(p);
            m_actual_state = State::ATTACKING;
            break;
    }
}

MovableUnit*
Tower::target() const {
  return m_target;
}

double
Tower::attack_speed() const {
  return m_attack_speed;
}

unsigned
Tower::id() const {
  return m_id;
}

int
Tower::x() const {
  return m_x;
}

int
Tower::y() const {
  return m_y;
}
}  // namespace SoMTD
