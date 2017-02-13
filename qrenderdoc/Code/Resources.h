/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Baldur Karlsson
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

#define RESOURCE_LIST()                                                 \
  RESOURCE_DEF(logo128, "logo128.png")                                  \
  RESOURCE_DEF(accept, "accept.png")                                    \
  RESOURCE_DEF(add, "add.png")                                          \
  RESOURCE_DEF(arrow_in, "arrow_in.png")                                \
  RESOURCE_DEF(arrow_join, "arrow_join.png")                            \
  RESOURCE_DEF(arrow_undo, "arrow_undo.png")                            \
  RESOURCE_DEF(asterisk_orange, "asterisk_orange.png")                  \
  RESOURCE_DEF(back, "back.png")                                        \
  RESOURCE_DEF(chart_curve, "chart_curve.png")                          \
  RESOURCE_DEF(cog, "cog.png")                                          \
  RESOURCE_DEF(cog_go, "cog_go.png")                                    \
  RESOURCE_DEF(color_wheel, "color_wheel.png")                          \
  RESOURCE_DEF(connect, "connect.png")                                  \
  RESOURCE_DEF(cross, "cross.png")                                      \
  RESOURCE_DEF(crosshatch, "crosshatch.png")                            \
  RESOURCE_DEF(del, "del.png")                                          \
  RESOURCE_DEF(disconnect, "disconnect.png")                            \
  RESOURCE_DEF(down_arrow, "down_arrow.png")                            \
  RESOURCE_DEF(find, "find.png")                                        \
  RESOURCE_DEF(fit_window, "fit_window.png")                            \
  RESOURCE_DEF(flag_green, "flag_green.png")                            \
  RESOURCE_DEF(flip_y, "flip_y.png")                                    \
  RESOURCE_DEF(folder_page, "folder_page.png")                          \
  RESOURCE_DEF(forward, "forward.png")                                  \
  RESOURCE_DEF(hourglass, "hourglass.png")                              \
  RESOURCE_DEF(house, "house.png")                                      \
  RESOURCE_DEF(information, "information.png")                          \
  RESOURCE_DEF(new_window, "new_window.png")                            \
  RESOURCE_DEF(page_white_code, "page_white_code.png")                  \
  RESOURCE_DEF(page_white_database, "page_white_database.png")          \
  RESOURCE_DEF(page_white_delete, "page_white_delete.png")              \
  RESOURCE_DEF(page_white_edit, "page_white_edit.png")                  \
  RESOURCE_DEF(page_white_link, "page_white_link.png")                  \
  RESOURCE_DEF(plugin_add, "plugin_add.png")                            \
  RESOURCE_DEF(red_x_16, "red_x_16.png")                                \
  RESOURCE_DEF(runback, "runback.png")                                  \
  RESOURCE_DEF(runcursor, "runcursor.png")                              \
  RESOURCE_DEF(runfwd, "runfwd.png")                                    \
  RESOURCE_DEF(runnaninf, "runnaninf.png")                              \
  RESOURCE_DEF(runsample, "runsample.png")                              \
  RESOURCE_DEF(save, "save.png")                                        \
  RESOURCE_DEF(stepnext, "stepnext.png")                                \
  RESOURCE_DEF(stepprev, "stepprev.png")                                \
  RESOURCE_DEF(tick, "tick.png")                                        \
  RESOURCE_DEF(time, "time.png")                                        \
  RESOURCE_DEF(timeline_marker, "timeline_marker.png")                  \
  RESOURCE_DEF(up_arrow, "up_arrow.png")                                \
  RESOURCE_DEF(upfolder, "upfolder.png")                                \
  RESOURCE_DEF(wand, "wand.png")                                        \
  RESOURCE_DEF(wireframe_mesh, "wireframe_mesh.png")                    \
  RESOURCE_DEF(wrench, "wrench.png")                                    \
  RESOURCE_DEF(zoom, "zoom.png")                                        \
  RESOURCE_DEF(topo_linelist, "topologies/topo_linelist.png")           \
  RESOURCE_DEF(topo_linelist_adj, "topologies/topo_linelist_adj.png")   \
  RESOURCE_DEF(topo_linestrip, "topologies/topo_linestrip.png")         \
  RESOURCE_DEF(topo_linestrip_adj, "topologies/topo_linestrip_adj.png") \
  RESOURCE_DEF(topo_patch, "topologies/topo_patch.png")                 \
  RESOURCE_DEF(topo_pointlist, "topologies/topo_pointlist.png")         \
  RESOURCE_DEF(topo_trilist, "topologies/topo_trilist.png")             \
  RESOURCE_DEF(topo_trilist_adj, "topologies/topo_trilist_adj.png")     \
  RESOURCE_DEF(topo_tristrip, "topologies/topo_tristrip.png")           \
  RESOURCE_DEF(topo_tristrip_adj, "topologies/topo_tristrip_adj.png")   \
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
#define RESOURCE_DEF(name, filename) \
  static const Resource &name() { return resources->name##_data; }
  RESOURCE_LIST()

private:
#undef RESOURCE_DEF
#define RESOURCE_DEF(name, filename) Resource name##_data;

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
#define RESOURCE_DEF(name, filename) \
  static const QPixmap &name() { return Resources::name().pixmap; }
  RESOURCE_LIST()
};

struct Icons
{
#undef RESOURCE_DEF
#define RESOURCE_DEF(name, filename) \
  static const QIcon &name() { return Resources::name().icon; }
  RESOURCE_LIST()
};