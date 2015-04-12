/*
 * Copyright © 2015 Slawomir Wojtasiak
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authored By: Slawomir Wojtasiak <slawomir@wojtasiak.com>
 */

#include "debugger.h"

#include <gio/gio.h>

#include <jsrdbg/jsrdbg.h>

#include "util/error.h"

static void gjs_debugger_finalize(GObject *object);
static void gjs_debugger_constructed(GObject *object);
static void gjs_debugger_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void gjs_debugger_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);

using namespace JSR;

struct _GjsDebugger {
    GObject parent;
    gchar*   host;
    gint     port;
    JSRemoteDebugger *debugger;
};

struct _GjsDebuggerClass {
    GObjectClass parent;
};

G_DEFINE_TYPE(GjsDebugger, gjs_debugger, G_TYPE_OBJECT);

enum {
    PROP_DBG_0,
    PROP_DBG_HOST,
    PROP_DBG_PORT,
};

GjsDebugger*
gjs_debugger_new(void)
{
    return static_cast<GjsDebugger*>( g_object_new (GJS_TYPE_DEBUGGER, NULL) );
}

static void
gjs_debugger_init(GjsDebugger *js_debugger)
{
    js_debugger->host = JSR_DEFAULT_TCP_BINDING_IP;
    js_debugger->port = JSR_DEFAULT_TCP_PORT;
    js_debugger->debugger = NULL;
}

static void
gjs_debugger_class_init(GjsDebuggerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GParamSpec *pspec;

    object_class->finalize = gjs_debugger_finalize;
    object_class->constructed = gjs_debugger_constructed;
    object_class->get_property = gjs_debugger_get_property;
    object_class->set_property = gjs_debugger_set_property;

    pspec = g_param_spec_string("host", "Server host.", "Host/IP where server has to be bind to.",
                JSR_DEFAULT_TCP_BINDING_IP, (GParamFlags) (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property(object_class, PROP_DBG_HOST, pspec);

    pspec = g_param_spec_int("port", "Server port.", "TCP/IP port number for debugger server.",
                1, 0xFFFF, JSR_DEFAULT_TCP_PORT,(GParamFlags) (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property(object_class, PROP_DBG_PORT, pspec);

}

static void
gjs_debugger_finalize(GObject *object)
{
    GjsDebugger *js_debugger;

    js_debugger = GJS_DEBUGGER(object);

    if( js_debugger->debugger != NULL ) {
        delete js_debugger->debugger;
        js_debugger->debugger = NULL;
    }

    if (js_debugger->host != NULL) {
        g_free(js_debugger->host);
        js_debugger->host = NULL;
    }

    G_OBJECT_CLASS(gjs_debugger_parent_class)->finalize(object);
}

static void
gjs_debugger_constructed(GObject *object)
{
    GjsDebugger *js_debugger = GJS_DEBUGGER(object);
    int i;

    G_OBJECT_CLASS(gjs_debugger_parent_class)->constructed(object);

    /* Creates debugger */
    JSRemoteDebuggerCfg cfg;
    cfg.setProtocol( JSRemoteDebuggerCfg::PROTOCOL_TCP_IP );
    cfg.setTcpHost( js_debugger->host ? js_debugger->host : JSR_DEFAULT_TCP_BINDING_IP );
    cfg.setTcpPort( js_debugger->port );

    js_debugger->debugger = new JSRemoteDebugger( cfg );

}

static void
gjs_debugger_get_property (GObject     *object,
                          guint        prop_id,
                          GValue      *value,
                          GParamSpec  *pspec)
{
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
gjs_debugger_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
    GjsDebugger *js_debugger;

    js_debugger = GJS_DEBUGGER (object);

    switch (prop_id) {
    case PROP_DBG_HOST:
        js_debugger->host = g_value_dup_string(value);
        break;
    case PROP_DBG_PORT:
        js_debugger->port = g_value_get_int(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }

}

gboolean
gjs_debugger_start (GjsDebugger *js_debugger,
                    GError **error)
{
    int rc = js_debugger->debugger->start();
    if( rc ) {
        if( rc == JSR_ERROR_PORT_IN_USE || rc == JSR_ERROR_CANNOT_BIND_SOCKET ) {
            g_set_error(error, GJS_ERROR, rc, "Port is already in use.");
        } else {
            g_set_error(error, GJS_ERROR, rc, "Cannot start debugger.");
        }
        return FALSE;
    }
    return TRUE;
}

gboolean
gjs_debugger_stop (GjsDebugger *js_debugger,
                   GError **error)
{
    int rc = js_debugger->debugger->stop();
    if( rc ) {
        g_set_error(error, GJS_ERROR, rc, "Cannot stop debugger.");
        return FALSE;
    }
    return TRUE;
}

gboolean
gjs_debugger_install (GjsDebugger *js_debugger,
                      GjsContext *ctx,
                      const gchar *name,
                      GjsDebuggerEngineOptions *options,
                      GError **error)
{

    int rc;

    JSContext *context = static_cast<JSContext*>( gjs_context_get_native_context( ctx ) );
    if( !context ) {
        g_set_error(error, GJS_ERROR, GJS_ERROR_FAILED, "Native JS context not found.");
        return FALSE;
    }

    JSObject *global = static_cast<JSObject*>( gjs_context_get_native_global( ctx ) );
    if( !global ) {
        g_set_error(error, GJS_ERROR, GJS_ERROR_FAILED, "Native JS global not found.");
        return FALSE;
    }

    JSAutoCompartment compartment(context, global);
    JSAutoRequest request(context);
    JS::RootedObject jsGlobal( context, global );

    JSR::JSDbgEngineOptions engineOptions;
    if( options->suspend ) {
        engineOptions.suspended();
    }
    if( options->continuation ) {
        engineOptions.continueWhenNoConnections();
    }
    engineOptions.setSourceCodeDisplacement(options->source_displacement);

    if( ( rc = js_debugger->debugger->install( context, name, engineOptions ) ) ) {
        g_set_error(error, GJS_ERROR, rc, "Cannot install debugger for JSContext.");
        return FALSE;
    }

    if( ( rc = js_debugger->debugger->addDebuggee( context, jsGlobal ) ) ) {
        js_debugger->debugger->uninstall( context );
        g_set_error(error, GJS_ERROR, rc, "Cannot add global object to the debugger.");
        return FALSE;
    }

    return TRUE;
}

gboolean
gjs_debugger_uninstall (GjsDebugger *js_debugger,
                        GjsContext *ctx,
                        GError **error)
{

    int rc;

    JSContext *context = static_cast<JSContext*>( gjs_context_get_native_context( ctx ) );
    if( !context ) {
        g_set_error(error, GJS_ERROR, GJS_ERROR_FAILED, "Native JS context not found.");
        return FALSE;
    }

    JSObject *global = static_cast<JSObject*>( gjs_context_get_native_global( ctx ) );
    if( !global ) {
        g_set_error(error, GJS_ERROR, GJS_ERROR_FAILED, "Native JS global not found.");
        return FALSE;
    }

    JSAutoCompartment compartment(context, global);
    JSAutoRequest request(context);
    JS::RootedObject jsGlobal( context, global );

    if( ( rc = js_debugger->debugger->removeDebuggee( context, jsGlobal ) ) ) {
        g_set_error(error, GJS_ERROR, rc, "Cannot remove debugger.");
        return FALSE;
    }

    if( ( rc = js_debugger->debugger->uninstall( context ) ) ) {
        g_set_error(error, GJS_ERROR, rc, "Cannot uninstall debugger.");
        return FALSE;
    }

    return TRUE;
}
