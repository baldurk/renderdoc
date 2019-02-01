/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2019 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#pragma once

#include <QIcon>
#include <QPixmap>
#include <QWidget>

#define RESOURCE_LIST()                                                 \
  RESOURCE_DEF(add, "add.png")                                          \
  RESOURCE_DEF(arrow_in, "arrow_in.png")                                \
  RESOURCE_DEF(arrow_join, "arrow_join.png")                            \
  RESOURCE_DEF(arrow_left, "arrow_left.png")                            \
  RESOURCE_DEF(arrow_refresh, "arrow_refresh.png")                      \
  RESOURCE_DEF(arrow_right, "arrow_right.png")                          \
  RESOURCE_DEF(arrow_undo, "arrow_undo.png")                            \
  RESOURCE_DEF(asterisk_orange, "asterisk_orange.png")                  \
  RESOURCE_DEF(bug, "bug.png")                                          \
  RESOURCE_DEF(chart_curve, "chart_curve.png")                          \
  RESOURCE_DEF(cog, "cog.png")                                          \
  RESOURCE_DEF(color_wheel, "color_wheel.png")                          \
  RESOURCE_DEF(connect, "connect.png")                                  \
  RESOURCE_DEF(control_base_blue, "control_base_blue.png")              \
  RESOURCE_DEF(control_cursor_blue, "control_cursor_blue.png")          \
  RESOURCE_DEF(control_end_blue, "control_end_blue.png")                \
  RESOURCE_DEF(control_play_blue, "control_play_blue.png")              \
  RESOURCE_DEF(control_nan_blue, "control_nan_blue.png")                \
  RESOURCE_DEF(control_reverse_blue, "control_reverse_blue.png")        \
  RESOURCE_DEF(control_sample_blue, "control_sample_blue.png")          \
  RESOURCE_DEF(control_start_blue, "control_start_blue.png")            \
  RESOURCE_DEF(cross, "cross.png")                                      \
  RESOURCE_DEF(checkerboard, "checkerboard.png")                        \
  RESOURCE_DEF(del, "del.png")                                          \
  RESOURCE_DEF(disconnect, "disconnect.png")                            \
  RESOURCE_DEF(find, "find.png")                                        \
  RESOURCE_DEF(arrow_out, "arrow_out.png")                              \
  RESOURCE_DEF(flag_green, "flag_green.png")                            \
  RESOURCE_DEF(flip_y, "flip_y.png")                                    \
  RESOURCE_DEF(folder, "folder.png")                                    \
  RESOURCE_DEF(folder_page_white, "folder_page_white.png")              \
  RESOURCE_DEF(hourglass, "hourglass.png")                              \
  RESOURCE_DEF(house, "house.png")                                      \
  RESOURCE_DEF(information, "information.png")                          \
  RESOURCE_DEF(link, "link.png")                                        \
  RESOURCE_DEF(page_go, "page_go.png")                                  \
  RESOURCE_DEF(page_white_code, "page_white_code.png")                  \
  RESOURCE_DEF(page_white_database, "page_white_database.png")          \
  RESOURCE_DEF(page_white_delete, "page_white_delete.png")              \
  RESOURCE_DEF(page_white_edit, "page_white_edit.png")                  \
  RESOURCE_DEF(page_white_link, "page_white_link.png")                  \
  RESOURCE_DEF(page_white_stack, "page_white_stack.png")                \
  RESOURCE_DEF(plugin, "plugin.png")                                    \
  RESOURCE_DEF(plugin_add, "plugin_add.png")                            \
  RESOURCE_DEF(save, "save.png")                                        \
  RESOURCE_DEF(tick, "tick.png")                                        \
  RESOURCE_DEF(time, "time.png")                                        \
  RESOURCE_DEF(timeline_marker, "timeline_marker.png")                  \
  RESOURCE_DEF(upfolder, "upfolder.png")                                \
  RESOURCE_DEF(update, "update.png")                                    \
  RESOURCE_DEF(wand, "wand.png")                                        \
  RESOURCE_DEF(wireframe_mesh, "wireframe_mesh.png")                    \
  RESOURCE_DEF(wrench, "wrench.png")                                    \
  RESOURCE_DEF(zoom, "zoom.png")                                        \
  RESOURCE_DEF(topo_linelist, "topologies/topo_linelist.svg")           \
  RESOURCE_DEF(topo_linelist_adj, "topologies/topo_linelist_adj.svg")   \
  RESOURCE_DEF(topo_linestrip, "topologies/topo_linestrip.svg")         \
  RESOURCE_DEF(topo_linestrip_adj, "topologies/topo_linestrip_adj.svg") \
  RESOURCE_DEF(topo_patch, "topologies/topo_patch.svg")                 \
  RESOURCE_DEF(topo_pointlist, "topologies/topo_pointlist.svg")         \
  RESOURCE_DEF(topo_trilist, "topologies/topo_trilist.svg")             \
  RESOURCE_DEF(topo_trilist_adj, "topologies/topo_trilist_adj.svg")     \
  RESOURCE_DEF(topo_tristrip, "topologies/topo_tristrip.svg")           \
  RESOURCE_DEF(topo_tristrip_adj, "topologies/topo_tristrip_adj.svg")   \
  RESOURCE_DEF(action, "action.png")                                    \
  RESOURCE_DEF(action_hover, "action_hover.png")

struct Resource
{
  QIcon icon;
  QPixmap pixmap;
};

class Resources
{
public:
  static void Initialise();
  ~Resources();

#undef RESOURCE_DEF
#define RESOURCE_DEF(name, filename)                               \
  static const Resource &name() { return resources->name##_data; } \
  static const Resource &name##_2x() { return resources->name##_2x_data; }
  RESOURCE_LIST()

private:
#undef RESOURCE_DEF
#define RESOURCE_DEF(name, filename) Resource name##_data, name##_2x_data;

  struct ResourceSet
  {
    RESOURCE_LIST();
  };

  static ResourceSet *resources;
};

// helper forwarding structs

struct Pixmaps
{
#undef RESOURCE_DEF
#define RESOURCE_DEF(name, filename)               \
  static const QPixmap &name(int devicePixelRatio) \
  {                                                \
    if(devicePixelRatio == 1)                      \
      return Resources::name().pixmap;             \
    else                                           \
      return Resources::name##_2x().pixmap;        \
  }                                                \
  static const QPixmap &name(QWidget *widget) { return name(widget->devicePixelRatio()); }
  RESOURCE_LIST()
};

struct Icons
{
#undef RESOURCE_DEF
#define RESOURCE_DEF(name, filename) \
  static const QIcon &name() { return Resources::name().icon; }
  RESOURCE_LIST()
};