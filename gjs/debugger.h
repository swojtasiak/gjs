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

#ifndef __GJS_JS_DEBUGGER_H__
#define __GJS_JS_DEBUGGER_H__

#if !defined (__GJS_GJS_H__) && !defined (GJS_COMPILATION)
#error "Only <gjs/gjs.h> can be included directly."
#endif

#include <glib-object.h>

#include "context.h"

G_BEGIN_DECLS

typedef struct _GjsDebugger      GjsDebugger;
typedef struct _GjsDebuggerClass GjsDebuggerClass;

#define GJS_TYPE_DEBUGGER             (gjs_debugger_get_type ())
#define GJS_DEBUGGER(object)          (G_TYPE_CHECK_INSTANCE_CAST ((object), GJS_TYPE_DEBUGGER, GjsDebugger))
#define GJS_DEBUGGER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GJS_TYPE_DEBUGGER, GjsDebuggerClass))
#define GJS_IS_DEBUGGER(object)       (G_TYPE_CHECK_INSTANCE_TYPE ((object), GJS_TYPE_DEBUGGER))
#define GJS_IS_DEBUGGER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GJS_TYPE_DEBUGGER))
#define GJS_DEBUGGER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GJS_TYPE_DEBUGGER, GjsDebuggerClass))

typedef struct GjsDebuggerEngineOptions {
    gboolean suspend;
    gboolean continuation;
    gint source_displacement;
} GjsDebuggerEngineOptions;

GType           gjs_debugger_get_type            (void) G_GNUC_CONST;

GjsDebugger*    gjs_debugger_new                 (void);

gboolean        gjs_debugger_start               (GjsDebugger *js_debugger,
                                                  GError **error);

gboolean        gjs_debugger_stop                (GjsDebugger *js_debugger,
                                                  GError **error);

gboolean        gjs_debugger_install             (GjsDebugger *js_debugger,
                                                  GjsContext *ctx,
                                                  const gchar *name,
                                                  GjsDebuggerEngineOptions *options,
                                                  GError **error);

gboolean        gjs_debugger_uninstall           (GjsDebugger *js_debugger,
                                                  GjsContext *ctx,
                                                  GError **error);

G_END_DECLS

#endif /* __GJS_JS_DEBUGGER_H__ */
