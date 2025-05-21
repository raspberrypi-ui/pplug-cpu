/*============================================================================
Copyright (c) 2018-2025 Raspberry Pi
All rights reserved.

Some code taken from the lxpanel project

Copyright (c) 2006-2010 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
            2006-2008 Jim Huang <jserv.tw@gmail.com>
            2008 Fred Chien <fred@lxde.org>
            2009 Ying-Chun Liu (PaulLiu) <grandpaul@gmail.com>
            2009-2010 Marty Jack <martyj19@comcast.net>
            2010 Jürgen Hötzel <juergen@archlinux.org>
            2010-2011 Julien Lavergne <julien.lavergne@gmail.com>
            2012-2013 Henry Gebhardt <hsggebhardt@gmail.com>
            2012 Michael Rawson <michaelrawson76@gmail.com>
            2014 Max Krummenacher <max.oss.09@gmail.com>
            2014 SHiNE CsyFeK <csyfek@users.sourceforge.net>
            2014 Andriy Grytsenko <andrej@rep.kiev.ua>

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
============================================================================*/

#include <locale.h>
#include <glib/gi18n.h>

#ifdef LXPLUG
#include "plugin.h"
#else
#include "lxutils.h"
#endif

#include "cpu.h"

/*----------------------------------------------------------------------------*/
/* Typedefs and macros                                                        */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* Global data                                                                */
/*----------------------------------------------------------------------------*/

conf_table_t conf_table[4] = {
    {CONF_TYPE_BOOL,     "show_percentage",  N_("Show usage as percentage"),    NULL},
    {CONF_TYPE_COLOUR,   "foreground",       N_("Foreground colour"),           NULL},
    {CONF_TYPE_COLOUR,   "background",       N_("Background colour"),           NULL},
    {CONF_TYPE_NONE,     NULL,               NULL,                              NULL}
};

/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static gboolean cpu_update (CPUPlugin *c);

/*----------------------------------------------------------------------------*/
/* Function definitions                                                       */
/*----------------------------------------------------------------------------*/

/* Periodic timer callback */

static gboolean cpu_update (CPUPlugin *c)
{
    cpu_stat cpu, cpu_delta;
    char buffer[256];
    FILE *stat;
    float cpu_uns;

    if (g_source_is_destroyed (g_main_current_source ())) return FALSE;

    /* Open statistics file and scan out CPU usage */
    stat = fopen ("/proc/stat", "r");
    if (stat == NULL) return TRUE;
    if (!fgets (buffer, 256, stat))
    {
        fclose (stat);
        return TRUE;
    }
    fclose (stat);
    if (!strlen (buffer)) return TRUE;
    if (sscanf (buffer, "cpu %llu %llu %llu %llu", &cpu.u, &cpu.n, &cpu.s, &cpu.i) == 4)
    {
        /* Compute delta from previous statistics */
        cpu_delta.u = cpu.u - c->previous_cpu_stat.u;
        cpu_delta.n = cpu.n - c->previous_cpu_stat.n;
        cpu_delta.s = cpu.s - c->previous_cpu_stat.s;
        cpu_delta.i = cpu.i - c->previous_cpu_stat.i;

        /* Copy current to previous */
        memcpy (&c->previous_cpu_stat, &cpu, sizeof (cpu_stat));

        /* Compute user + nice + system as a fraction of total */
        cpu_uns = cpu_delta.u + cpu_delta.n + cpu_delta.s;
        cpu_uns /= (cpu_uns + cpu_delta.i);
        if (c->show_percentage) sprintf (buffer, "C:%3.0f", cpu_uns * 100.0);
        else buffer[0] = 0;

        graph_new_point (&(c->graph), cpu_uns, 0, buffer);
    }

    return TRUE;
}

/*----------------------------------------------------------------------------*/
/* wf-panel plugin functions                                                  */
/*----------------------------------------------------------------------------*/

/* Handler for system config changed message from panel */
void cpu_update_display (CPUPlugin *c)
{
    GdkRGBA none = {0, 0, 0, 0};
    graph_reload (&(c->graph), wrap_icon_size (c), c->background_colour, c->foreground_colour, none, none);
}

void cpu_init (CPUPlugin *c)
{
    setlocale (LC_ALL, "");
    bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

    /* Allocate icon as a child of top level */
    graph_init (&(c->graph));
    gtk_container_add (GTK_CONTAINER (c->plugin), c->graph.da);

    cpu_update_display (c);

    /* Connect a timer to refresh the statistics. */
    c->timer = g_timeout_add (1500, (GSourceFunc) cpu_update, (gpointer) c);

    /* Show the widget and return. */
    gtk_widget_show_all (c->plugin);
}

void cpu_destructor (gpointer user_data)
{
    CPUPlugin *c = (CPUPlugin *) user_data;
    graph_free (&(c->graph));
    if (c->timer) g_source_remove (c->timer);
    g_free (c);
}

/*----------------------------------------------------------------------------*/
/* LXPanel plugin functions                                                   */
/*----------------------------------------------------------------------------*/
#ifdef LXPLUG

/* Constructor */
static GtkWidget *cpu_constructor (LXPanel *panel, config_setting_t *settings)
{
    /* Allocate and initialize plugin context */
    CPUPlugin *c = g_new0 (CPUPlugin, 1);

    /* Allocate top level widget and set into plugin widget pointer. */
    c->panel = panel;
    c->settings = settings;
    c->plugin = gtk_event_box_new ();
    lxpanel_plugin_set_data (c->plugin, c, cpu_destructor);

    /* Set config defaults */
    gdk_rgba_parse (&c->foreground_colour, "dark gray");
    gdk_rgba_parse (&c->background_colour, "light gray");
    c->show_percentage = TRUE;

    /* Read config */
    conf_table[0].value = (void *) &c->show_percentage;
    conf_table[1].value = (void *) &c->foreground_colour;
    conf_table[2].value = (void *) &c->background_colour;
    lxplug_read_settings (c->settings, conf_table);

    cpu_init (c);

    return c->plugin;
}

/* Handler for system config changed message from panel */
static void cpu_configuration_changed (LXPanel *, GtkWidget *plugin)
{
    CPUPlugin *c = lxpanel_plugin_get_data (plugin);
    cpu_update_display (c);
}

/* Apply changes from config dialog */
static gboolean cpu_apply_configuration (gpointer user_data)
{
    CPUPlugin *c = lxpanel_plugin_get_data (GTK_WIDGET (user_data));

    lxplug_write_settings (c->settings, conf_table);

    cpu_update_display (c);
    return FALSE;
}

/* Display configuration dialog */
static GtkWidget *cpu_configure (LXPanel *panel, GtkWidget *plugin)
{
    return lxpanel_generic_config_dlg_new (_(PLUGIN_TITLE), panel,
        cpu_apply_configuration, plugin,
        conf_table);
}

FM_DEFINE_MODULE (lxpanel_gtk, cpu)

/* Plugin descriptor */
LXPanelPluginInit fm_module_init_lxpanel_gtk = {
    .name = N_(PLUGIN_TITLE),
    .config = cpu_configure,
    .description = N_("Display CPU usage"),
    .new_instance = cpu_constructor,
    .reconfigure = cpu_configuration_changed,
    .gettext_package = GETTEXT_PACKAGE
};
#endif

/* End of file */
/*----------------------------------------------------------------------------*/
