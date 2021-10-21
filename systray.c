/*
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Serghei Amelian <serghei@amelian.ro>
 *
 * Copyright (C) 2021 Serghei Amelian
 *
 */

#include <gmodule.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <shell/e-shell.h>
#include <shell/e-shell-view.h>
#include <shell/e-shell-window.h>
#include <mail/em-folder-tree.h>
#include <libebackend/libebackend.h>
#include <libxapp/xapp-status-icon.h>

typedef struct _ESystray ESystray;
typedef struct _ESystrayClass ESystrayClass;

struct _ESystray {
        EExtension parent;
};

struct _ESystrayClass {
        EExtensionClass parent_class;
};


static XAppStatusIcon *status_icon = 0;

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

/* Forward Declarations */
GType e_systray_get_type (void);

G_DEFINE_DYNAMIC_TYPE (ESystray, e_systray, E_TYPE_EXTENSION)

static gboolean
on_widget_deleted(GtkWidget *widget, GdkEvent *event, gpointer data)
{

}

static gboolean
on_window_close_alert(GtkWindow *window, GdkEvent *event, gpointer data)
{
    if(event->type == GDK_DELETE)
    {
        gtk_window_iconify(window);
        gtk_window_set_skip_taskbar_hint(window, TRUE);
        gtk_window_set_skip_pager_hint(window, TRUE);
        return TRUE;
    }

    return FALSE;
}

static gboolean
on_activate(GtkWindow *window, gpointer user_data)
{
    gboolean is_active = gtk_window_is_active(window);

    g_print("is_active: %d\n", is_active);

    if(is_active)
    {
        gtk_window_iconify(window);
        gtk_window_set_skip_taskbar_hint(window, TRUE);
        gtk_window_set_skip_pager_hint(window, TRUE);
    }
    else
    {
        gtk_window_present_with_time(window, gdk_x11_get_server_time(gtk_widget_get_window(GTK_WIDGET(window))));
        gtk_window_set_skip_taskbar_hint(window, FALSE);
        gtk_window_set_skip_pager_hint(window, FALSE);
    }

    //g_print("PTR TO WINDOW: %p\n", window);

    //gtk_window_present_with_time(window, gdk_x11_get_server_time(gtk_widget_get_window(window)));
    //gdk_window_show(gtk_widget_get_window(window));
    //gtk_window_present(window);

    //gdk_window_focus(window, 42);

    return TRUE;
}


static gboolean
on_shell_event(EShell *shell, gpointer event_data, gpointer user_data)
{
    g_print("INFO: shell event %p!!!\n", event_data);

    return FALSE;
}


void
on_shell_view_created(EShellWindow *shell_window,
                           EShellView   *shell_view,
                           gpointer      user_data)
{
    g_print("INFO: on_shell_view_created! %s\n", e_shell_view_get_name(shell_view));
}

void
on_folder_selected(EMFolderTree *folder_tree,
                         CamelStore *store,
                         const gchar *folder_name,
                         CamelFolderInfoFlags flags)

{
    g_print("INFO: folder selected: %s\n", folder_name);
}


gboolean
get_total_unread_messages(GtkTreeModel *model,
                                GtkTreePath  *path,
                                GtkTreeIter  *iter,
                                gpointer user_data)
{
    guint unread;
    gtk_tree_model_get(model, iter, COL_UINT_UNREAD, &unread, -1);

    guint *total_unread_messages = user_data;
    if(unread > 0)
        *total_unread_messages += unread;

    return FALSE;
}


void
on_unread_updated(MailFolderCache *folder_cache,
                       CamelStore *store,
                       const gchar *folder_name,
                       gint unread_messages,
                       GtkTreeModel *model)
{
    int total_unread_messages = 0;
    gtk_tree_model_foreach(model, get_total_unread_messages, &total_unread_messages);

    if(total_unread_messages)
    {
        gchar *num = g_strdup_printf("%i", total_unread_messages);
        xapp_status_icon_set_label(status_icon, num);
        g_free(num);
    }
    else
        xapp_status_icon_set_label(status_icon, "");
}


static void
e_systray_constructed (GObject *object)
{
        EExtensible *extensible;

        /* This retrieves the EShell instance we're extending. */
        extensible = e_extension_get_extensible (E_EXTENSION(object));

        EShellWindow *shell_window = E_SHELL_WINDOW(extensible);

        g_signal_connect_after(G_OBJECT(extensible),
                "event", G_CALLBACK(on_window_close_alert), NULL);

        //gtk_window_iconify(extensible);

        GtkWindow *window = GTK_WINDOW(extensible);

        status_icon = xapp_status_icon_new();
        xapp_status_icon_set_icon_name(status_icon, "evolution");

        g_signal_connect_swapped(status_icon, "activate", G_CALLBACK(on_activate), window);

        EShell *shell = e_shell_window_get_shell(shell_window);
        g_signal_connect(G_OBJECT(shell), "event", G_CALLBACK(on_shell_event), NULL);

        EShellView *shell_view = e_shell_window_get_shell_view(shell_window, "mail");
        EShellSidebar *shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);

        EMFolderTree *folder_tree;
        g_object_get (shell_sidebar, "folder-tree", &folder_tree, NULL);
        g_signal_connect(G_OBJECT(folder_tree), "folder-selected", G_CALLBACK(on_folder_selected), NULL);


        GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(folder_tree));
        EMailSession *email_session = em_folder_tree_model_get_session(EM_FOLDER_TREE_MODEL(model));
        MailFolderCache *mail_folder_cache = e_mail_session_get_folder_cache(email_session);

        g_signal_connect(G_OBJECT(mail_folder_cache), "folder-unread-updated", G_CALLBACK(on_unread_updated), model);
}

static void
e_systray_finalize (GObject *object)
{
        g_print ("Goodbye cruel world!\n");

        /* Chain up to parent's finalize() method. */
        G_OBJECT_CLASS (e_systray_parent_class)->finalize (object);
}

static void
e_systray_class_init (ESystrayClass *class)
{
        GObjectClass *object_class;
        EExtensionClass *extension_class;

        object_class = G_OBJECT_CLASS (class);
        object_class->constructed = e_systray_constructed;
        object_class->finalize = e_systray_finalize;

        /* Specify the GType of the class we're extending.
         * The class must implement the EExtensible interface. */
        extension_class = E_EXTENSION_CLASS (class);
        extension_class->extensible_type = E_TYPE_SHELL_WINDOW;
}

static void
e_systray_class_finalize (ESystrayClass *class)
{
}


static void
e_systray_init (ESystray *extension)
{
}

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
        e_systray_register_type (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}
