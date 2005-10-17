/*
 * Mono Plugin Loader
 *
 * -- Thanks to the perl plugin loader for all the great tips ;-)
 *
 * Eoin Coffey
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "internal.h"
#include "debug.h"
#include "plugin.h"
#include "version.h"
#include "mono-helper.h"

#define MONO_PLUGIN_ID "core-mono"

/* This is where our code executes */
static MonoDomain *domain;

/* probes the given plugin to determine if its a plugin */
static gboolean probe_mono_plugin(GaimPlugin *plugin)
{
	MonoAssembly *assm;
	MonoMethod *m = NULL;
	MonoMethod *info_method = NULL;
	MonoObject *plugin_info;
	gboolean found_load = FALSE, found_unload = FALSE, found_destroy = FALSE, found_info = FALSE;
	gpointer iter = NULL;
	
	GaimPluginInfo *info;
	GaimMonoPlugin *mplug;
	
	char *file = plugin->path;
	
	assm = mono_domain_assembly_open(domain, file);
	
	if (!assm) {
		return FALSE;
	} 
		
	gaim_debug(GAIM_DEBUG_INFO, "mono", "Probing plugin\n");
	
	if (mono_loader_is_api_dll(mono_assembly_get_image(assm))) {
		gaim_debug(GAIM_DEBUG_INFO, "mono", "Found our GaimAPI.dll\n");
		return FALSE;
	}
		
	info = g_new0(GaimPluginInfo, 1);
	mplug = g_new0(GaimMonoPlugin, 1);
		
	mplug->assm = assm;
		
	mplug->klass = mono_loader_find_plugin_class(mono_assembly_get_image(mplug->assm));
	if (!mplug->klass) {
		gaim_debug(GAIM_DEBUG_ERROR, "mono", "no plugin class in \'%s\'\n", file);
		return FALSE;
	}
	
	mplug->obj = mono_object_new(domain, mplug->klass);
	if (!mplug->obj) {
		gaim_debug(GAIM_DEBUG_ERROR, "mono", "obj not valid\n");
		return FALSE;
	}
	
	mono_runtime_object_init(mplug->obj);
	
	while ((m = mono_class_get_methods(mplug->klass, &iter))) {
		if (strcmp(mono_method_get_name(m), "Load") == 0) {
			mplug->load = m;
			found_load = TRUE;
		} else if (strcmp(mono_method_get_name(m), "Unload") == 0) {
			mplug->unload = m;
			found_unload = TRUE;
		} else if (strcmp(mono_method_get_name(m), "Destroy") == 0) {
			mplug->destroy = m;
			found_destroy = TRUE;
		} else if (strcmp(mono_method_get_name(m), "Info") == 0) {
			info_method = m;
			found_info = TRUE;
		}
	}
	
	if (!(found_load && found_unload && found_destroy && found_info)) {
		gaim_debug(GAIM_DEBUG_ERROR, "mono", "did not find the required methods\n");
		return FALSE;
	}
	
	plugin_info = mono_runtime_invoke(info_method, mplug->obj, NULL, NULL);
	
	/* now that the methods are filled out we can populate
	   the info struct with all the needed info */
	
	info->name = mono_loader_get_prop_string(plugin_info, "Name");
	info->version = mono_loader_get_prop_string(plugin_info, "Version");
	info->summary = mono_loader_get_prop_string(plugin_info, "Summary");
	info->description = mono_loader_get_prop_string(plugin_info, "Description");
	info->author = mono_loader_get_prop_string(plugin_info, "Author");
	info->homepage = mono_loader_get_prop_string(plugin_info, "Homepage");
		
	info->magic = GAIM_PLUGIN_MAGIC;
	info->major_version = GAIM_MAJOR_VERSION;
	info->minor_version = GAIM_MINOR_VERSION;
	info->type = GAIM_PLUGIN_STANDARD;
		
	/* this plugin depends on us; duh */
	info->dependencies = g_list_append(info->dependencies, MONO_PLUGIN_ID);
	mplug->plugin = plugin;
				
	plugin->info = info;
	info->extra_info = mplug;
	
	mono_loader_add_plugin(mplug);
	
	return gaim_plugin_register(plugin);
}

/* Loads a Mono Plugin by calling 'load' in the class */
static gboolean load_mono_plugin(GaimPlugin *plugin)
{
	GaimMonoPlugin *mplug;
	
	gaim_debug(GAIM_DEBUG_INFO, "mono", "Loading plugin\n");
	
	mplug = (GaimMonoPlugin*)plugin->info->extra_info;
	
	mono_runtime_invoke(mplug->load, mplug->obj, NULL, NULL);
	
	return TRUE;
}

/* Unloads a Mono Plugin by calling 'unload' in the class */
static gboolean unload_mono_plugin(GaimPlugin *plugin)
{
	GaimMonoPlugin *mplug;
	
	gaim_debug(GAIM_DEBUG_INFO, "mono", "Unloading plugin\n");
	
	mplug = (GaimMonoPlugin*)plugin->info->extra_info;
	
	gaim_signals_disconnect_by_handle((gpointer)mplug->klass);
	
	mono_runtime_invoke(mplug->unload, mplug->obj, NULL, NULL);
	
	return TRUE;
}

/* Destroys a Mono Plugin by calling 'destroy' in the class,
   and cleaning up all the malloced memory */
static void destroy_mono_plugin(GaimPlugin *plugin)
{
	GaimMonoPlugin *mplug;
	
	gaim_debug(GAIM_DEBUG_INFO, "mono", "Destroying plugin\n");
	
	mplug = (GaimMonoPlugin*)plugin->info->extra_info;
	
	mono_runtime_invoke(mplug->destroy, mplug->obj, NULL, NULL);
	
	if (plugin->info) {
		if (plugin->info->name) g_free(plugin->info->name);
		if (plugin->info->version) g_free(plugin->info->version);
		if (plugin->info->summary) g_free(plugin->info->summary);
		if (plugin->info->description) g_free(plugin->info->description);
		if (plugin->info->author) g_free(plugin->info->author);
		if (plugin->info->homepage) g_free(plugin->info->homepage);
	}
	
	if (mplug) {
		if (mplug->assm) {
			mono_assembly_close(mplug->assm);
		}
		
		g_free(mplug);
		mplug = NULL;
	}
}

static void plugin_destroy(GaimPlugin *plugin)
{
	mono_jit_cleanup(domain);	
}

static GaimPluginLoaderInfo loader_info =
{
	NULL,
	probe_mono_plugin,
	load_mono_plugin,
	unload_mono_plugin,
	destroy_mono_plugin
};

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_LOADER,
	NULL,
	0,
	NULL,
	GAIM_PRIORITY_DEFAULT,
	MONO_PLUGIN_ID,
	N_("Mono Plugin Loader"),
	VERSION,
	N_("Loads .NET plugins with Mono."),
	N_("Loads .NET plugins with Mono."),
	"Eoin Coffey <ecoffey@simla.colostate.edu>",
	GAIM_WEBSITE,
	NULL,
	NULL,
	plugin_destroy,
	NULL,
	&loader_info,
	NULL,
	NULL
};

/* Creates the domain to execute in, and setups our CS Gaim API (note:
   in the future the 'mono_add_internal_call' will be spread through out
   the source to whatever module is exposing the API; this function will
   simply call helper functions to do so) */
static void init_plugin(GaimPlugin *plugin)
{
	domain = mono_jit_init("gaim");
	
	mono_loader_set_domain(domain);
	
	mono_loader_init_internal_calls();
	
	loader_info.exts = g_list_append(loader_info.exts, "dll");
}

GAIM_INIT_PLUGIN(mono, init_plugin, info)
