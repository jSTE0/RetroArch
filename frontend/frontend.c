/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2013 - Daniel De Matteis
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include "../general.h"
#include "../conf/config_file.h"
#include "../file.h"

#ifdef HAVE_RGUI
#include "../frontend/menu/rgui.h"
#endif

#ifdef __APPLE__
#include "SDL.h" 
// OSX seems to really need -lSDLmain, 
// so we include SDL.h here so it can hack our main.
// We want to use -mconsole in Win32, so we need main().
#endif

int main(int argc, char *argv[])
{
#ifdef HAVE_RARCH_MAIN_IMPLEMENTATION
   // Consoles use the higher level API.
   return rarch_main(argc, argv);
#else

   rarch_main_clear_state();
   rarch_init_msg_queue();

   int init_ret;
   if ((init_ret = rarch_main_init(argc, argv))) return init_ret;

#ifdef HAVE_RGUI
   menu_init();
   g_extern.lifecycle_mode_state |= 1ULL << MODE_GAME;

   for (;;)
   {
      if (g_extern.system.shutdown)
         break;
      else if (g_extern.lifecycle_mode_state & (1ULL << MODE_LOAD_GAME))
      {
         if (g_extern.lifecycle_mode_state & (1ULL << MODE_INFO_DRAW))
         {
            char tmp[PATH_MAX];
            char str[PATH_MAX];

            fill_pathname_base(tmp, g_extern.fullpath, sizeof(tmp));
            snprintf(str, sizeof(str), "INFO - Loading %s...", tmp);
            msg_queue_push(g_extern.msg_queue, str, 1, 1);
         }

#if defined(HAVE_RGUI) || defined(HAVE_RMENU) || defined(HAVE_RMENU_XUI)
         if (rgui->history)
         {
            rom_history_push(rgui->history,
                  g_extern.fullpath,
                  g_settings.libretro,
                  rgui->info.library_name);
         }

         // draw frame for loading message
         if (driver.video_poke && driver.video_poke->set_texture_enable)
            driver.video_poke->set_texture_enable(driver.video_data, rgui->frame_buf_show, MENU_TEXTURE_FULLSCREEN);

         rarch_render_cached_frame();

         if (driver.video_poke && driver.video_poke->set_texture_enable)
            driver.video_poke->set_texture_enable(driver.video_data, false,
                  MENU_TEXTURE_FULLSCREEN);
#endif

         g_extern.lifecycle_mode_state &= ~(1ULL << MODE_LOAD_GAME);
         g_extern.lifecycle_mode_state |= (1ULL << MODE_INIT);
      }
      else if (g_extern.lifecycle_mode_state & (1ULL << MODE_GAME))
      {
         while ((g_extern.is_paused && !g_extern.is_oneshot) ? rarch_main_idle_iterate() : rarch_main_iterate());
         g_extern.lifecycle_mode_state &= ~(1ULL << MODE_GAME);
      }
      else if (g_extern.lifecycle_mode_state & (1ULL << MODE_INIT))
      {
         if (g_extern.main_is_init)
            rarch_main_deinit();

         struct rarch_main_wrap args = {0};

         args.verbose       = g_extern.verbose;
         args.config_path   = *g_extern.config_path ? g_extern.config_path : NULL;
         args.sram_path     = *g_extern.savefile_dir ? g_extern.savefile_dir : NULL;
         args.state_path    = *g_extern.savestate_dir ? g_extern.savestate_dir : NULL;
         args.rom_path      = g_extern.fullpath;
         args.libretro_path = g_settings.libretro;

         int init_ret = rarch_main_init_wrap(&args);
         if (init_ret == 0)
         {
            RARCH_LOG("rarch_main_init() succeeded.\n");
            g_extern.lifecycle_mode_state |= (1ULL << MODE_GAME);
         }
         else
         {
            RARCH_ERR("rarch_main_init() failed.\n");
            return 1;
         }

         g_extern.lifecycle_mode_state &= ~(1ULL << MODE_INIT);
      }
      else if (g_extern.lifecycle_mode_state & (1ULL << MODE_MENU))
      {
         g_extern.lifecycle_mode_state |= 1ULL << MODE_MENU_PREINIT;
         // Menu should always run with vsync on.
         video_set_nonblock_state_func(false);
         while (menu_iterate());
         driver_set_nonblock_state(driver.nonblock_state);
         g_extern.lifecycle_mode_state &= ~(1ULL << MODE_MENU);
      }
      else
         break;
   }

   menu_free();
   if (g_extern.main_is_init)
      rarch_main_deinit();
#else
   while ((g_extern.is_paused && !g_extern.is_oneshot) ? rarch_main_idle_iterate() : rarch_main_iterate());
   rarch_main_deinit();
#endif
   
   rarch_deinit_msg_queue();

#ifdef PERF_TEST
   rarch_perf_log();
#endif

   rarch_main_clear_state();
   return 0;
#endif
}
