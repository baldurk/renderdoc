/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015-2016 Baldur Karlsson
 * Copyright (c) 2014 Crytek
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

#include <string>

#include <string.h>
#include <locale.h>
#include <iconv.h>

#include <xcb/xcb.h>

#include <unistd.h>

#include <replay/renderdoc_replay.h>

using std::string;

string GetUsername()
{
	char buf[256] = {0};
	getlogin_r(buf, 255);

	return string(buf, buf+strlen(buf));
}

void DisplayRendererPreview(ReplayRenderer *renderer, TextureDisplay displayCfg)
{
	int scr;

	xcb_connection_t *connection = xcb_connect(NULL, &scr);
	const xcb_setup_t *setup = xcb_get_setup(connection);
	xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
	while (scr-- > 0) xcb_screen_next(&iter);

	xcb_screen_t *screen = iter.data;

	uint32_t value_mask, value_list[32];

	xcb_window_t window = xcb_generate_id(connection);

	value_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
	value_list[0] = screen->black_pixel;
	value_list[1] = XCB_EVENT_MASK_KEY_RELEASE |
									XCB_EVENT_MASK_EXPOSURE |
									XCB_EVENT_MASK_STRUCTURE_NOTIFY;

	xcb_create_window(connection,
					XCB_COPY_FROM_PARENT,
					window, screen->root,
					0, 0, 1280, 720, 0,
					XCB_WINDOW_CLASS_INPUT_OUTPUT,
					screen->root_visual,
					value_mask, value_list);

	/* Magic code that will send notification when window is destroyed */
	xcb_intern_atom_cookie_t cookie = xcb_intern_atom(connection, 1, 12,
																										"WM_PROTOCOLS");
	xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(connection, cookie, 0);

	xcb_intern_atom_cookie_t cookie2 = xcb_intern_atom(connection, 0, 16, "WM_DELETE_WINDOW");
	xcb_intern_atom_reply_t *atom_wm_delete_window = xcb_intern_atom_reply(connection, cookie2, 0);

	xcb_change_property (connection,
			XCB_PROP_MODE_REPLACE,
			window,
			XCB_ATOM_WM_NAME,
			XCB_ATOM_STRING,
			8,
			sizeof ("renderdoccmd")-1,
			"renderdoccmd" );

	xcb_change_property(connection, XCB_PROP_MODE_REPLACE,
											window, (*reply).atom, 4, 32, 1,
											&(*atom_wm_delete_window).atom);
	free(reply);

	xcb_map_window(connection, window);

	void *connectionScreenWindow[] = { (void *)connection, (void *)(uintptr_t)scr, (void *)(uintptr_t)window };

	ReplayOutput *out = ReplayRenderer_CreateOutput(renderer, connectionScreenWindow, eOutputType_TexDisplay);

	OutputConfig c = { eOutputType_TexDisplay };

	ReplayOutput_SetOutputConfig(out, c);
	ReplayOutput_SetTextureDisplay(out, displayCfg);

	xcb_flush(connection);

	bool done = false;
	while(!done)
	{
		xcb_generic_event_t *event;

		event = xcb_poll_for_event(connection);
		if (event)
		{
			switch (event->response_type & 0x7f)
			{
				case XCB_EXPOSE:
					ReplayRenderer_SetFrameEvent(renderer, 10000000+rand()%1000, true);
					ReplayOutput_Display(out);
					break;
				case XCB_CLIENT_MESSAGE:
					if((*(xcb_client_message_event_t*)event).data.data32[0] ==
							(*atom_wm_delete_window).atom) {
						done = true;
					}
					break;
				case XCB_KEY_RELEASE:
					{
						const xcb_key_release_event_t *key =
							(const xcb_key_release_event_t *) event;

						if (key->detail == 0x9)
							done = true;
					}
					break;
				case XCB_DESTROY_NOTIFY:
					done = true;
					break;
				default:
					break;
			}
			free(event);
		}

		ReplayRenderer_SetFrameEvent(renderer, 10000000+rand()%1000, true);
		ReplayOutput_Display(out);

		usleep(100000);
	}
}

// symbol defined in libGL but not librenderdoc.
// Forces link of libGL after renderdoc (otherwise all symbols would
// be resolved and libGL wouldn't link, meaning dlsym(RTLD_NEXT) would fai
extern "C" void glXWaitGL();

int renderdoccmd(int argc, char **argv);

int main(int argc, char *argv[])
{
	std::setlocale(LC_CTYPE, "");

	volatile bool never_run = false; if(never_run) glXWaitGL();

	// do any linux-specific setup here

	// process any linux-specific arguments here

	return renderdoccmd(argc, argv);
}
