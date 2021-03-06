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

#include "game.hpp"
#include <vector>

#include <ijengine/system_event.h>
#include <ijengine/keyboard_event.h>

namespace SoMTD {
Game::Game(const string& title, int w, int h) :
    m_game(title, w, h), m_engine(), m_level_factory() {

  ijengine::event::register_translator(&m_translator);
  ijengine::level::register_factory(&m_level_factory);
  ijengine::resources::set_textures_dir("res");
  ijengine::resources::set_fonts_dir("res");
}

Game::~Game() {
  ijengine::level::unregister_factory();
  ijengine::event::unregister_translator(&m_translator);
}

int
Game::run(const string& level_id) {
  return m_game.run(level_id);
}

namespace tools {
  Point
  grid_to_isometric(int _x, int _y) {
    const double tile_width = 100.;
    const double tile_height = 81.;
    const double x0 = 1024./2.;
    const double offset = 11.0;
    double y0 = tile_height/2.0;
    double xs = (_x - _y) * (tile_width/2) + x0;
    double ys = (_x + _y) * (-offset +tile_height/2.0) + y0;

    return Point { xs, ys };
  }
}

[[deprecated]]
std::pair<int, int>
tools::grid_to_isometric(int x_grid, int y_grid, int tile_width, int tile_height, int x0, int offset) {
  double y0 = tile_height/2.0;
  double xs = (x_grid - y_grid) * (tile_width/2) + x0;
  double ys = (x_grid + y_grid) * (-offset +tile_height/2.0) + y0;

  return std::pair<int, int>((int)xs, (int)ys);
}

[[deprecated]]
std::pair<int, int>
tools::isometric_to_grid(int xs, int ys, int w, int h, int x0, int offset)
{
  x0 += w/2;
  double y0 = h/2;
  double xg = ((double)(xs - x0)/w) + ((double)ys-y0)/(h - 2.0*offset);
  double yg = (-(double)(xs-x0)/w) + (ys-y0)/(h - 2.0*offset);
  return std::pair<int, int>(xg, yg);
}
}
